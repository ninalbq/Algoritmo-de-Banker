#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024

typedef struct {
    int n_customers;
    int n_resources;
    int *available;
    int **maximum;
    int **allocation;
    int **need;
} Banker;

void fatal(const char *msg);
int *alloc_vec(int n);
int **alloc_mat(int rows, int cols);
void free_mat(int **m, int rows);
char *trim(char *s);
int parse_nonneg(const char *s, int *val);
void banker_init(Banker *b, int argc, char *argv[]);
void banker_free(Banker *b);
void read_customers(Banker *b, const char *fname);
void process_commands(Banker *b, const char *fname, FILE *out);
int is_safe(const Banker *b);
void request_resources(Banker *b, int cust, const int *req, FILE *out);
void release_resources(Banker *b, int cust, const int *rel, FILE *out);
void print_vec(const Banker *b, const int *v, FILE *out);
void print_state(const Banker *b, FILE *out);

void fatal(const char *msg) {
    fprintf(stderr, "Erro: %s\n", msg);
    exit(EXIT_FAILURE);
}

int *alloc_vec(int n) {
    int *v = calloc(n, sizeof(int));
    if (!v) fatal("falha de alocacao de memoria.");
    return v;
}

int **alloc_mat(int rows, int cols) {
    int i;
    int **m = malloc(rows * sizeof(int *));
    if (!m) fatal("falha de alocacao de memoria.");
    for (i = 0; i < rows; i++) m[i] = alloc_vec(cols);
    return m;
}

void free_mat(int **m, int rows) {
    int i;
    if (!m) return;
    for (i = 0; i < rows; i++) free(m[i]);
    free(m);
}

char *trim(char *s) {
    char *e;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) {
        *e = '\0';
        e--;
    }
    return s;
}

int parse_nonneg(const char *s, int *val) {
    const char *p;
    long v;
    if (!s || !*s) return 0;
    for (p = s; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    v = strtol(s, NULL, 10);
    if (v < 0 || v > 1000000000L) return 0;
    *val = (int)v;
    return 1;
}

void banker_init(Banker *b, int argc, char *argv[]) {
    int j, v;

    if (argc < 2) {
        fprintf(stderr, "Erro: numero de argumentos invalido.\n");
        fprintf(stderr, "Uso: %s <inst_recurso_0> <inst_recurso_1> ...\n", argv[0]);
        fprintf(stderr, "Exemplo: %s 10 5 7\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memset(b, 0, sizeof(Banker));
    b->n_resources = argc - 1;
    b->available = alloc_vec(b->n_resources);

    for (j = 0; j < b->n_resources; j++) {
        if (!parse_nonneg(argv[j + 1], &v)) {
            fprintf(stderr, "Erro: argumento '%s' invalido; use inteiros nao negativos.\n", argv[j + 1]);
            exit(EXIT_FAILURE);
        }
        b->available[j] = v;
    }
}

void banker_free(Banker *b) {
    free(b->available);
    free_mat(b->maximum, b->n_customers);
    free_mat(b->allocation, b->n_customers);
    free_mat(b->need, b->n_customers);
}

static int count_customers(const char *fname) {
    FILE *f;
    char line[MAX_LINE];
    int n = 0;

    f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }
    while (fgets(line, sizeof(line), f)) {
        if (*trim(line) != '\0') n++;
    }
    fclose(f);
    return n;
}

void read_customers(Banker *b, const char *fname) {
    FILE *f;
    char line[MAX_LINE];
    char *t, *tok, *v;
    int c = 0, r, j, val;

    b->n_customers = count_customers(fname);
    if (b->n_customers == 0)
        fatal("o arquivo customer.txt esta vazio.");

    f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }

    b->maximum = alloc_mat(b->n_customers, b->n_resources);
    b->allocation = alloc_mat(b->n_customers, b->n_resources);
    b->need = alloc_mat(b->n_customers, b->n_resources);

    while (fgets(line, sizeof(line), f)) {
        t = trim(line);
        if (*t == '\0') continue;

        r = 0;
        tok = strtok(t, ",");
        while (tok) {
            v = trim(tok);
            if (!parse_nonneg(v, &val)) {
                fprintf(stderr, "Erro: formatacao invalida em customer.txt (linha %d): valor '%s' invalido.\n", c + 1, v);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            if (r >= b->n_resources) {
                fprintf(stderr, "Erro: customer.txt (linha %d) possui mais colunas do que o numero de recursos informado (%d).\n", c + 1, b->n_resources);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            b->maximum[c][r] = val;
            r++;
            tok = strtok(NULL, ",");
        }
        if (r != b->n_resources) {
            fprintf(stderr, "Erro: customer.txt (linha %d) possui %d colunas, mas foram informados %d recursos na linha de comando.\n", c + 1, r, b->n_resources);
            fclose(f);
            exit(EXIT_FAILURE);
        }
        for (j = 0; j < b->n_resources; j++) {
            if (b->maximum[c][j] > b->available[j]) {
                fprintf(stderr, "Erro: customer.txt (linha %d): maximo do recurso %d (%d) excede o total de instancias (%d).\n", c + 1, j, b->maximum[c][j], b->available[j]);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            b->need[c][j] = b->maximum[c][j];
        }
        c++;
    }
    fclose(f);
}

int is_safe(const Banker *b) {
    int *work = alloc_vec(b->n_resources);
    int *finish = alloc_vec(b->n_customers);
    int i, j, ok, progress, safe;

    memcpy(work, b->available, b->n_resources * sizeof(int));

    progress = 1;
    while (progress) {
        progress = 0;
        for (i = 0; i < b->n_customers; i++) {
            if (finish[i]) continue;
            ok = 1;
            for (j = 0; j < b->n_resources; j++) {
                if (b->need[i][j] > work[j]) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                for (j = 0; j < b->n_resources; j++)
                    work[j] += b->allocation[i][j];
                finish[i] = 1;
                progress = 1;
            }
        }
    }

    safe = 1;
    for (i = 0; i < b->n_customers; i++) {
        if (!finish[i]) {
            safe = 0;
            break;
        }
    }

    free(work);
    free(finish);
    return safe;
}

void print_vec(const Banker *b, const int *v, FILE *out) {
    int j;
    for (j = 0; j < b->n_resources; j++)
        fprintf(out, "%d ", v[j]);
}

void print_state(const Banker *b, FILE *out) {
    int i;
    fprintf(out, "MAXIMUM | ALLOCATION | NEED\n");
    for (i = 0; i < b->n_customers; i++) {
        print_vec(b, b->maximum[i], out);
        fprintf(out, "| ");
        print_vec(b, b->allocation[i], out);
        fprintf(out, "| ");
        print_vec(b, b->need[i], out);
        fprintf(out, "\n");
    }
    fprintf(out, "AVAILABLE ");
    print_vec(b, b->available, out);
    fprintf(out, "\n");
}

void request_resources(Banker *b, int cust, const int *req, FILE *out) {
    int j;

    for (j = 0; j < b->n_resources; j++) {
        if (req[j] > b->need[cust][j]) {
            fprintf(out, "The customer %d request ", cust);
            print_vec(b, req, out);
            fprintf(out, "was denied because exceed its maximum allocation\n");
            return;
        }
    }

    for (j = 0; j < b->n_resources; j++) {
        if (req[j] > b->available[j]) {
            fprintf(out, "The resources ");
            print_vec(b, b->available, out);
            fprintf(out, "are not enough to customer %d request ", cust);
            print_vec(b, req, out);
            fprintf(out, "\n");
            return;
        }
    }

    for (j = 0; j < b->n_resources; j++) {
        b->available[j] -= req[j];
        b->allocation[cust][j] += req[j];
        b->need[cust][j] -= req[j];
    }

    if (is_safe(b)) {
        fprintf(out, "Allocate to customer %d the resources ", cust);
        print_vec(b, req, out);
        fprintf(out, "\n");
    } else {
        for (j = 0; j < b->n_resources; j++) {
            b->available[j] += req[j];
            b->allocation[cust][j] -= req[j];
            b->need[cust][j] += req[j];
        }
        fprintf(out, "The customer %d request ", cust);
        print_vec(b, req, out);
        fprintf(out, "was denied because result in an unsafe state\n");
    }
}

void release_resources(Banker *b, int cust, const int *rel, FILE *out) {
    int j;

    for (j = 0; j < b->n_resources; j++) {
        if (rel[j] > b->allocation[cust][j]) {
            fprintf(out, "The customer %d release ", cust);
            print_vec(b, rel, out);
            fprintf(out, "was denied because exceed its maximum allocation\n");
            return;
        }
    }

    for (j = 0; j < b->n_resources; j++) {
        b->available[j] += rel[j];
        b->allocation[cust][j] -= rel[j];
        b->need[cust][j] += rel[j];
    }
    fprintf(out, "Release from customer %d the resources ", cust);
    print_vec(b, rel, out);
    fprintf(out, "\n");
}

void process_commands(Banker *b, const char *fname, FILE *out) {
    FILE *f;
    char line[MAX_LINE];
    char copy[MAX_LINE];
    char *t, *cmd, *tok;
    int lineno = 0, count, val, cust, j;
    int *vec;

    f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        t = trim(line);
        if (*t == '\0') continue;

        strncpy(copy, t, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        cmd = strtok(copy, " \t");
        if (strcmp(cmd, "*") == 0) {
            if (strtok(NULL, " \t")) {
                fprintf(stderr, "Erro: commands.txt (linha %d): o comando '*' nao aceita argumentos.\n", lineno);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            continue;
        }
        if (strcmp(cmd, "RQ") != 0 && strcmp(cmd, "RL") != 0) {
            fprintf(stderr, "Erro: commands.txt (linha %d): comando '%s' desconhecido (use RQ, RL ou *).\n", lineno, cmd);
            fclose(f);
            exit(EXIT_FAILURE);
        }

        count = 0;
        cust = -1;
        while ((tok = strtok(NULL, " \t")) != NULL) {
            if (!parse_nonneg(tok, &val)) {
                fprintf(stderr, "Erro: commands.txt (linha %d): valor '%s' invalido.\n", lineno, tok);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            if (count == 0) cust = val;
            count++;
        }
        if (count != b->n_resources + 1) {
            fprintf(stderr, "Erro: commands.txt (linha %d): esperados %d valores (cliente + %d recursos), encontrados %d. O numero de recursos nao confere com os argumentos da execucao.\n", lineno, b->n_resources + 1, b->n_resources, count);
            fclose(f);
            exit(EXIT_FAILURE);
        }
        if (cust >= b->n_customers) {
            fprintf(stderr, "Erro: commands.txt (linha %d): cliente %d inexistente (0 a %d).\n", lineno, cust, b->n_customers - 1);
            fclose(f);
            exit(EXIT_FAILURE);
        }
    }

    rewind(f);
    vec = alloc_vec(b->n_resources);

    while (fgets(line, sizeof(line), f)) {
        t = trim(line);
        if (*t == '\0') continue;

        cmd = strtok(t, " \t");
        if (strcmp(cmd, "*") == 0) {
            print_state(b, out);
            continue;
        }
        cust = atoi(strtok(NULL, " \t"));
        for (j = 0; j < b->n_resources; j++)
            vec[j] = atoi(strtok(NULL, " \t"));

        if (strcmp(cmd, "RQ") == 0)
            request_resources(b, cust, vec, out);
        else
            release_resources(b, cust, vec, out);
    }

    free(vec);
    fclose(f);
}

int main(int argc, char *argv[]) {
    Banker b;
    FILE *out;

    banker_init(&b, argc, argv);
    read_customers(&b, "customer.txt");

    out = fopen("result.txt", "w");
    if (!out) fatal("nao foi possivel criar o arquivo result.txt.");

    process_commands(&b, "commands.txt", out);

    fclose(out);
    banker_free(&b);
    return EXIT_SUCCESS;
}

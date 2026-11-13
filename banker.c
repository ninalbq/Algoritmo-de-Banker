#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024

static int NUMBER_OF_RESOURCES = 0;
static int NUMBER_OF_CUSTOMERS = 0;

static int *available;      /* [R]    */
static int **maximum;       /* [C][R] */
static int **allocation;    /* [C][R] */
static int **need;          /* [C][R] */

static FILE *out;

/* ---------- utilitários ---------- */

static void fatal(const char *msg) {
    fprintf(stderr, "Erro: %s\n", msg);
    exit(EXIT_FAILURE);
}

static int *alloc_vec(int n) {
    int *v = calloc(n, sizeof(int));
    if (!v) fatal("falha de alocacao de memoria.");
    return v;
}

static int **alloc_mat(int r, int c) {
    int **m = malloc(r * sizeof(int *));
    if (!m) fatal("falha de alocacao de memoria.");
    for (int i = 0; i < r; i++) m[i] = alloc_vec(c);
    return m;
}

static void free_mat(int **m, int r) {
    if (!m) return;
    for (int i = 0; i < r; i++) free(m[i]);
    free(m);
}

/* remove espacos/CR/LF nas pontas; retorna ponteiro dentro de s */
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

/* converte string para inteiro nao negativo estrito */
static int parse_nonneg(const char *s, int *val) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    long v = strtol(s, NULL, 10);
    if (v < 0 || v > 1000000000L) return 0;
    *val = (int)v;
    return 1;
}

/* ---------- leitura de customer.txt ---------- */

/* conta linhas nao vazias */
static int count_customers(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }
    char line[MAX_LINE];
    int n = 0;
    while (fgets(line, sizeof line, f)) {
        if (*trim(line) != '\0') n++;
    }
    fclose(f);
    return n;
}

static void read_customers(const char *fname) {
    NUMBER_OF_CUSTOMERS = count_customers(fname);
    if (NUMBER_OF_CUSTOMERS == 0)
        fatal("o arquivo customer.txt esta vazio.");

    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }

    maximum    = alloc_mat(NUMBER_OF_CUSTOMERS, NUMBER_OF_RESOURCES);
    allocation = alloc_mat(NUMBER_OF_CUSTOMERS, NUMBER_OF_RESOURCES);
    need       = alloc_mat(NUMBER_OF_CUSTOMERS, NUMBER_OF_RESOURCES);

    char line[MAX_LINE];
    int c = 0;
    while (fgets(line, sizeof line, f)) {
        char *t = trim(line);
        if (*t == '\0') continue;

        int r = 0;
        char *tok = strtok(t, ",");
        while (tok) {
            char *v = trim(tok);
            int val;
            if (!parse_nonneg(v, &val)) {
                fprintf(stderr,
                    "Erro: formatacao invalida em customer.txt (linha %d): "
                    "valor '%s' nao e um inteiro nao negativo.\n", c + 1, v);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            if (r >= NUMBER_OF_RESOURCES) {
                fprintf(stderr,
                    "Erro: customer.txt (linha %d) possui mais colunas do que "
                    "o numero de recursos informado (%d).\n",
                    c + 1, NUMBER_OF_RESOURCES);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            maximum[c][r] = val;
            r++;
            tok = strtok(NULL, ",");
        }
        if (r != NUMBER_OF_RESOURCES) {
            fprintf(stderr,
                "Erro: customer.txt (linha %d) possui %d colunas, mas foram "
                "informados %d recursos na linha de comando.\n",
                c + 1, r, NUMBER_OF_RESOURCES);
            fclose(f);
            exit(EXIT_FAILURE);
        }
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
            if (maximum[c][j] > available[j]) {
                fprintf(stderr,
                    "Erro: customer.txt (linha %d): maximo do recurso %d (%d) "
                    "excede o total de instancias (%d).\n",
                    c + 1, j, maximum[c][j], available[j]);
                fclose(f);
                exit(EXIT_FAILURE);
            }
            need[c][j] = maximum[c][j];
        }
        c++;
    }
    fclose(f);
}

/* ---------- algoritmo de seguranca ---------- */

static int is_safe(void) {
    int *work = alloc_vec(NUMBER_OF_RESOURCES);
    int *finish = alloc_vec(NUMBER_OF_CUSTOMERS);
    memcpy(work, available, NUMBER_OF_RESOURCES * sizeof(int));

    int progress = 1;
    while (progress) {
        progress = 0;
        for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
            if (finish[i]) continue;
            int ok = 1;
            for (int j = 0; j < NUMBER_OF_RESOURCES; j++)
                if (need[i][j] > work[j]) { ok = 0; break; }
            if (ok) {
                for (int j = 0; j < NUMBER_OF_RESOURCES; j++)
                    work[j] += allocation[i][j];
                finish[i] = 1;
                progress = 1;
            }
        }
    }
    int safe = 1;
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++)
        if (!finish[i]) { safe = 0; break; }

    free(work);
    free(finish);
    return safe;
}

/* ---------- impressao ---------- */

static void print_vec(const int *v) {
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++)
        fprintf(out, "%d ", v[j]);
}

static void print_state(void) {
    fprintf(out, "MAXIMUM | ALLOCATION | NEED\n");
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; i++) {
        print_vec(maximum[i]);    fprintf(out, "| ");
        print_vec(allocation[i]); fprintf(out, "| ");
        print_vec(need[i]);
        fprintf(out, "\n");
    }
    fprintf(out, "AVAILABLE ");
    print_vec(available);
    fprintf(out, "\n");
}

/* ---------- request / release ---------- */

static void request_resources(int cust, const int *req) {
    /* 1) excede a necessidade (maximo) */
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        if (req[j] > need[cust][j]) {
            fprintf(out, "The customer %d request ", cust);
            print_vec(req);
            fprintf(out, "was denied because exceed its maximum allocation\n");
            return;
        }
    }
    /* 2) recursos insuficientes */
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        if (req[j] > available[j]) {
            fprintf(out, "The resources ");
            print_vec(available);
            fprintf(out, "are not enough to customer %d request ", cust);
            print_vec(req);
            fprintf(out, "\n");
            return;
        }
    }
    /* 3) alocacao hipotetica */
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        available[j]        -= req[j];
        allocation[cust][j] += req[j];
        need[cust][j]       -= req[j];
    }
    if (is_safe()) {
        fprintf(out, "Allocate to customer %d the resources ", cust);
        print_vec(req);
        fprintf(out, "\n");
    } else {
        /* rollback */
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
            available[j]        += req[j];
            allocation[cust][j] -= req[j];
            need[cust][j]       += req[j];
        }
        fprintf(out, "The customer %d request ", cust);
        print_vec(req);
        fprintf(out, "was denied because result in an unsafe state\n");
    }
}

static void release_resources(int cust, const int *rel) {
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        if (rel[j] > allocation[cust][j]) {
            fprintf(out, "The customer %d release ", cust);
            print_vec(rel);
            fprintf(out, "was denied because exceed its maximum allocation\n");
            return;
        }
    }
    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        available[j]        += rel[j];
        allocation[cust][j] -= rel[j];
        need[cust][j]       += rel[j];
    }
    fprintf(out, "Release from customer %d the resources ", cust);
    print_vec(rel);
    fprintf(out, "\n");
}

/* ---------- leitura de commands.txt ---------- */

static void process_commands(const char *fname) {
    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", fname);
        exit(EXIT_FAILURE);
    }

    /* primeira passada: valida TODO o arquivo antes de executar */
    char line[MAX_LINE];
    int lineno = 0;
    while (fgets(line, sizeof line, f)) {
        lineno++;
        char *t = trim(line);
        if (*t == '\0') continue;

        char copy[MAX_LINE];
        strncpy(copy, t, sizeof copy - 1);
        copy[sizeof copy - 1] = '\0';

        char *cmd = strtok(copy, " \t");
        if (strcmp(cmd, "*") == 0) {
            if (strtok(NULL, " \t")) {
                fprintf(stderr, "Erro: commands.txt (linha %d): o comando '*' "
                                "nao aceita argumentos.\n", lineno);
                fclose(f); exit(EXIT_FAILURE);
            }
            continue;
        }
        if (strcmp(cmd, "RQ") != 0 && strcmp(cmd, "RL") != 0) {
            fprintf(stderr, "Erro: commands.txt (linha %d): comando '%s' "
                            "desconhecido (use RQ, RL ou *).\n", lineno, cmd);
            fclose(f); exit(EXIT_FAILURE);
        }
        int count = 0, val;
        char *tok;
        int cust = -1;
        while ((tok = strtok(NULL, " \t")) != NULL) {
            if (!parse_nonneg(tok, &val)) {
                fprintf(stderr, "Erro: commands.txt (linha %d): valor '%s' "
                                "invalido.\n", lineno, tok);
                fclose(f); exit(EXIT_FAILURE);
            }
            if (count == 0) cust = val;
            count++;
        }
        if (count != NUMBER_OF_RESOURCES + 1) {
            fprintf(stderr, "Erro: commands.txt (linha %d): esperados %d "
                            "valores (cliente + %d recursos), encontrados %d. "
                            "O numero de recursos nao confere com os "
                            "argumentos da execucao.\n",
                    lineno, NUMBER_OF_RESOURCES + 1, NUMBER_OF_RESOURCES, count);
            fclose(f); exit(EXIT_FAILURE);
        }
        if (cust >= NUMBER_OF_CUSTOMERS) {
            fprintf(stderr, "Erro: commands.txt (linha %d): cliente %d "
                            "inexistente (0 a %d).\n",
                    lineno, cust, NUMBER_OF_CUSTOMERS - 1);
            fclose(f); exit(EXIT_FAILURE);
        }
    }

    /* segunda passada: executa */
    rewind(f);
    int *vec = alloc_vec(NUMBER_OF_RESOURCES);
    while (fgets(line, sizeof line, f)) {
        char *t = trim(line);
        if (*t == '\0') continue;

        char *cmd = strtok(t, " \t");
        if (strcmp(cmd, "*") == 0) {
            print_state();
            continue;
        }
        int cust = atoi(strtok(NULL, " \t"));
        for (int j = 0; j < NUMBER_OF_RESOURCES; j++)
            vec[j] = atoi(strtok(NULL, " \t"));

        if (strcmp(cmd, "RQ") == 0) request_resources(cust, vec);
        else                        release_resources(cust, vec);
    }
    free(vec);
    fclose(f);
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Erro: numero de argumentos invalido.\n"
                        "Uso: %s <inst_recurso_0> <inst_recurso_1> ...\n"
                        "Exemplo: %s 10 5 7\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    NUMBER_OF_RESOURCES = argc - 1;
    available = alloc_vec(NUMBER_OF_RESOURCES);

    for (int j = 0; j < NUMBER_OF_RESOURCES; j++) {
        int v;
        if (!parse_nonneg(argv[j + 1], &v)) {
            fprintf(stderr, "Erro: argumento '%s' invalido; use inteiros "
                            "nao negativos.\n", argv[j + 1]);
            return EXIT_FAILURE;
        }
        available[j] = v;
    }

    read_customers("customer.txt");

    out = fopen("result.txt", "w");
    if (!out) fatal("nao foi possivel criar o arquivo result.txt.");

    process_commands("commands.txt");

    fclose(out);
    free(available);
    free_mat(maximum, NUMBER_OF_CUSTOMERS);
    free_mat(allocation, NUMBER_OF_CUSTOMERS);
    free_mat(need, NUMBER_OF_CUSTOMERS);
    return EXIT_SUCCESS;
}

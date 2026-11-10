#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "banker.h"

void fail_with_error(const char *msg) {
    fprintf(stderr, "Erro: %s\n", msg);
    exit(EXIT_FAILURE);
}

BankerSystem* init_system(int num_customers, int num_resources, int *initial_resources) {
    BankerSystem *sys = malloc(sizeof(BankerSystem));
    sys->num_customers = num_customers;
    sys->num_resources = num_resources;
    
    sys->available = malloc(num_resources * sizeof(int));
    for (int i = 0; i < num_resources; i++) {
        sys->available[i] = initial_resources[i];
    }
    
    sys->maximum = malloc(num_customers * sizeof(int*));
    sys->allocation = malloc(num_customers * sizeof(int*));
    sys->need = malloc(num_customers * sizeof(int*));
    
    for (int i = 0; i < num_customers; i++) {
        sys->maximum[i] = calloc(num_resources, sizeof(int));
        sys->allocation[i] = calloc(num_resources, sizeof(int));
        sys->need[i] = calloc(num_resources, sizeof(int));
    }
    
    return sys;
}

void free_system(BankerSystem *sys) {
    if (!sys) return;
    for (int i = 0; i < sys->num_customers; i++) {
        free(sys->maximum[i]);
        free(sys->allocation[i]);
        free(sys->need[i]);
    }
    free(sys->maximum);
    free(sys->allocation);
    free(sys->need);
    free(sys->available);
    free(sys);
}

int count_customers(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) fail_with_error("Nao foi possivel abrir o arquivo customer.txt");
    
    int count = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f)) {
        if (strlen(buffer) > 1) count++;
    }
    fclose(f);
    return count;
}

bool read_customer_file(const char *filename, BankerSystem *sys) {
    FILE *f = fopen(filename, "r");
    if (!f) fail_with_error("Nao foi possivel abrir o arquivo customer.txt");
    
    for (int i = 0; i < sys->num_customers; i++) {
        for (int j = 0; j < sys->num_resources; j++) {
            if (fscanf(f, "%d", &sys->maximum[i][j]) != 1) {
                fclose(f);
                fail_with_error("Formatacao invalida no arquivo customer.txt");
            }
            sys->need[i][j] = sys->maximum[i][j];
            
            int c = fgetc(f);
            if (c != ',' && c != '\n' && c != EOF && c != ' ' && c != '\r') {
                ungetc(c, f);
            }
        }
    }
    fclose(f);
    return true;
}

bool is_safe_state(BankerSystem *sys) {
    int *work = malloc(sys->num_resources * sizeof(int));
    bool *finish = calloc(sys->num_customers, sizeof(bool));
    
    for (int i = 0; i < sys->num_resources; i++) {
        work[i] = sys->available[i];
    }
    
    int count = 0;
    while (count < sys->num_customers) {
        bool found = false;
        for (int i = 0; i < sys->num_customers; i++) {
            if (!finish[i]) {
                bool can_allocate = true;
                for (int j = 0; j < sys->num_resources; j++) {
                    if (sys->need[i][j] > work[j]) {
                        can_allocate = false;
                        break;
                    }
                }
                
                if (can_allocate) {
                    for (int j = 0; j < sys->num_resources; j++) {
                        work[j] += sys->allocation[i][j];
                    }
                    finish[i] = true;
                    found = true;
                    count++;
                }
            }
        }
        if (!found) break; 
    }
    
    bool safe = (count == sys->num_customers);
    free(work);
    free(finish);
    return safe;
}

bool request_resources(int customer_num, int *request, BankerSystem *sys, FILE *output_file) {
    for (int j = 0; j < sys->num_resources; j++) {
        if (request[j] > sys->need[customer_num][j]) {
            fprintf(output_file, "The customer %d request ", customer_num);
            for (int r = 0; r < sys->num_resources; r++) {
                fprintf(output_file, "%d", request[r]);
                if (r < sys->num_resources - 1) fprintf(output_file, " ");
            }
            fprintf(output_file, " was denied because exceed its maximum allocation\n");
            return false;
        }
    }
    
    for (int j = 0; j < sys->num_resources; j++) {
        if (request[j] > sys->available[j]) {
            fprintf(output_file, "The resources ");
            for (int r = 0; r < sys->num_resources; r++) {
                fprintf(output_file, "%d", sys->available[r]);
                if (r < sys->num_resources - 1) fprintf(output_file, " ");
            }
            fprintf(output_file, " are not enough to customer %d request ", customer_num);
            for (int r = 0; r < sys->num_resources; r++) {
                fprintf(output_file, "%d", request[r]);
                if (r < sys->num_resources - 1) fprintf(output_file, " ");
            }
            fprintf(output_file, "\n");
            return false;
        }
    }
    
    for (int j = 0; j < sys->num_resources; j++) {
        sys->available[j] -= request[j];
        sys->allocation[customer_num][j] += request[j];
        sys->need[customer_num][j] -= request[j];
    }
    
    if (is_safe_state(sys)) {
        fprintf(output_file, "Allocate to customer %d the resources ", customer_num);
        for (int r = 0; r < sys->num_resources; r++) {
            fprintf(output_file, "%d", request[r]);
            if (r < sys->num_resources - 1) fprintf(output_file, " ");
        }
        fprintf(output_file, "\n");
        return true;
    } else {
        for (int j = 0; j < sys->num_resources; j++) {
            sys->available[j] += request[j];
            sys->allocation[customer_num][j] -= request[j];
            sys->need[customer_num][j] += request[j];
        }
        fprintf(output_file, "The customer %d request ", customer_num);
        for (int r = 0; r < sys->num_resources; r++) {
            fprintf(output_file, "%d", request[r]);
            if (r < sys->num_resources - 1) fprintf(output_file, " ");
        }
        fprintf(output_file, " was denied because result in an unsafe state\n");
        return false;
    }
}

void release_resources(int customer_num, int *release, BankerSystem *sys, FILE *output_file) {
    for (int j = 0; j < sys->num_resources; j++) {
        sys->allocation[customer_num][j] -= release[j];
        sys->available[j] += release[j];
        sys->need[customer_num][j] += release[j];
    }
    fprintf(output_file, "Release from customer %d the resources ", customer_num);
    for (int r = 0; r < sys->num_resources; r++) {
        fprintf(output_file, "%d", release[r]);
        if (r < sys->num_resources - 1) fprintf(output_file, " ");
    }
    fprintf(output_file, "\n");
}

void print_system_state(BankerSystem *sys, FILE *output_file) {
    fprintf(output_file, "MAXIMUM | ALLOCATION | NEED\n");
    for (int i = 0; i < sys->num_customers; i++) {
        for (int j = 0; j < sys->num_resources; j++) {
            fprintf(output_file, "%d", sys->maximum[i][j]);
            if (j < sys->num_resources - 1) fprintf(output_file, " ");
        }
        fprintf(output_file, " ");
        for (int j = 0; j < sys->num_resources; j++) {
            fprintf(output_file, "%d", sys->allocation[i][j]);
            if (j < sys->num_resources - 1) fprintf(output_file, " ");
        }
        fprintf(output_file, " ");
        for (int j = 0; j < sys->num_resources; j++) {
            fprintf(output_file, "%d", sys->need[i][j]);
            if (j < sys->num_resources - 1) fprintf(output_file, " ");
        }
        fprintf(output_file, "\n");
    }
    fprintf(output_file, "AVAILABLE ");
    for (int j = 0; j < sys->num_resources; j++) {
        fprintf(output_file, "%d", sys->available[j]);
        if (j < sys->num_resources - 1) fprintf(output_file, " ");
    }
    fprintf(output_file, "\n");
}

void process_commands_file(const char *filename, const char *output_filename, BankerSystem *sys) {
    FILE *f = fopen(filename, "r");
    if (!f) fail_with_error("Nao foi possivel abrir o arquivo commands.txt");
    
    FILE *out = fopen(output_filename, "w");
    if (!out) {
        fclose(f);
        fail_with_error("Nao foi possivel criar result.txt");
    }
    
    char cmd[10];
    int customer_num;
    int *req_rel = malloc(sys->num_resources * sizeof(int));
    
    while (fscanf(f, "%s", cmd) == 1) {
        if (strcmp(cmd, "*") == 0) {
            print_system_state(sys, out);
        } else if (strcmp(cmd, "RQ") == 0 || strcmp(cmd, "RL") == 0) {
            if (fscanf(f, "%d", &customer_num) != 1) fail_with_error("Erro de formato em commands.txt");
            for (int i = 0; i < sys->num_resources; i++) {
                if (fscanf(f, "%d", &req_rel[i]) != 1) fail_with_error("Erro de formato em commands.txt");
            }
            if (strcmp(cmd, "RQ") == 0) {
                request_resources(customer_num, req_rel, sys, out);
            } else {
                release_resources(customer_num, req_rel, sys, out);
            }
        }
    }
    
    free(req_rel);
    fclose(f);
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fail_with_error("Numero de argumentos invalido. Use: ./banker <res1> <res2> ...");
    }
    
    int num_resources = argc - 1;
    int *initial_resources = malloc(num_resources * sizeof(int));
    for (int i = 0; i < num_resources; i++) {
        initial_resources[i] = atoi(argv[i+1]);
    }
    
    int num_customers = count_customers("customer.txt");
    
    BankerSystem *sys = init_system(num_customers, num_resources, initial_resources);
    read_customer_file("customer.txt", sys);
    process_commands_file("commands.txt", "result.txt", sys);
    
    free_system(sys);
    free(initial_resources);
    
    return 0;
}
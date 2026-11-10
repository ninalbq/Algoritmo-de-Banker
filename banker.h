#ifndef BANKER_H
#define BANKER_H

#include <stdbool.h>
#include <stdio.h>


#define MAX_RESOURCES 100 

typedef struct {  
    
    int num_customers;
    int num_resources;
    int *available;         
    int **maximum;          
    int **allocation;       
    int **need;             
} BankerSystem;

BankerSystem* init_system(int num_customers, int num_resources, int *initial_resources);
void free_system(BankerSystem *sys);

bool read_customer_file(const char *filename, BankerSystem *sys);
void process_commands_file(const char *filename, const char *output_filename, BankerSystem *sys);

bool request_resources(int customer_num, int *request, BankerSystem *sys, FILE *output_file);
void release_resources(int customer_num, int *release, BankerSystem *sys, FILE *output_file);
bool is_safe_state(BankerSystem *sys);
void print_system_state(BankerSystem *sys, FILE *output_file);

#endif // BANKER_H
#ifndef DEFS_H
#define DEFS_H

#include <pthread.h>
#include <semaphore.h>

#define PRIORIDADE_BASE 1000
#define TEMPO_BASE 500000
#define TIMEOUT 5

/* Forward Declaration (Resolve dependência circular) */
typedef struct Setor Setor;
typedef struct Aviao Aviao;

typedef struct Fila {
    Aviao** contents;
    int size;
    int index;
    int max_size;
} Fila;

struct Aviao {
    pthread_t thread;
    unsigned int id;
    unsigned int prioridade; 
    Setor** rota;
    unsigned int rota_size;
    unsigned int setor_atual;
    int refazer_pedido;
    double tempo_espera;
};

struct Setor {
    unsigned int numero_setor;
    Aviao* aviao_atual;
    Fila fila;
};

typedef struct arg_central {
    Setor* lista_setores;
    int n_setores;
} arg_central;

/* Protótipo global: permite que fila.c e controle.c usem a mesma função */
double tempo_atual(); 

#endif
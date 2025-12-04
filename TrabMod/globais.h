#ifndef GLOBAIS_H
#define GLOBAIS_H

#include "defs.h"

// "extern" significa que a variável está definida no .c
extern int avioes_ativos;
extern sem_t espera_aviao;
extern sem_t espera_central;
extern sem_t *aguardando_fila;
extern pthread_mutex_t mutex_pedido;
extern pthread_mutex_t modificando_filas;
extern pthread_mutex_t mutex_avioes_ativos;
extern Aviao* pedido_aviao;

#endif
#include "globais.h"
#include <stdlib.h> // Para NULL

int avioes_ativos;
sem_t espera_aviao;
sem_t espera_central;
sem_t *aguardando_fila;
pthread_mutex_t mutex_pedido;
pthread_mutex_t modificando_filas;
pthread_mutex_t mutex_avioes_ativos;
Aviao* pedido_aviao = NULL;
#ifndef FILA_H
#define FILA_H

#include "defs.h"

void inserir_por_prioridade(Fila *fila, Aviao* novo_aviao);
void inserir_no_fim(Fila *fila, Aviao* aviao);
void remover_da_fila(Fila *fila, Aviao* aviao);

#endif
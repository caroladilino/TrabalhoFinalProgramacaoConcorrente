#include <stdio.h>
#include <stdlib.h>
#include "fila.h"

void inserir_por_prioridade(Fila *fila, Aviao* novo_aviao) {
    /* Implementação LINEAR: fila sempre começa do índice 0.
       Se não há espaço no final mas há espaço total (elementos removidos),
       compacta a fila movendo todos os elementos para o início.
       Depois insere mantendo ordem por prioridade (maior primeiro). */
    
    if (fila->size >= fila->max_size) {
        printf("[WARN] fila cheia ao inserir aviao %u\n", novo_aviao->id);
        fflush(stdout);
        return;
    }

    /* COMPACTAÇÃO: Se há espaço no vetor mas não no final (alguns foram removidos),
       mover tudo para o início */
    if (fila->index > 0) {
        for (int i = 0; i < fila->size; i++) {
            fila->contents[i] = fila->contents[fila->index + i];
        }
        fila->index = 0;
    }

    /* Encontrar posição de inserção mantendo ordem por prioridade (maior primeiro) */
    int pos = 0;
    while (pos < fila->size && novo_aviao->prioridade <= fila->contents[pos]->prioridade) {
        pos++;
    }

    /* Mover elementos à direita para abrir espaço */
    for (int i = fila->size; i > pos; i--) {
        fila->contents[i] = fila->contents[i - 1];
    }

    /* Inserir novo avião */
    novo_aviao->tempo_espera = tempo_atual();
    fila->contents[pos] = novo_aviao;
    fila->size++;
}

void inserir_no_fim(Fila *fila, Aviao* aviao) {
    if (fila->size == fila->max_size) return;

    aviao->tempo_espera = tempo_atual();
    fila->contents[fila->size] = aviao;
    fila->size++;
}

void remover_da_fila(Fila *fila, Aviao* aviao) {
    if (fila->size == 0) return;

    /* Buscar posição do avião na fila */
    int pos = -1;
    for (int i = 0; i < fila->size; i++) {
        if (fila->contents[i] != NULL && fila->contents[i]->id == aviao->id) {
            pos = i;
            break;
        }
    }

    if (pos == -1) return; /* Não encontrou */

    /* Remover deslocando para trás */
    for (int j = pos; j < fila->size - 1; j++) {
        fila->contents[j] = fila->contents[j + 1];
    }

    fila->contents[fila->size - 1] = NULL;
    fila->size--;
}

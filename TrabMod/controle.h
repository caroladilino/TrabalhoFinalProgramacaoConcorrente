#ifndef CONTROLE_H
#define CONTROLE_H

#include "defs.h"

void inicializar_sistema(int n_setores, int n_avioes, Setor* setores, Aviao* avioes);
// Correção: Removido parâmetro 'central' não utilizado
void finalizar_sistema(Setor* setores, Aviao* avioes, int n_setores, int n_avioes);
void * aviao_executa(void * arg);
void * central_executa(void * arg);

#endif
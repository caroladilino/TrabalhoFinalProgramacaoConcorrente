#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "controle.h"
#include "globais.h"
#include "fila.h"

// Implementação única do tempo_atual

double tempo_atual() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

void acordar_proximo(Setor *s) {
    Fila *fila = &s->fila;
    if (fila->size == 0) return;

    Aviao *prox = fila->contents[0];

    for (int i = 0; i < fila->size - 1; i++)
        fila->contents[i] = fila->contents[i + 1];

    fila->contents[fila->size - 1] = NULL;
    fila->size--;
    fila->index = 0;

    sem_post(&aguardando_fila[prox->id]);
}


void timeout_check(Setor* lista_setores, int n_setores) {
    for (int i = 0; i < n_setores; i++) {
        Fila* fila = &lista_setores[i].fila;
        for (int j = 0; j < fila->size; j++) {
            Aviao *a = fila->contents[j];
            if (a == NULL) continue;
            
            double espera = (double) (tempo_atual() - a->tempo_espera);

            if (espera > TIMEOUT) {
                printf("[AVIAO %d] AVIÃO FICOU MUITO TEMPO NA FILA DE ESPERA. EXECUTANDO MANOBRA DE EMERGÊNCIA\n", a->id);
                fflush(stdout);
                
                /* Remover do seu setor atual se estiver lá */
                Setor* setor_atual = a->rota[a->setor_atual];
                if (setor_atual->aviao_atual == a) {
                    setor_atual->aviao_atual = NULL;
                }
                
                /* Remover da fila */
                remover_da_fila(fila, a);
                
                /* Despertar para refazer pedido */
                a->refazer_pedido = 1;
                sem_post(&aguardando_fila[a->id]);
            }
        }
    }
}

int deadlock_check(Aviao* aviao_inicial) { 
    Aviao* temp = aviao_inicial;
    int iteracoes = 0;
    int max_iteracoes = 500;
    
    do { 
        iteracoes++;
        if (iteracoes > max_iteracoes) return 0; /* Proteção contra loops infinitos */
        
        if (temp == NULL) return 0;
        if (temp->setor_atual + 1 >= temp->rota_size) return 0; /* Proteção: índice válido */

        Setor* proximo_setor = temp->rota[temp->setor_atual+1];
        
        /* Verificar se está na fila e é o primeiro */
        if (proximo_setor->fila.size == 0 || proximo_setor->fila.contents == NULL || 
            proximo_setor->fila.contents[0] == NULL || proximo_setor->fila.contents[0] != temp)
            return 0;

        temp = proximo_setor->aviao_atual;
        if (temp == NULL) return 0;
    } while (temp != aviao_inicial);

    return 1; /* Ciclo encontrado = DEADLOCK */ 
}



void inicializar_sistema(int n_setores, int n_avioes, Setor* setores, Aviao* avioes){
    avioes_ativos = n_avioes;

    aguardando_fila = malloc(n_avioes * sizeof(sem_t));
    sem_init(&espera_aviao, 0, 0);
    sem_init(&espera_central, 0, 0);
    for (int i = 0; i < n_avioes; i++)
        sem_init(&aguardando_fila[i], 0, 0);
    pthread_mutex_init(&mutex_pedido, NULL);
    pthread_mutex_init(&modificando_filas, NULL);
    pthread_mutex_init(&mutex_avioes_ativos, NULL);

    for (int i = 0; i < n_setores; i++) {
        setores[i].numero_setor = i;
        setores[i].aviao_atual = NULL;
        setores[i].fila.contents = malloc(n_avioes * sizeof(Aviao*));
        setores[i].fila.size = 0;
        setores[i].fila.index = 0;
        setores[i].fila.max_size = n_avioes;
    }

    for (int i = 0; i < n_avioes; i++) {
        avioes[i].id = i;
        avioes[i].prioridade = (rand() % PRIORIDADE_BASE) + 1;
        avioes[i].refazer_pedido = 0;

        int max_rota = (n_setores > 10) ? 10 : n_setores;
        int rota_size = (rand() % max_rota) + 2;

        avioes[i].rota_size = rota_size;
        avioes[i].setor_atual = 0;
        avioes[i].rota = malloc(rota_size * sizeof(Setor*));

        /* Distribui o inicio da rota e preenche com ponteiros para setores */
        avioes[i].rota[0] = &setores[i];
        for (int j = 1; j < rota_size; j++) {
            unsigned int  novo_setor = rand() % n_setores;
            if (novo_setor == avioes[i].rota[j-1]->numero_setor)
                novo_setor = (novo_setor + 1) % n_setores;
            avioes[i].rota[j] = &setores[novo_setor];
        }

        /* coloca o avião no setor inicial */
        avioes[i].rota[0]->aviao_atual = &avioes[i];

        printf("[AVIÃO %d] Criado! Rota: ", avioes[i].id);
        for (int j = 0; j < rota_size; j++) {
            printf("S%d", avioes[i].rota[j]->numero_setor);
            if (j < rota_size - 1) printf(" -> ");
        }
        printf(" Prioridade: %d\n", avioes[i].prioridade);
        fflush(stdout);
    }
}

void * aviao_executa(void * arg) {
    Aviao *aviao = (Aviao *) arg;
    
    while(aviao->setor_atual < aviao->rota_size - 1) {
        printf("[AVIAO %d] ESPERANDO PARA FALAR COM CENTRAL\n", aviao->id);
        fflush(stdout);
        pthread_mutex_lock(&mutex_pedido);
        printf("[AVIAO %d] ESTÁ FALANDO COM CENTRAL\n", aviao->id);
        fflush(stdout);
        pedido_aviao = aviao;
        sem_post(&espera_central); // Acorda central
        sem_wait(&espera_aviao);   // Espera ACK da central
        pthread_mutex_unlock(&mutex_pedido);
        
        sem_wait(&aguardando_fila[aviao->id]); // Espera autorização de pouso/mudança
        if (!aviao->refazer_pedido){
            pthread_mutex_lock(&modificando_filas);
            Setor *proximo_setor = aviao->rota[aviao->setor_atual+1];
            Setor *setor_antigo = aviao->rota[aviao->setor_atual];
            proximo_setor->aviao_atual = aviao;
            aviao->setor_atual++;
            setor_antigo->aviao_atual = NULL;
            acordar_proximo(setor_antigo);
            pthread_mutex_unlock(&modificando_filas);
            
            Setor* setor_atual = aviao->rota[aviao->setor_atual];
            printf("[AVIAO %d] VOANDO PARA SETOR %d\n", aviao->id, setor_atual->numero_setor);
            fflush(stdout);
            usleep(TEMPO_BASE + rand() % 4000); // Simula tempo de voo [2 segundos, 6 segundos]
            printf("[AVIAO %d] CHEGOU NO SETOR %d\n", aviao->id, setor_atual->numero_setor);
            fflush(stdout);
        } else {
            usleep(5000);
            aviao->refazer_pedido = 0;
        }
        
    }
    printf("[AVIAO %d] CHEGOU AO DESTINO FINAL!\n", aviao->id);
    fflush(stdout);
    /* Liberando setor final */
    pthread_mutex_lock(&modificando_filas);
    Setor* setor_final = aviao->rota[aviao->setor_atual];
    setor_final->aviao_atual = NULL; /* Liberar o setor para outros aviões */
    acordar_proximo(setor_final);
    printf("[AVIAO %d] LIBEROU O DESTINO FINAL!\n", aviao->id);
    fflush(stdout);
    pthread_mutex_unlock(&modificando_filas);

    pthread_mutex_lock(&mutex_avioes_ativos);
    avioes_ativos--;
    pthread_mutex_unlock(&mutex_avioes_ativos);

    return NULL;
}

void * central_executa(void * arg) {
    arg_central *arguments = (arg_central *) arg;
    while (1) {

        pthread_mutex_lock(&mutex_avioes_ativos);
        if (avioes_ativos == 0) {
            pthread_mutex_unlock(&mutex_avioes_ativos);
            break; 
        }
        pthread_mutex_unlock(&mutex_avioes_ativos);

        pthread_mutex_lock(&modificando_filas);
        timeout_check(arguments->lista_setores, arguments->n_setores);
        pthread_mutex_unlock(&modificando_filas);


        if (!sem_trywait(&espera_central)) {
            Aviao* aviao_atual = pedido_aviao;
            Setor* proximo_setor = aviao_atual->rota[aviao_atual->setor_atual+1];

            /* IMPORTANTE: manter lock curto - só para verificação */
            pthread_mutex_lock(&modificando_filas);
            
            if (proximo_setor->aviao_atual == NULL) {
                /* Setor livre - liberar avião para entrar */
                pthread_mutex_unlock(&modificando_filas);
                sem_post(&aguardando_fila[aviao_atual->id]);
            } else {
                /* Setor ocupado - colocar na fila */
                Fila* fila_proximo_setor = &proximo_setor->fila;
                inserir_por_prioridade(fila_proximo_setor, aviao_atual);
                printf("[AVIAO %d] ENTROU NA FILA DO SETOR %d\n", aviao_atual->id, proximo_setor->numero_setor);
                fflush(stdout);
                if (deadlock_check(aviao_atual)) {
                    // Libera setores do avião atual
                    printf("[AVIAO %d] EXECUTANDO MANOBRA DE EMERGÊNCIA!\n", aviao_atual->id);
                    fflush(stdout);
                    Setor* setor_atual = aviao_atual->rota[aviao_atual->setor_atual];
                    setor_atual->aviao_atual = NULL;
                    acordar_proximo(setor_atual);
                }
                pthread_mutex_unlock(&modificando_filas);
            }
            
            sem_post(&espera_aviao);
        }

        usleep(20000);
    }

    return NULL;
}

// Correção: Removemos o pthread_t central que não era usado
void finalizar_sistema(Setor* setores, Aviao* avioes, int n_setores, int n_avioes){
    /* Cancela e espera a thread da central */

    /* Destrói semáforos */
    sem_destroy(&espera_aviao);
    sem_destroy(&espera_central);
    for (int i = 0; i < n_avioes; i++) {
        sem_destroy(&aguardando_fila[i]);
    }

    free(aguardando_fila);

    /* Destrói mutex */
    pthread_mutex_destroy(&mutex_pedido);
    pthread_mutex_destroy(&modificando_filas);
    pthread_mutex_destroy(&mutex_avioes_ativos);

    /* Libera as filas dos setores */
    for (int i = 0; i < n_setores; i++) {
        free(setores[i].fila.contents);
    }

    /* Libera as rotas dos aviões */
    for (int i = 0; i < n_avioes; i++) {
        free(avioes[i].rota);
    }
}
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define PRIORIDADE_BASE 1000
#define TEMPO_BASE 2000

typedef struct Aviao{
    pthread_t thread;
    unsigned int id;
    unsigned int prioridade; 
    struct Setor** rota;
    unsigned int rota_size;
    unsigned int setor_atual; // É o INDICE dentro do vetor rota (0, 1, 2...)
} Aviao;

typedef struct Fila{
    Aviao** contents;
    int size;
    int index;
    int max_size;
} Fila;

typedef struct Setor{
    unsigned int numero_setor;
    Aviao* aviao_atual;
    Fila fila;
} Setor;

typedef struct arg_central {
    Aviao* lista_avioes;
    Setor* lista_setores;
} arg_central;

// Declaracao variaveis globais
sem_t espera_aviao;
sem_t espera_central;
sem_t *aguardando_fila;
pthread_mutex_t mutex_pedido;
Aviao* pedido_aviao = NULL;

void inicializar_sistema(int n_setores, int n_avioes, Setor* setores, Aviao* avioes){
    aguardando_fila = malloc(n_avioes * sizeof(sem_t));
    sem_init(&espera_aviao, 0, 0);
    sem_init(&espera_central, 0, 0);
    for (int i = 0; i < n_avioes; i++)
        sem_init(&aguardando_fila[i], 0, 0);
    pthread_mutex_init(&mutex_pedido, NULL);

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
        avioes[i].prioridade = (rand() % 1000) + 1;

        int max_rota = (n_setores > 10) ? 10 : n_setores;
        int rota_size = (rand() % max_rota) + 2;

        avioes[i].rota_size = rota_size;
        avioes[i].setor_atual = 0;
        avioes[i].rota = malloc(rota_size * sizeof(Setor*));

        /* Distribui o inicio da rota e preenche com ponteiros para setores */
        avioes[i].rota[0] = &setores[i];
        for (int j = 1; j < rota_size; j++) {
            int novo_setor = rand() % n_setores;
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
        printf("\n");
    }
}

void insert_by_priority(Fila *fila, Aviao* novo_aviao) {
    /* Implementação segura: converte a fila circular para linear, insere
       mantendo ordenação por prioridade (maior prioridade primeiro), e copia
       de volta para o buffer circular com index = 0. Isso evita erros de
       indexação com aritmética modular negativa. */
    if (fila->size >= fila->max_size) {
        // fila cheia: não inserimos
        printf("[WARN] fila cheia ao inserir aviao %u\n", novo_aviao->id);
        return;
    }

    Aviao **tmp = malloc(fila->max_size * sizeof(Aviao*));
    if (!tmp) return; // falha alocação (muito improvável)

    // copiar conteúdo lógico (da posição index, em ordem) para tmp[0..size-1]
    for (int i = 0; i < fila->size; i++) {
        tmp[i] = fila->contents[(fila->index + i) % fila->max_size];
    }

    // encontra posição de inserção: manter maior prioridade primeiro
    int pos = 0;
    while (pos < fila->size && novo_aviao->prioridade <= tmp[pos]->prioridade) pos++;

    // move elementos à direita em tmp e insere
    for (int i = fila->size; i > pos; --i) tmp[i] = tmp[i-1];
    tmp[pos] = novo_aviao;
    // copia tmp de volta para o buffer circular, resetando index para 0
    for (int i = 0; i <= fila->size; ++i) {
        fila->contents[i] = tmp[i];
    }
    fila->index = 0;
    fila->size++;
    free(tmp);
}

void * aviao_executa(void * arg) {
    Aviao *aviao = (Aviao *) arg;
    
    while(aviao->setor_atual < aviao->rota_size - 1) {
        printf("[AVIAO %d] ESPERANDO PARA FALAR COM CENTRAL\n", aviao->id);
        pthread_mutex_lock(&mutex_pedido);
        printf("[AVIAO %d] ESTÁ FALANDO COM CENTRAL\n", aviao->id);
        pedido_aviao = aviao;
        sem_post(&espera_central); // Acorda central
        sem_wait(&espera_aviao);   // Espera ACK da central
        pthread_mutex_unlock(&mutex_pedido);
        
        sem_wait(&aguardando_fila[aviao->id]); // Espera autorização de pouso/mudança
        Setor *proximo_setor = aviao->rota[aviao->setor_atual+1];
        Setor *setor_antigo = aviao->rota[aviao->setor_atual];
        proximo_setor->aviao_atual = aviao;
        aviao->setor_atual++;
        setor_antigo->aviao_atual = NULL;
        Fila *fila_setor_antigo = &setor_antigo->fila;
        if (fila_setor_antigo->size != 0){
            Aviao *aviao_primeiro_fila = fila_setor_antigo->contents[fila_setor_antigo->index];
            int old_index = fila_setor_antigo->index;
            fila_setor_antigo->index = (fila_setor_antigo->index + 1) % fila_setor_antigo->max_size;
            fila_setor_antigo->size--;
            fila_setor_antigo->contents[old_index] = NULL;
            if (aviao_primeiro_fila != NULL)
                sem_post(&aguardando_fila[aviao_primeiro_fila->id]);
        }

        
        Setor* setor_atual = aviao->rota[aviao->setor_atual];
        printf("[AVIAO %d] VOANDO PARA SETOR %d\n", aviao->id, setor_atual->numero_setor);
        usleep(TEMPO_BASE + (rand() % 4000)); // Simula tempo de voo [2 segundos, 6 segundos]
        printf("[AVIAO %d] CHEGOU NO SETOR %d\n", aviao->id, setor_atual->numero_setor);
    }
    printf("[AVIAO %d] CHEGOU AO DESTINO FINAL!\n", aviao->id);
    return NULL;
}

void * central_executa(void * arg) {
    arg_central *arguments = (arg_central *) arg;
    while (1) {
        sem_wait(&espera_central);
        Aviao* aviao_atual = pedido_aviao;
        Setor* proximo_setor = aviao_atual->rota[aviao_atual->setor_atual+1];

        if (proximo_setor->aviao_atual == NULL) {
            sem_post(&aguardando_fila[aviao_atual->id]);
        } else {
            Fila* fila_proximo_setor = &proximo_setor->fila;
            insert_by_priority(fila_proximo_setor, aviao_atual);
            printf("[AVIAO %d] ENTROU NA FILA DO SETOR %d\n", aviao_atual->id, proximo_setor->numero_setor);
        }
        sem_post(&espera_aviao);
    }
}

void finalizar_sistema(pthread_t central, Setor* setores, Aviao* avioes, int n_setores, int n_avioes){
    /* Cancela e espera a thread da central */
    pthread_cancel(central);
    pthread_join(central, NULL);

    /* Destrói semáforos */
    sem_destroy(&espera_aviao);
    sem_destroy(&espera_central);
    for (int i = 0; i < n_avioes; i++) {
        sem_destroy(&aguardando_fila[i]);
    }

    free(aguardando_fila);

    /* Destrói mutex */
    pthread_mutex_destroy(&mutex_pedido);

    /* Libera as filas dos setores */
    for (int i = 0; i < n_setores; i++) {
        free(setores[i].fila.contents);
    }

    /* Libera as rotas dos aviões */
    for (int i = 0; i < n_avioes; i++) {
        free(avioes[i].rota);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Uso: %s n_avioes n_setores\n", argv[0]);
        return 1;
    }

    int n_avioes = atoi(argv[1]);
    int n_setores = atoi(argv[2]);

    if(n_avioes > n_setores){
        printf("O numero de avioes nao deve ser maior que o de setores!\n");
        return 1;
    }

    srand(time(NULL));

    Aviao avioes[n_avioes];
    Setor setores[n_setores];
    pthread_t central;
    
    inicializar_sistema(n_setores, n_avioes, setores, avioes);
    
    arg_central c_arg;
    c_arg.lista_avioes = avioes;
    c_arg.lista_setores = setores;
    
    pthread_create(&central, NULL, central_executa, (void *) &c_arg);

    for (int i = 0; i < n_avioes; i++) {
        pthread_create(&avioes[i].thread, NULL, aviao_executa, (void *) &avioes[i]);
    }

    for (int i = 0; i < n_avioes; i++) {
        pthread_join(avioes[i].thread, NULL);
    }
    
    printf("Simulação finalizada.\n");
    /* Finaliza recursos: cancela/joina central, destrói semáforos e libera memórias */
    finalizar_sistema(central, setores, avioes, n_setores, n_avioes);
    return 0;
}
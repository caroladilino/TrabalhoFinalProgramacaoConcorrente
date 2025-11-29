#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#define PRIORIDADE_BASE 1000

struct Aviao;
struct Setor;

typedef struct Aviao{
    pthread_t thread;
    unsigned int id;
    unsigned int prioridade;
    unsigned int* rota;
    unsigned int rota_size;
    unsigned int setor_atual;
    sem_t espera_aviao;
    sem_t espera_central;
    sem_t na_fila;
    pthread_mutex_t mutex_pedido;
    Aviao* pedido_aviao;
} Aviao;

typedef struct Setor{
    unsigned int numero_setor;
    Aviao* aviao_atual;
    Aviao* fila;
    unsigned int size;
} Setor;

typedef struct arg_central {
    Aviao* lista_avioes;
    Setor* lista_setores;
    sem_t espera_aviao;
    sem_t espera_central;
    Aviao *pedido_aviao;
} arg_central;

void inicializar_sistema(int n_setores, int n_avioes, Setor* setores, Aviao* avioes, sem_t espera_aviao, sem_t espera_central, sem_t *sem_filas, pthread_mutex_t mutex_pedido){
    for(int i = 0 ; i < n_setores ; i++){
        Setor s;
        s.numero_setor = i;
        s.aviao_atual = NULL;
        // Alocando espaço na memória para um vetor com tamanho de n_avioes
        s.fila = malloc(n_avioes * sizeof(Aviao));
        s.size = 0;
        setores[i];
    }

    for(int i = 0; i < n_avioes; i++) {
        Aviao a;
        a.id = i;
        // rand() -> [0, 999]. Soma um para intervalo [1, 1000] -> nivel prioridade
        a.prioridade = (rand() % 1000) + 1;
        // Tamanho máximo de uma rota [3, 14]
        int rota_size = (rand() % n_setores) + 3;
        a.rota = malloc(rota_size * sizeof(Setor));
        a.rota[0] = i;
        a.setor_atual = 0;
        for (int j = 1; j < rota_size; j++) {
            // Setores vão de [0, n_setores-1]
            int novo_setor = rand() % n_setores;
            if (novo_setor == a.rota[j-1])
                novo_setor = (novo_setor + 1) % n_setores;
            a.rota[j] = novo_setor;
        }

        a.espera_aviao = espera_aviao;
        a.espera_central = espera_central;
        a.na_fila = sem_filas[i];
        a.mutex_pedido = mutex_pedido;
        a.pedido_aviao = NULL;
        avioes[i] = a;

        printf("[AVIÃO %d] Criado!\n", a.id);
        printf("[AVIÃO %d] Prioridade: %d\n", a.id, a.prioridade);
        printf("[AVIÃO %d] Rota ", a.id);
        for (int j = 0; j < rota_size; j++) {
            printf("S%d ", a.rota[j]);
            if (j < rota_size-1)
                printf("-> ");
        }
        printf("\n");

        sem_init(&espera_aviao, 0, 0);
        sem_init(&espera_central, 0, 0);
        for (int i = 0; i < n_avioes; i++)
            sem_init(&sem_filas[i], 0, 0);
        pthread_mutex_init(&mutex_pedido, NULL);
    }
}

void * aviao_executa(void * arg) {
    Aviao *aviao = (Aviao *) arg;
    pthread_mutex_lock(&aviao->mutex_pedido);
    aviao->pedido_aviao = aviao;
    sem_post(&aviao->espera_central);
    sem_wait(&aviao->espera_aviao);
    pthread__unlock(&aviao->mutex_pedido);
    sem_wait(&aviao->na_fila);
}

void * central_executa(void * arg) {
    arg_central *arguments = (arg_central *) arg;
    while (1) {
        sem_wait(&arguments->espera_central);
        Aviao *aviao_atual = arguments->pedido_aviao;
        if (arguments->lista_setores[aviao_atual->rota[aviao_atual->setor_atual+1]].aviao_atual == NULL) {
            aviao_atual->setor_atual++;

        }
    }

}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Uso: %s n_threads n_loops\n", argv[0]);
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
    sem_t espera_aviao;
    sem_t espera_central;
    sem_t sem_filas[n_avioes];
    pthread_mutex_t mutex_pedido;
    inicializar_sistema(n_setores, n_avioes, setores, avioes, espera_aviao, espera_central, sem_filas, mutex_pedido);
    
    Aviao* pedido_aviao = NULL;
    arg_central c_arg;
    c_arg.espera_aviao = espera_aviao;
    c_arg.espera_central = espera_central;
    c_arg.lista_avioes = avioes;
    c_arg.lista_setores = setores;
    c_arg.pedido_aviao = pedido_aviao;
    pthread_create(&central, NULL, central_executa, (void *) &c_arg);

    for (int i = 0; i < n_avioes; i++) {
        pthread_create(&avioes[i].thread, NULL, aviao_executa, (void *) &avioes[i]);
    }

    for (int i = 0; i < n_avioes; i++) {
        phtread_join(avioes[i], NULL);
    }
    pthread_join(central, NULL);

}



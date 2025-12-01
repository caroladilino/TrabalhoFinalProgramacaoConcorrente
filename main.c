#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define PRIORIDADE_BASE 1000

typedef struct Aviao{
    pthread_t thread;
    unsigned int id;
    unsigned int prioridade; 
    struct Setor** rota;
    unsigned int rota_size;
    unsigned int setor_atual; // É o INDICE dentro do vetor rota (0, 1, 2...)
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
} arg_central;

// Declaracao variaveis globais
sem_t espera_aviao;
sem_t espera_central;
sem_t *na_fila;
pthread_mutex_t mutex_pedido;
Aviao* pedido_aviao = NULL;

void inicializar_sistema(int n_setores, int n_avioes, Setor* setores, Aviao* avioes){
    na_fila = malloc(n_avioes * sizeof(sem_t));
    sem_init(&espera_aviao, 0, 0);
    sem_init(&espera_central, 0, 0);
    for (int i = 0; i < n_avioes; i++)
        sem_init(&na_fila[i], 0, 0);
    pthread_mutex_init(&mutex_pedido, NULL);
    
    for(int i = 0 ; i < n_setores ; i++){
        Setor s;
        s.numero_setor = i;
        s.aviao_atual = NULL;
        s.fila = malloc(n_avioes * sizeof(Aviao));
        s.size = 0;
        setores[i] = s;
    }

    for(int i = 0; i < n_avioes; i++) {
        Aviao a;
        a.id = i;
        a.prioridade = (rand() % 1000) + 1;
        
        // Proteção para rota não ser maior que setores disponíveis
        int max_rota = (n_setores > 10) ? 10 : n_setores;
        int rota_size = (rand() % max_rota) + 2; 

        a.rota = malloc(rota_size * sizeof(unsigned int));
        a.rota[0] = i % n_setores; // Distribui o inicio
        a.setor_atual = 0;
        a.rota_size = rota_size;

        for (int j = 1; j < rota_size; j++) {
            int novo_setor = rand() % n_setores;
            if (novo_setor == a.rota[j-1])
                novo_setor = (novo_setor + 1) % n_setores;
            a.rota[j] = novo_setor;
        }

        avioes[i] = a;
        
        // --- CORREÇÃO 1: Usar endereço do vetor persistente, não da variável local ---
        setores[avioes[i].rota[0]].aviao_atual = &avioes[i];

        printf("[AVIÃO %d] Criado! Rota: ", avioes[i].id);
        for (int j = 0; j < rota_size; j++) {
            printf("S%d ", avioes[i].rota[j]);
            if (j < rota_size-1) printf("-> ");
        }
        printf("\n");
    }
}

void * aviao_executa(void * arg) {
    Aviao *aviao = (Aviao *) arg;
    
    // --- CORREÇÃO 3: Loop para percorrer a rota ---
    while(aviao->setor_atual < aviao->rota_size - 1) {
        printf("[AVIAO %d] TENTANDO FALAR COM CENTRAL\n", aviao->id);
        
        pthread_mutex_lock(&mutex_pedido);
        pedido_aviao = aviao;
        sem_post(&espera_central); // Acorda central
        sem_wait(&espera_aviao);   // Espera ACK da central
        pthread_mutex_unlock(&mutex_pedido);
        
        sem_wait(&na_fila[aviao->id]); // Espera autorização de pouso/mudança
        
        sleep(1); // Simula tempo de voo
    }
    printf("[AVIAO %d] CHEGOU AO DESTINO FINAL!\n", aviao->id);
    return NULL;
}

void * central_executa(void * arg) {
    arg_central *arguments = (arg_central *) arg;
    while (1) {
        sem_wait(&espera_central);
        Aviao *aviao_atual = pedido_aviao;
        
        // ACK imediato para liberar o mutex do avião
        sem_post(&espera_aviao);

        // --- CORREÇÃO 2: Lógica correta de IDs de setor ---
        int indice_rota = aviao_atual->setor_atual;
        int id_setor_atual = aviao_atual->rota[indice_rota];
        int id_proximo_setor = aviao_atual->rota[indice_rota + 1];

        if (arguments->lista_setores[id_proximo_setor].aviao_atual == NULL) {
            // Limpa o setor onde ele estava (usando o ID correto)
            arguments->lista_setores[id_setor_atual].aviao_atual = NULL;
            
            // Atualiza índice
            aviao_atual->setor_atual++;
            
            // Ocupa novo setor
            arguments->lista_setores[id_proximo_setor].aviao_atual = aviao_atual;
            
            printf(">>> [CENTRAL] AVIAO %d moveu de S%d para S%d\n", 
                   aviao_atual->id, id_setor_atual, id_proximo_setor);
        } else {
            printf("... [CENTRAL] AVIAO %d EM ESPERA (S%d ocupado)\n", 
                   aviao_atual->id, id_proximo_setor);
        }
        
        sem_post(&na_fila[aviao_atual->id]);
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
    // pthread_join(central, NULL); // Central nunca termina, então deixamos o programa sair
    return 0;
}
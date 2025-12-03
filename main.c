#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>

#define PRIORIDADE_BASE 1000
#define TEMPO_BASE 500000
#define TIMEOUT 5

typedef struct Aviao{
pthread_t thread;
unsigned int id;
unsigned int prioridade;
struct Setor** rota;
unsigned int rota_size;
unsigned int setor_atual; // É o INDICE dentro do vetor rota (0, 1, 2...)
int refazer_pedido;
double tempo_espera;
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
Setor* lista_setores;
int n_setores;
} arg_central;

// Declaracao variaveis globais
int avioes_ativos;
sem_t espera_aviao;
sem_t espera_central;
sem_t aguardando_fila;
pthread_mutex_t mutex_pedido;
pthread_mutex_t modificando_filas;
pthread_mutex_t mutex_avioes_ativos;
Aviao pedido_aviao = NULL;

double tempo_atual() {
struct timeval tv;
gettimeofday(&tv, NULL);
return tv.tv_sec + tv.tv_usec * 1e-6;
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
    printf(" Prioridade: %d\n", avioes[i].prioridade);  
    fflush(stdout);  
}

}

void inserir_por_prioridade(Fila fila, Aviao novo_aviao) {
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

void inserir_no_fim(Fila fila, Aviao aviao) {
if (fila->size == fila->max_size) return;

aviao->tempo_espera = tempo_atual();  
fila->contents[fila->size] = aviao;  
fila->size++;

}

void remover_da_fila(Fila fila, Aviao aviao) {
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
double espera = (double) (tempo_atual() - a->tempo_espera);

if (espera > TIMEOUT) {  
            printf("[AVIAO %d] AVIÃO FICOU MUITO TEMPO NA FILA DE ESPERA. EXECUTANDO MANOBRA DE EMERGÊNCIA\n", a->id);  
            fflush(stdout);  
              
            remover_da_fila(fila, a);  
            a->refazer_pedido = 1;  
            sem_post(&aguardando_fila[a->id]);  
        }  
    }  
}

}

int deadlock_check(Aviao* aviao_inicial) {
Aviao* temp = aviao_inicial;
do {
if (temp->setor_atual + 1 == temp->rota_size) return 0;

Setor* proximo_setor = temp->rota[temp->setor_atual+1];  

    if (proximo_setor->fila.size == 0 || proximo_setor->fila.contents[0] != temp)  
        return 0;   

    temp = proximo_setor->aviao_atual;   
} while (temp != aviao_inicial);  

return 1; /* Ciclo encontrado = DEADLOCK */

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

void finalizar_sistema(pthread_t central, Setor* setores, Aviao* avioes, int n_setores, int n_avioes){
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
c_arg.lista_setores = setores;  
c_arg.n_setores = n_setores;  
  
pthread_create(&central, NULL, central_executa, (void *) &c_arg);  

for (int i = 0; i < n_avioes; i++) {  
    pthread_create(&avioes[i].thread, NULL, aviao_executa, (void *) &avioes[i]);  
}  

for (int i = 0; i < n_avioes; i++) {  
    pthread_join(avioes[i].thread, NULL);  
}  

pthread_join(central, NULL);  
  
printf("Simulação finalizada.\n");  
fflush(stdout);  
  
/* Finaliza recursos: cancela/joina central, destrói semáforos e libera memórias */  
finalizar_sistema(central, setores, avioes, n_setores, n_avioes);  
return 0;

}

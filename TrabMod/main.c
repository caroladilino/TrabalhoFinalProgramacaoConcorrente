#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "defs.h"
#include "controle.h"

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
    finalizar_sistema(setores, avioes, n_setores, n_avioes);
    return 0;
}
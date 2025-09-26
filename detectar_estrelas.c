#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#define BRILHO_MINIMO 200

// Estrutura para armazenar as coordenadas de uma estrela
typedef struct {
    int x, y;
} CoordenadaEstrela;

// Função para ler o cabeçalho de uma imagem PGM (formato P2)
void lerCabecalhoPGM(FILE *fp, int *largura, int *altura, int *max_val) {
    char p2[3];
    fscanf(fp, "%2s", p2); // Lê o "P2"
    if (strcmp(p2, "P2") != 0) {
        fprintf(stderr, "Erro: Formato de arquivo PGM inválido. Esperado P2.\n");
        exit(1);
    }
    fscanf(fp, "%d %d", largura, altura);
    fscanf(fp, "%d", max_val);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // --- DIAGNÓSTICO INICIAL ---
    // Todos os processos imprimirão esta mensagem.
    printf("[Processo %d/%d] Olá! O processo foi iniciado.\n", rank, size);
    fflush(stdout);
    
    // Sincroniza os processos para garantir que as mensagens de "Olá" apareçam antes do resto.
    MPI_Barrier(MPI_COMM_WORLD);

    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "Erro: Este programa requer pelo menos 2 processos (1 mestre e 1 escravo).\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (argc != 2) {
        if (rank == 0) {
            fprintf(stderr, "Uso: mpirun -np <num_processos> %s <arquivo_imagem.pgm>\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    // =================================================
    // PROCESSO MESTRE (RANK 0)
    // =================================================
    if (rank == 0) {
        printf("[Mestre %d] Lendo a imagem '%s'...\n", rank, argv[1]);
        fflush(stdout);

        FILE *arquivoImagem = fopen(argv[1], "r");
        if (!arquivoImagem) {
            perror("Erro ao abrir a imagem");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        int largura, altura, max_val;
        lerCabecalhoPGM(arquivoImagem, &largura, &altura, &max_val);

        int **imagem = (int **)malloc(altura * sizeof(int *));
        for (int i = 0; i < altura; i++) {
            imagem[i] = (int *)malloc(largura * sizeof(int));
        }

        for (int i = 0; i < altura; i++) {
            for (int j = 0; j < largura; j++) {
                fscanf(arquivoImagem, "%d", &imagem[i][j]);
            }
        }
        fclose(arquivoImagem);

        int num_escravos = size - 1;
        int largura_bloco = largura / num_escravos;
        printf("[Mestre %d] Distribuindo trabalho para %d escravos...\n", rank, num_escravos);
        fflush(stdout);

        for (int i = 1; i <= num_escravos; i++) {
            int inicio_col = (i - 1) * largura_bloco;
            int fim_col = (i == num_escravos) ? largura : i * largura_bloco;
            int largura_envio = fim_col - inicio_col;

            MPI_Send(&altura, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&largura_envio, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

            for (int j = 0; j < altura; j++) {
                MPI_Send(&imagem[j][inicio_col], largura_envio, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
        }

        printf("[Mestre %d] Coletando resultados...\n", rank);
        fflush(stdout);

        int total_estrelas = 0;
        printf("\n--- ESTRELAS ENCONTRADAS ---\n");
        for (int i = 1; i <= num_escravos; i++) {
            int inicio_col = (i - 1) * largura_bloco;
            int num_estrelas_escravo;
            MPI_Status status;
            MPI_Recv(&num_estrelas_escravo, 1, MPI_INT, i, 0, MPI_COMM_WORLD, &status);
            
            if (num_estrelas_escravo > 0) {
                CoordenadaEstrela *estrelas_escravo = (CoordenadaEstrela *)malloc(num_estrelas_escravo * sizeof(CoordenadaEstrela));
                MPI_Recv(estrelas_escravo, num_estrelas_escravo * sizeof(CoordenadaEstrela), MPI_BYTE, i, 0, MPI_COMM_WORLD, &status);
                
                for(int j = 0; j < num_estrelas_escravo; j++) {
                    printf("  - Escravo %d encontrou estrela na coordenada global (%d, %d)\n", status.MPI_SOURCE, estrelas_escravo[j].x + inicio_col, estrelas_escravo[j].y);
                    fflush(stdout);
                }
                free(estrelas_escravo);
            }
            total_estrelas += num_estrelas_escravo;
        }

        printf("\nTotal de estrelas detectadas: %d\n", total_estrelas);
        fflush(stdout);

        for (int i = 0; i < altura; i++) free(imagem[i]);
        free(imagem);

    // =================================================
    // PROCESSOS ESCRAVOS (RANK > 0)
    // =================================================
    } else {
        int altura, largura_bloco;
        MPI_Recv(&altura, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&largura_bloco, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("[Escravo %d] Recebi um bloco de %dx%d. Processando...\n", rank, largura_bloco, altura);
        fflush(stdout);

        int **bloco_imagem = (int **)malloc(altura * sizeof(int *));
        for (int i = 0; i < altura; i++) {
            bloco_imagem[i] = (int *)malloc(largura_bloco * sizeof(int));
            MPI_Recv(bloco_imagem[i], largura_bloco, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        CoordenadaEstrela estrelas_encontradas[1000];
        int num_estrelas = 0;

        for (int y = 1; y < altura - 1; y++) {
            for (int x = 1; x < largura_bloco - 1; x++) {
                int pixel_central = bloco_imagem[y][x];
                if (pixel_central > BRILHO_MINIMO) {
                    if (pixel_central > bloco_imagem[y-1][x-1] &&
                        pixel_central > bloco_imagem[y-1][x]   &&
                        pixel_central > bloco_imagem[y-1][x+1] &&
                        pixel_central > bloco_imagem[y][x-1]   &&
                        pixel_central > bloco_imagem[y][x+1]   &&
                        pixel_central > bloco_imagem[y+1][x-1] &&
                        pixel_central > bloco_imagem[y+1][x]   &&
                        pixel_central > bloco_imagem[y+1][x+1]) 
                    {
                        if(num_estrelas < 1000) {
                            estrelas_encontradas[num_estrelas].x = x;
                            estrelas_encontradas[num_estrelas].y = y;
                            num_estrelas++;
                        }
                    }
                }
            }
        }
        
        printf("[Escravo %d] Processamento concluído. Encontrei %d estrela(s). Enviando para o mestre...\n", rank, num_estrelas);
        fflush(stdout);

        MPI_Send(&num_estrelas, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        if (num_estrelas > 0) {
            MPI_Send(estrelas_encontradas, num_estrelas * sizeof(CoordenadaEstrela), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
        }

        for (int i = 0; i < altura; i++) free(bloco_imagem[i]);
        free(bloco_imagem);
    }
    
    printf("[Processo %d/%d] Finalizando.\n", rank, size);
    fflush(stdout);

    MPI_Finalize();
    return 0;
}
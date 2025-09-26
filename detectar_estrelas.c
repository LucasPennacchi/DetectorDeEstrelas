#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

//=================================================
// PARÂMETROS DE DETECÇÃO
//=================================================
#define BRILHO_MINIMO 200               // Luz mínima para considerar como estrela
#define LIMIAR_DE_CIRCULARIDADE 0.75    // Verifica se tem forma de estrela
#define OVERLAP 30                      // Verifica se a estrela entra no campo de outro processo
#define M_PI 3.14

//=================================================
// ESTRUTURAS E FUNÇÕES AUXILIARES
//=================================================
typedef struct {
    int x, y;
} Point;

typedef struct {
    Point *points;
    int front, rear, size, capacity;
} Queue;

typedef struct {
    int id;
    int area;
    double perimetro;
    Point centro;
    Point *pixels;
} Blob;

// ... (Funções da Fila e lerCabecalhoPGM - sem alterações) ...
Queue* createQueue(int capacity) {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->capacity = capacity;
    q->front = q->size = 0;
    q->rear = capacity - 1;
    q->points = (Point*)malloc(q->capacity * sizeof(Point));
    return q;
}
int isQueueEmpty(Queue* q) { return (q->size == 0); }
void enqueue(Queue* q, Point item) {
    if (q->size == q->capacity) return;
    q->rear = (q->rear + 1) % q->capacity;
    q->points[q->rear] = item;
    q->size = q->size + 1;
}
Point dequeue(Queue* q) {
    Point item = q->points[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size = q->size - 1;
    return item;
}
void freeQueue(Queue* q) {
    free(q->points);
    free(q);
}
void lerCabecalhoPGM(FILE *fp, int *largura, int *altura, int *max_val) {
    char p2[3];
    fscanf(fp, "%2s", p2);
    fscanf(fp, "%d %d", largura, altura);
    fscanf(fp, "%d", max_val);
}

/**
 * @brief Encontra todos os componentes conectados (blobs) numa imagem binarizada.
 */
Blob* encontrarComponentesConectados(int *img_binaria, int altura, int largura_bloco, int *img_rotulada, int *num_blobs_encontrados) {
    int num_blobs = 0;
    Blob *blobs = NULL;
    Queue *q = createQueue(largura_bloco * altura);

    for (int y = 0; y < altura; y++) {
        for (int x = 0; x < largura_bloco; x++) {
            if (img_binaria[y * largura_bloco + x] == 1 && img_rotulada[y * largura_bloco + x] == 0) {
                num_blobs++;
                blobs = (Blob *)realloc(blobs, num_blobs * sizeof(Blob));
                Blob *b = &blobs[num_blobs - 1];
                b->id = num_blobs;
                b->area = 0;
                b->perimetro = 0;
                b->centro = (Point){0, 0};
                b->pixels = NULL;

                enqueue(q, (Point){x, y});
                img_rotulada[y * largura_bloco + x] = b->id;

                while (!isQueueEmpty(q)) {
                    Point p = dequeue(q);
                    b->area++;
                    b->pixels = (Point *)realloc(b->pixels, b->area * sizeof(Point));
                    b->pixels[b->area - 1] = p;
                    b->centro.x += p.x;
                    b->centro.y += p.y;
                    
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            int nx = p.x + dx;
                            int ny = p.y + dy;
                            if (nx >= 0 && nx < largura_bloco && ny >= 0 && ny < altura) {
                                if (img_binaria[ny * largura_bloco + nx] == 1 && img_rotulada[ny * largura_bloco + nx] == 0) {
                                    img_rotulada[ny * largura_bloco + nx] = b->id;
                                    enqueue(q, (Point){nx, ny});
                                }
                            }
                        }
                    }
                }
                if (b->area > 0) {
                    b->centro.x /= b->area;
                    b->centro.y /= b->area;
                }
            }
        }
    }
    freeQueue(q);
    *num_blobs_encontrados = num_blobs;
    return blobs;
}


//=================================================
// NOVA FUNÇÃO PARA CÁLCULO DO PERÍMETRO
//=================================================
/**
 * @brief Calcula o perímetro de um blob contando seus pixels de borda.
 */
double calcularPerimetro(Blob* b, int* img_rotulada, int altura, int largura_bloco) {
    double perimetro = 0;
    for (int j = 0; j < b->area; j++) {
        Point p = b->pixels[j];
        int eh_borda = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = p.x + dx;
                int ny = p.y + dy;
                // Um pixel é borda se um de seus vizinhos está fora da imagem ou não pertence ao mesmo blob.
                if (nx < 0 || nx >= largura_bloco || ny < 0 || ny >= altura || img_rotulada[ny * largura_bloco + nx] != b->id) {
                    eh_borda = 1;
                    break;
                }
            }
            if (eh_borda) break;
        }
        if (eh_borda) perimetro++;
    }
    return perimetro;
}


//=================================================
// FUNÇÃO PRINCIPAL
//=================================================
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // ... (Validação de argumentos - sem alterações) ...
    if (size < 2) {
        if (rank == 0) fprintf(stderr, "Erro: Requer pelo menos 2 processos.\n");
        MPI_Finalize();
        return 1;
    }
    if (argc != 2) {
        if (rank == 0) fprintf(stderr, "Uso: mpirun -np <N> %s <imagem.pgm>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    //=================================================
    // PROCESSO MESTRE (RANK 0)
    //=================================================
    if (rank == 0) {
        // ... (código do mestre sem alterações) ...
        FILE *arquivoImagem = fopen(argv[1], "r");
        if (!arquivoImagem) {
            perror("Erro ao abrir imagem");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        int largura, altura, max_val;
        lerCabecalhoPGM(arquivoImagem, &largura, &altura, &max_val);

        int *imagem_linear = (int *)malloc(largura * altura * sizeof(int));
        for (int i = 0; i < altura * largura; i++) {
            fscanf(arquivoImagem, "%d", &imagem_linear[i]);
        }
        fclose(arquivoImagem);

        int num_escravos = size - 1;
        int largura_base = largura / num_escravos;

        for (int i = 1; i <= num_escravos; i++) {
            int inicio_autoridade = (i - 1) * largura_base;
            int fim_autoridade = (i == num_escravos) ? largura : i * largura_base;

            int inicio_envio = fmax(0, inicio_autoridade - OVERLAP);
            int fim_envio = fmin(largura, fim_autoridade + OVERLAP);
            int largura_envio = fim_envio - inicio_envio;
            
            int largura_autoridade = fim_autoridade - inicio_autoridade;
            int offset_autoridade = inicio_autoridade - inicio_envio;

            MPI_Send(&altura, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&largura_envio, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&largura_autoridade, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&offset_autoridade, 1, MPI_INT, i, 0, MPI_COMM_WORLD);

            int *bloco_envio = (int *)malloc(altura * largura_envio * sizeof(int));
            for (int y = 0; y < altura; y++) {
                memcpy(&bloco_envio[y * largura_envio], &imagem_linear[y * largura + inicio_envio], largura_envio * sizeof(int));
            }
            MPI_Send(bloco_envio, altura * largura_envio, MPI_INT, i, 0, MPI_COMM_WORLD);
            free(bloco_envio);
        }

        int total_estrelas = 0;
        for (int i = 1; i <= num_escravos; i++) {
            int contagem_escravo;
            MPI_Recv(&contagem_escravo, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            total_estrelas += contagem_escravo;
        }

        printf("Total de estrelas detectadas: %d\n", total_estrelas);
        free(imagem_linear);
    }
    //=================================================
    // PROCESSOS ESCRAVOS (RANK > 0)
    //=================================================
    else {
        int altura, largura_bloco, largura_autoridade, offset_autoridade;
        MPI_Recv(&altura, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&largura_bloco, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&largura_autoridade, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&offset_autoridade, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int *bloco_imagem = (int *)malloc(altura * largura_bloco * sizeof(int));
        MPI_Recv(bloco_imagem, altura * largura_bloco, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        int *img_binaria = (int *)calloc(altura * largura_bloco, sizeof(int));
        for (int i = 0; i < altura * largura_bloco; i++) {
            if (bloco_imagem[i] > BRILHO_MINIMO) {
                img_binaria[i] = 1;
            }
        }

        // Encontra blobs e faz a busca de estrelas usando a imagem agora binária        
        int *img_rotulada = (int *)calloc(altura * largura_bloco, sizeof(int));
        int num_blobs = 0;
        Blob *blobs = encontrarComponentesConectados(img_binaria, altura, largura_bloco, img_rotulada, &num_blobs);
        
        int estrelas_contadas = 0;
        for (int i = 0; i < num_blobs; i++) {
            Blob *b = &blobs[i];
            
            // Verifica se NÃO é o responsável do blob
            if (b->centro.x < offset_autoridade || b->centro.x >= offset_autoridade + largura_autoridade) {
                free(b->pixels);
                continue; // passa pro próximo blob
            }
            
            // Verifica o perímetro dos blobs
            b->perimetro = calcularPerimetro(b, img_rotulada, altura, largura_bloco);
            
            // Verifica se é um circulo ou um conjunto de estrelas que tem forma irregular
            double circularidade = (b->area > 0 && b->perimetro > 0) ? (4 * M_PI * b->area) / (b->perimetro * b->perimetro) : 0;
            
            if (circularidade >= LIMIAR_DE_CIRCULARIDADE) {
                estrelas_contadas++;
            } else {
                int picos = 0;
                for (int j = 0; j < b->area; j++) {
                    Point p = b->pixels[j];
                    int valor_central = bloco_imagem[p.y * largura_bloco + p.x];
                    int eh_pico = 1;
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = p.x + dx, ny = p.y + dy;
                            if (nx >= 0 && nx < largura_bloco && ny >= 0 && ny < altura) {
                                if (bloco_imagem[ny*largura_bloco + nx] > valor_central) {
                                    eh_pico = 0;
                                    break;
                                }
                            }
                        }
                        if(!eh_pico) break;
                    }
                    if (eh_pico) picos++;
                }
                estrelas_contadas += (picos > 0) ? picos : 1;
            }
            free(b->pixels);
        }

        MPI_Send(&estrelas_contadas, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        
        free(blobs);
        free(bloco_imagem);
        free(img_binaria);
        free(img_rotulada);
    }

    MPI_Finalize();
    return 0;
}
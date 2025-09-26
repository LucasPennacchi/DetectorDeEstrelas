# DetectorDeEstrelas
Trabalho de faculdade para matéria de Computação Paralela e Distribuída - Unifal 2025

1. Visão Geral
O detectar_estrelas_avancado.c é um programa de alto desempenho escrito em C com MPI (Message Passing Interface) projetado para identificar e contar "estrelas" em imagens astronómicas no formato PGM (Portable Graymap).

O programa foi construído para ser robusto e lidar com cenários complexos do mundo real, incluindo:

Estrelas de tamanhos variados, desde pequenos pontos de luz até objetos maiores.

Aglomerados de estrelas, onde múltiplos objetos se sobrepõem ou se tocam.

Processamento paralelo eficiente, dividindo a carga de trabalho entre múltiplos processos para acelerar a análise em máquinas multi-core ou clusters.

2. Compilação e Execução
2.1. Requisitos
Um compilador C (como o GCC).

Uma implementação do padrão MPI (ex: Open MPI ou MPICH).

2.2. Compilação
Use o compilador wrapper do MPI, mpicc, para compilar o código. Isso garante que todas as bibliotecas MPI necessárias sejam vinculadas corretamente.

Bash

mpicc -o detectar_estrelas_avancado detectar_estrelas_avancado.c -lm
(A flag -lm é adicionada para vincular a biblioteca matemática, necessária para funções como sqrt e fmax/fmin)

2.3. Execução
O programa é executado com mpirun. É necessário especificar o número de processos a serem usados com a flag -np e o caminho para a imagem PGM.

Bash

mpirun -np <numero_de_processos> ./detectar_estrelas_avancado <caminho_para_imagem.pgm>
Exemplo com 4 processos:

Bash

mpirun -np 4 ./detectar_estrelas_avancado estrelas3.pgm
Nota Importante para Execução Local:
Ao executar num computador pessoal (laptop/desktop), o MPI pode limitar o número de processos por padrão. Para forçar a execução com o número de processos solicitado, use a flag --oversubscribe:

Bash

mpirun -np 4 --oversubscribe ./detectar_estrelas_avancado estrelas3.pgm
3. Estratégia e Algoritmos Implementados
O programa utiliza um modelo Mestre-Escravo e uma cadeia de algoritmos de visão computacional para garantir uma contagem precisa.

3.1. Paralelismo e Divisão de Tarefas
Processo Mestre (Rank 0):

Lê a imagem PGM completa para a memória.

Divide a imagem em fatias verticais, uma para cada processo escravo.

Envia a cada escravo a sua fatia de trabalho.

Aguarda e recolhe o resultado (a contagem de estrelas) de cada escravo.

Soma os resultados e apresenta a contagem final.

Processos Escravos (Rank > 0):

Recebem uma fatia da imagem.

Executam o algoritmo de deteção avançada nessa fatia.

Enviam a sua contagem final de volta para o mestre.

3.2. Solução para o Problema da Fronteira
Para evitar que estrelas localizadas nas bordas das fatias sejam contadas duplamente ou ignoradas, implementou-se uma solução em duas partes:

Zonas de Sobreposição (Overlap Zones): O mestre não envia fatias exatas. Ele envia a cada escravo a sua fatia de responsabilidade mais uma "moldura" extra com os pixels dos seus vizinhos (definido pelo OVERLAP). Isso garante que cada escravo tenha o contexto completo para analisar qualquer estrela perto da sua borda.

Regra de Autoridade: Após um escravo detetar uma estrela, ele calcula o seu centro de massa. A estrela só é contabilizada se o seu centro de massa estiver dentro da zona de responsabilidade original do escravo (excluindo a área de sobreposição). Isso garante que, embora dois escravos possam ver a mesma estrela na fronteira, apenas um (o que tem "autoridade" sobre o seu centro) a irá contar.

3.3. Algoritmo de Deteção (Por Escravo)
Cada escravo executa um pipeline de análise sofisticado:

Binarização: A imagem recebida é convertida para um formato binário (preto e branco). Pixels com brilho acima do BRILHO_MINIMO tornam-se "ativos" (brancos), o resto é ignorado (preto).

Deteção de Blobs (Análise de Componentes Conectados): O programa varre a imagem binarizada e agrupa todos os pixels ativos conectados, formando "blobs" ou "manchas". Cada blob representa um objeto candidato a ser uma ou mais estrelas.

Análise Morfológica (Decisão Baseada na Forma): Esta é a parte mais inteligente do algoritmo. Para cada blob, o programa decide se é uma estrela simples ou um aglomerado complexo:

Cálculo de Circularidade: O programa mede quão "redondo" o blob é usando a fórmula (4 * PI * Área) / (Perímetro²), onde um círculo perfeito resulta em 1.0.

Decisão:

Se a circularidade for alta (>= LIMIAR_DE_CIRCULARIDADE), a forma é considerada simples. O blob é contado como 1 estrela.

Se a circularidade for baixa, a forma é complexa e irregular, indicando um aglomerado de estrelas sobrepostas.

Separação de Aglomerados: Para os blobs complexos, o programa ativa um algoritmo para contar os objetos individuais dentro dele. Ele simula o resultado de uma Transformada da Distância ao encontrar todos os picos de brilho (máximos locais) dentro da área do blob. O número de picos encontrados é o número de estrelas no aglomerado.

4. Parâmetros Configuráveis
No topo do ficheiro detectar_estrelas_avancado.c, existem constantes que podem ser ajustadas para otimizar a deteção para diferentes tipos de imagens:

BRILHO_MINIMO: (Padrão: 200) Define o limiar de brilho. Aumente para detetar apenas os objetos mais brilhantes; diminua para incluir objetos mais ténues.

LIMIAR_DE_CIRCULARIDADE: (Padrão: 0.75) Controla a sensibilidade da deteção de aglomerados. Diminua se estrelas únicas ligeiramente irregulares estiverem a ser contadas como aglomerados; aumente se aglomerados óbvios não estiverem a ser separados.

OVERLAP: (Padrão: 30) Define o tamanho em pixels da zona de sobreposição. Este valor deve ser maior que o raio da maior estrela esperada na imagem para garantir uma deteção correta nas fronteiras.

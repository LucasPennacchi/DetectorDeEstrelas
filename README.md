# DetectorDeEstrelas
Trabalho de faculdade para matéria de Computação Paralela e Distribuída - Unifal 2025

-----

# Documentação: Detector Paralelo de Estrelas com MPI

## 1\. Visão Geral

O `detectar_estrelas.c` é um programa de alto desempenho escrito em C com MPI (Message Passing Interface) projetado para identificar e contar "estrelas" em imagens astronómicas no formato PGM (Portable Graymap).

O programa foi construído para ser robusto e lidar com cenários complexos do mundo real, incluindo:

  * **Estrelas de tamanhos variados**, desde pequenos pontos de luz até objetos maiores.
  * **Aglomerados de estrelas**, onde múltiplos objetos se sobrepõem ou se tocam.
  * **Processamento paralelo eficiente**, dividindo a carga de trabalho entre múltiplos processos para acelerar a análise em máquinas multi-core ou clusters.

-----

## 2\. Compilação e Execução

Esta seção detalha como compilar o código e as várias formas de o executar, especialmente em ambientes de Máquina Virtual (VM) onde a configuração de rede do MPI pode ser um desafio.

#### 2.1. Requisitos

  * Um compilador C (como o GCC).
  * Uma implementação do padrão MPI. **Este guia foca-se no MPICH**, que é comum em distribuições como o Ubuntu.

#### 2.2. Compilação

Use o compilador wrapper do MPI, `mpicc`, para compilar o código. A flag `-lm` é essencial para vincular a biblioteca de matemática.

```bash
mpicc -o detectar_estrelas detectar_estrelas.c -lm
```

Se o comando for executado sem erros, um ficheiro executável chamado `detectar_estrelas` será criado.

#### 2.3. Execução

A execução é controlada pelo comando `mpirun`. O número de processos é definido pela flag `-np`. Devido à natureza dos ambientes virtualizados, os processos podem não conseguir comunicar entre si por padrão. Tente os seguintes comandos por ordem.

**\<N\>** deve ser o número total de processos (ex: 4) e **\<imagem.pgm\>** o nome do seu ficheiro (ex: `estrelas3.pgm`).

-----

**Tentativa 1: O Método `hostfile` (Mais Recomendado para VMs)**

Esta é a abordagem mais explícita e fiável. Ela informa ao MPI exatamente quantos "espaços" de execução estão disponíveis na sua máquina.

1.  **Crie um ficheiro de configuração** chamado `myhosts`:

    ```bash
    touch myhosts
    ```

2.  **Edite o ficheiro `myhosts`**. Para executar com **4 processos**, o conteúdo do ficheiro deve ser:

    ```
    localhost
    localhost
    localhost
    localhost
    ```

    *(Alternativamente, pode usar o formato `localhost:4` se a sua versão do MPICH o suportar).*

3.  **Execute** o programa apontando para este ficheiro:

    ```bash
    mpirun -np 4 --hostfile myhosts ./detectar_estrelas estrelas3.pgm
    ```

-----

**Tentativa 2: Forçar a Interface de Rede Local**

Se o método `hostfile` falhar, este comando força o MPI a usar a interface de rede de `loopback` (`lo`), que é puramente interna à máquina e não é afetada por firewalls externos.

```bash
mpirun -np 4 -iface lo ./detectar_estrelas estrelas3.pgm
```

-----

**Tentativa 3: O Comando Padrão do MPICH**

Este comando tenta especificar o `localhost` diretamente. Pode funcionar em algumas configurações.

```bash
mpirun -np 4 -host localhost ./detectar_estrelas estrelas3.pgm
```

-----

**Nota sobre Outras Versões do MPI:**
Se estivesse a usar uma versão diferente do MPI, como o **Open MPI**, o comando para forçar a execução seria diferente. O equivalente ao que tentámos resolver seria:
`mpirun -np 4 --oversubscribe ./detectar_estrelas estrelas3.pgm`
Isto é fornecido apenas para referência. Para o seu ambiente **MPICH**, use as soluções `hostfile` ou `-iface lo`.

## 3\. Estratégia e Algoritmos

*(Esta seção descreve a lógica interna do código `detectar_estrelas.c`)*

#### 3.1. Paralelismo e Divisão de Tarefas

O programa utiliza um modelo **Mestre-Escravo**. O Mestre (Rank 0) lê e distribui a imagem em fatias para os Escravos (Rank \> 0), que processam a sua fatia e devolvem a contagem de estrelas.

#### 3.2. Solução para o Problema da Fronteira

  * **Zonas de Sobreposição (Overlap Zones):** Cada escravo recebe uma fatia ligeiramente maior que a sua zona de responsabilidade, garantindo que objetos nas bordas sejam vistos por inteiro.
  * **Regra de Autoridade:** Uma estrela só é contada pelo escravo em cuja zona de responsabilidade o seu **centro de massa** se encontra. Isto evita a contagem dupla.

#### 3.3. Algoritmo de Deteção (Por Escravo)

1.  **Binarização:** Pixels acima de `BRILHO_MINIMO` são marcados como candidatos.
2.  **Deteção de Blobs:** Pixels candidatos conectados são agrupados em "blobs".
3.  **Análise Morfológica:**
      * Para cada blob, a **circularidade** é calculada usando a fórmula `(4 * PI * Área) / (Perímetro²)`.
      * Se a circularidade for alta (\>= `LIMIAR_DE_CIRCULARIDADE`), a forma é simples e contada como **1 estrela**.
      * Se for baixa, a forma é complexa e considerada um aglomerado.
4.  **Separação de Aglomerados:** Para blobs complexos, o programa conta os **picos de brilho (máximos locais)** internos para determinar o número real de estrelas sobrepostas.

## 4\. Parâmetros Configuráveis

  * `BRILHO_MINIMO`: (Padrão: 200) Limiar de brilho para considerar um pixel.
  * `LIMIAR_DE_CIRCULARIDADE`: (Padrão: 0.75) Nota de "perfeição" da forma para decidir se um blob é simples ou um aglomerado.
  * `OVERLAP`: (Padrão: 30) Tamanho da zona de sobreposição em pixels.

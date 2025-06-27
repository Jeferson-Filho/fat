#include "fat.h"
#include "ds.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

// Blocos reservados do sistema de arquivos simulados
#define SUPER 0  // Bloco 0: Superbloco (metadados gerais do sistema de arquivos)
#define DIR 1    // Bloco 1: Diretório (lista de arquivos)
#define TABLE 2  // Bloco 2 em diante: FAT (File Allocation Table, tabela de alocação de blocos)

#define SIZE 1024

// O superbloco
#define MAGIC_N           0xAC0010DE

// Estrutura do superbloco: guarda informações essenciais do sistema de arquivos
typedef struct {
	int magic;             // Valor mágico para validação do disco formatado
	int number_blocks;     // Quantidade total de blocos no disco
	int n_fat_blocks;      // Quantidade de blocos usados pela FAT
	char empty[BLOCK_SIZE - 3 * sizeof(int)]; // Espaço restante para preencher o bloco (padding)
} super;


super sb;

// Constantes e estrutura do diretório (lista de arquivos do sistema)
#define MAX_LETTERS 6
#define OK 1
#define NON_OK 0

// Cada entrada no diretório representa um arquivo
typedef struct{
	unsigned char used; // Marca se a entrada está em uso (OK ou NON_OK)
	char name[MAX_LETTERS+1];
	unsigned int length;
	unsigned int first; // Bloco inicial onde os dados do arquivo estão armazenados
} dir_item;

#define N_ITEMS (BLOCK_SIZE / sizeof(dir_item))
dir_item dir[N_ITEMS];

// Estados possíveis dos blocos na FAT
#define FREE 0   // Bloco livre, pode ser usado
#define EOFF 1   // Fim de arquivo (último bloco de um arquivo)
#define BUSY 2   // Bloco reservado ou em uso (sistema ou arquivo)

unsigned int *fat;

int mountState = 0; // Indica se o sistema de arquivos foi montado (1) ou não (0)

// Função utilitária: encontra o próximo bloco livre na FAT
int find_free_block() {
	for (int i = 0; i < sb.number_blocks; i++) {
		if (fat[i] == FREE) return i;
	}
	return -1;  // Nenhum bloco disponível
}

// A fat_format zera o disco, cria a base estrutural do sistema FAT 
//e garante que os blocos e diretórios estejam prontos para uso.
int fat_format() {
    if (mountState) return -1;

    int n_blocks = ds_size();
    if (n_blocks < 4) return -1;

    // Calcular quantos blocos a FAT ocupa
    int fat_entries_per_block = BLOCK_SIZE / sizeof(unsigned int);
    int n_fat_blocks = (n_blocks + fat_entries_per_block - 1) / fat_entries_per_block;

    // Superbloco
    sb.magic = MAGIC_N;
    sb.number_blocks = n_blocks;
    sb.n_fat_blocks = n_fat_blocks;
    memset(sb.empty, 0, sizeof(sb.empty));

    ds_write(SUPER, (char *)&sb);

    // Inicializa o diretório com entradas vazias
    for (int i = 0; i < N_ITEMS; i++) {
        dir[i].used = NON_OK;
        memset(dir[i].name, 0, MAX_LETTERS + 1);
        dir[i].length = 0;
        dir[i].first = (unsigned int)-1;
    }

	// Escreve o diretório no bloco 1 do disco
    ds_write(DIR, (char *)dir);

    // Aloca a FAT em memória (um vetor com n_blocks posições)
    fat = malloc(n_blocks * sizeof(unsigned int));
    if (!fat) return -1;

    for (int i = 0; i < n_blocks; i++) {
        fat[i] = FREE;
    }

    // Reserva os blocos que são usados pelo sistema (super, dir, e blocos da FAT)
    fat[SUPER] = BUSY;
    fat[DIR] = BUSY;
    for (int i = 0; i < n_fat_blocks; i++) {
        fat[TABLE + i] = BUSY;
    }

    // Grava a FAT no disco, bloco por bloco
    for (int i = 0; i < n_fat_blocks; i++) {
        ds_write(TABLE + i, (char *)(fat + i * fat_entries_per_block));
    }

    free(fat);
    return 0;
}

// fat_debug() serve para verificar se a simulação do sistema de arquivos está funcionando corretamente,
// permitindo o usuário visualizar o estado interno do sistema FAT.
void fat_debug() {
	printf("===== fat_debug() =====\n");

	// Lê o superbloco (bloco 0)
	ds_read(SUPER, (char *)&sb);
	printf("super bloco:\n");

	if (sb.magic == MAGIC_N) {
		printf("magico OK!\n");
	} else {
		printf("magic WRONG! (0x%x)\n", sb.magic);
		return;
	}

	printf("%d blocos\n", sb.number_blocks);
	printf("%d bloco%s fat\n", sb.n_fat_blocks, sb.n_fat_blocks > 1 ? "s" : "");

	// Lê o diretório (bloco 1) contendo as entradas de arquivos
	ds_read(DIR, (char *)dir);

	// Carregar FAT da imagem do disco
	int total_entries = sb.number_blocks;
	fat = malloc(total_entries * sizeof(unsigned int));
	if (!fat) {
		fprintf(stderr, "erro ao alocar memória para a FAT\n");
		return;
	}

	// Lê os blocos da FAT (blocos 2 em diante)
	int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);
	for (int i = 0; i < sb.n_fat_blocks; i++) {
		ds_read(TABLE + i, (char *)(fat + i * entries_per_block));
	}

	// Imprimir arquivos válidos
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == OK) {
			printf("\nArquivo \"%s\":\n", dir[i].name);
			printf("tamanho: %u bytes\n", dir[i].length);

			if (dir[i].first < sb.number_blocks) {
				printf("Blocks: ");
				unsigned int b = dir[i].first;
				while (1) {
					printf("%u", b);

					// Se encontrou o fim da cadeia, para
					if (fat[b] == EOFF) break;

					// Proteção extra ;)
					if (fat[b] >= sb.number_blocks || fat[b] == b) {
						printf(" [erro na FAT]");
						break;
					}

					printf(" ");
					b = fat[b];
				}
				printf("\n");
			} else {
				printf("Blocks:\n");
			}
		}
	}

	free(fat);
}

// Carrega os metadados do disco para a RAM
// Tudo isso fica em memória para operações futuras
int fat_mount() {
	printf("===== fat_mount() =====\n");

	// Impede montar mais de uma vez
	if (mountState) return -1;

	// Ler superbloco
	ds_read(SUPER, (char *)&sb);
	if (sb.magic != MAGIC_N) return -1;

	int n_blocks = sb.number_blocks;
	int n_fat_blocks = sb.n_fat_blocks;

	// Ler diretório
	ds_read(DIR, (char *)dir);

	// Aloca memória para carregar a FAT
	fat = malloc(n_blocks * sizeof(unsigned int));
	if (!fat) return -1;

	// Lê os blocos da FAT
	int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);
	for (int i = 0; i < n_fat_blocks; i++) {
		ds_read(TABLE + i, (char *)(fat + i * entries_per_block));
	}

	// Marca que o sistema está montado
	mountState = 1;
	return 0;
}

//  fat_create() é responsável por registrar um novo arquivo na FAT
// o conteúdo é adicionado depois via fat_write.
int fat_create(char *name) {
	printf("===== fat_create() =====\n");

	// Verifica se o sistema está montado
	if (!mountState) return -1;

	// Validação do nome
	if (strlen(name) > MAX_LETTERS) return -1;

	// Verifica se o nome já existe
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == OK && strcmp(dir[i].name, name) == 0) {
			return -1;
		}
	}

	// Procura entrada livre
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == NON_OK) {
			dir[i].used = OK;
			strncpy(dir[i].name, name, MAX_LETTERS);
			dir[i].name[MAX_LETTERS] = '\0'; // garante terminação
			dir[i].length = 0;
			dir[i].first = (unsigned int)-1; // marca como "sem bloco"

			// Salva no disco
			ds_write(DIR, (char *)dir);
			return 0;
		}
	}

	// Diretório cheio
	return -1;
}

// fat_delete() remove logicamente um arquivo da FAT
int fat_delete(char *name) {
	if (!mountState) return -1;

	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == OK && strcmp(dir[i].name, name) == 0) {
			// Liberar blocos na FAT
			unsigned int b = dir[i].first;

			if (b < sb.number_blocks) {
				while (1) {
					unsigned int prox = fat[b];
					fat[b] = FREE;
					if (prox == EOFF) break;

					// Proteção contra erro de FAT
					if (prox >= sb.number_blocks || prox == b) break;

					b = prox;
				}
			}

			// Liberar entrada do diretório
			dir[i].used = NON_OK;
			memset(dir[i].name, 0, MAX_LETTERS + 1);
			dir[i].length = 0;
			dir[i].first = (unsigned int)-1;

			// Escrever mudanças no disco
			ds_write(DIR, (char *)dir);

			// Escrever a FAT atualizada
			int entries_per_block = BLOCK_SIZE / sizeof(unsigned int);
			for (int j = 0; j < sb.n_fat_blocks; j++) {
				ds_write(TABLE + j, (char *)(fat + j * entries_per_block));
			}

			return 0;
		}
	}

	return -1; // arquivo não encontrado
}


// retorna o tamanho em bytes de um arquivo 
int fat_getsize(char *name) {
	printf("===== fat_getsize() =====\n");
	if (!mountState) return -1;

	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == OK && strcmp(dir[i].name, name) == 0) {
			return dir[i].length;
		}
	}

	return -1; // não encontrado
}


// fat_read lê dados de um arquivo armazenado no FAT
//Retorna a quantidade de caracteres lidos
int fat_read(char *name, char *buff, int length, int offset) {
	printf("===== fat_read() =====\n");
	if (!mountState || !buff || length <= 0 || offset < 0) return -1;

	// 1. Encontrar o arquivo no diretório
	for (int i = 0; i < N_ITEMS; i++) {
		if (dir[i].used == OK && strcmp(dir[i].name, name) == 0) {
			if ((unsigned int)offset >= dir[i].length) return 0; // além do fim

			int bytes_to_read = length;
			if ((unsigned int)(offset + length) > dir[i].length) {
				bytes_to_read = dir[i].length - offset;
			}

			int block_index = dir[i].first;
			int skipped = 0;
			char block[BLOCK_SIZE];

			// 2. Avançar até o bloco inicial do offset
			int inner_offset = offset;
			while (inner_offset >= BLOCK_SIZE && block_index < sb.number_blocks) {
				inner_offset -= BLOCK_SIZE;
				block_index = fat[block_index];
				if (block_index == EOFF || block_index >= sb.number_blocks) return -1;
			}

			// 3. Ler e copiar os dados
			int copied = 0;
			while (copied < bytes_to_read && block_index < sb.number_blocks) {
				ds_read(block_index, block);

				int chunk = BLOCK_SIZE - inner_offset;
				if (chunk > (bytes_to_read - copied)) {
					chunk = bytes_to_read - copied;
				}

				memcpy(buff + copied, block + inner_offset, chunk);
				copied += chunk;

				if (copied >= bytes_to_read) break;

				block_index = fat[block_index];
				if (block_index == EOFF || block_index >= sb.number_blocks) break;

				inner_offset = 0; // após o primeiro bloco
			}

			return copied;
		}
	}

	return -1; // arquivo não encontrado
}

// fat_write implementa a escrita de dados em um arquivo da FAT
//Retorna a quantidade de caracteres escritos
int fat_write(char *name, const char *buff, int length, int offset) {
    if (!mountState || !buff || length <= 0 || offset < 0) return -1;

    for (int i = 0; i < N_ITEMS; i++) {
        if (dir[i].used == OK && strcmp(dir[i].name, name) == 0) {
            int written = 0;
            int inner_offset = offset;
            int b = dir[i].first;
            int prev = -1;

            // Arquivo vazio: alocar primeiro bloco
            if (b >= sb.number_blocks || b == (unsigned int)-1) {
                b = find_free_block();
                if (b < 0) return written;
                fat[b] = EOFF;
                dir[i].first = b;
            }

            // Avançar até o bloco correspondente ao offset
            while (inner_offset >= BLOCK_SIZE) {
                inner_offset -= BLOCK_SIZE;
                prev = b;
                if (fat[b] == EOFF) {
                    int novo = find_free_block();
                    if (novo < 0) return written;
                    fat[b] = novo;
                    fat[novo] = EOFF;
                }
                b = fat[b];
            }

            // Escrever os dados
            char block[BLOCK_SIZE];
            while (written < length) {
                ds_read(b, block);

                int chunk = BLOCK_SIZE - inner_offset;
                if (chunk > (length - written)) {
                    chunk = length - written;
                }

                memcpy(block + inner_offset, buff + written, chunk);
                ds_write(b, block);

                written += chunk;
                inner_offset = 0;

                if (written >= length) break;

                if (fat[b] == EOFF) {
                    int novo = find_free_block();
                    if (novo < 0) break;
                    fat[b] = novo;
                    fat[novo] = EOFF;
                }

                b = fat[b];
            }

            // Atualiza tamanho se necessário
            if ((unsigned int)(offset + written) > dir[i].length) {
                dir[i].length = offset + written;
            }

            // Atualizar diretório
            ds_write(DIR, (char *)dir);

            // Atualizar FAT no disco
            int per_block = BLOCK_SIZE / sizeof(unsigned int);
            for (int j = 0; j < sb.n_fat_blocks; j++) {
                ds_write(TABLE + j, (char *)(fat + j * per_block));
            }

            return written;
        }
    }

    return -1;
}

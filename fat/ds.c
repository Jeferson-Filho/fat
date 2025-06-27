#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ds.h"

static int number_blocks = 0;   // Quantidade total de blocos do disco simulado
static int number_reads = 0;    // Contador de leituras feitas no disco
static int number_writes = 0;   // Contador de escritas feitas no disco
static FILE *disk;              // Ponteiro para o arquivo que simula o disco

// Retorna o tamanho do disco simulado em número de blocos.
int ds_size()
{
	return number_blocks;
}

// Inicializar o disco simulado, criando um arquivo com o tamanho necessário para representar os blocos.
// Este arquivo será manipulado como se fosse um disco real, com blocos de tamanho fixo (BLOCK_SIZE).
int ds_init( const char *filename, int n )
{
	disk = fopen(filename,"r+");
	if(!disk) disk = fopen(filename,"w+");
	if(!disk) return 0;

	ftruncate(fileno(disk),n * BLOCK_SIZE);

	number_blocks = n;
	number_reads = 0;
	number_writes = 0;

	return 1;
}

// Verificar se os parâmetros de leitura/escrita são válidos:
// Número de bloco não pode ser negativo ou ultrapassar o tamanho do disco.
// O ponteiro do buffer precisa estar válido.
static void check( int number, const void *buff )
{
	if(number<0) {
		printf("ERROR: blocknum (%d) is negative!\n",number);
		abort();
	}

	if(number>=number_blocks) {
		printf("ERROR: blocknum (%d) is too big!\n",number);
		abort();
	}

	if(!buff) {
		printf("ERROR: null data pointer!\n");
		abort();
	}
}

// Ler um bloco específico do "disco" (arquivo) e copiar seu conteúdo para o buffer.
// Usado por sistemas de arquivos simulados (ex: FAT) para acessar dados.
void ds_read( int number, char *buff )
{
	int x;
	check(number,buff);
	fseek(disk,number*BLOCK_SIZE,SEEK_SET);
	x = fread(buff,BLOCK_SIZE,1,disk);
	if(x==1) {
		number_reads++;
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1);
	}
}

// Gravar um bloco completo no "disco", sobrescrevendo o que havia antes.
// Essencial para simular o comportamento de escrita em sistemas de arquivos.
void ds_write( int number, const char *buff )
{
	int x;
	check(number,buff);
	fseek(disk,number*BLOCK_SIZE,SEEK_SET);
	x = fwrite(buff,BLOCK_SIZE,1,disk);
	if(x==1) {
		number_writes++;
	} else {
		printf("disk simulation failed\n");
		perror("ds");
		exit(1);
	}
}

// Fechar o arquivo do disco simulado e exibir o número total de leituras e escritas feitas.
void ds_close()
{
	printf("%d reads\n",number_reads);
	printf("%d writes\n",number_writes);
	fclose(disk);
}


<a id="readme-top"></a>

<div align="center">
  <h1 align="center">FAT</h1>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributors">Contributors</a></li>
    <li><a href="#contact">Contact</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->
## About The Project
TBD

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Built With

![C][C-shield]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- GETTING STARTED -->
## Getting Started
### Installation

### 1. Implementação de Formatação, Montagem e Debug
- [ ] Implementar `fat_format()`
  - [ ] Verificar se `mountState == 1` (se já montado, retornar erro)
  - [ ] Preencher `sb.magic = MAGIC_N`
  - [ ] Obter `sb.number_blocks = ds_size()`
  - [ ] Calcular `sb.n_fat_blocks = ceil((N × sizeof(unsigned int)) / BLOCK_SIZE)`
  - [ ] Zerar preenchimento de superbloco e gravar em bloco 0 (`ds_write(0, buffer_sb)`)
  - [ ] Inicializar diretório vazio (todos `dir_item.used = 0`) e gravar em bloco 1
  - [ ] Alocar `fat = malloc(N × sizeof(unsigned int))`, marcar todas entradas como `FREE (0)`
  - [ ] Gravar área da FAT em blocos a partir de 2 (`ds_write(2 + i, buffer_fat)`)
  - [ ] Definir `mountState = 0` e retornar 0
- [ ] Implementar `fat_mount()`
  - [ ] Ler superbloco de bloco 0, copiar para `sb`
  - [ ] Verificar `sb.magic == MAGIC_N`; se não, erro
  - [ ] Verificar `sb.number_blocks == ds_size()`; se não, erro
  - [ ] Calcular `expected_nfat = ceil((N × 4) / 4096)` e comparar com `sb.n_fat_blocks`
  - [ ] Alocar `fat[N]` e ler FAT de blocos 2…(2+sb.n_fat_blocks–1) para `fat[]`
  - [ ] Ler diretório de bloco 1 para `dir[N_ITEMS]`
  - [ ] Definir `mountState = 1` e retornar 0
- [ ] Implementar `fat_debug()`
  - [ ] Se `mountState == 0`, ler superbloco, FAT (em buffer temporário) e diretório diretamente do disco
  - [ ] Exibir:
    - `superblock:`
      - `magic is ok` ou `magic is wrong`
      - `<number_blocks> blocks`
      - `<n_fat_blocks>  block fat`
    - Para cada `dir_item.used == OK`:
      - `File "<nome>":`
      - `    size: <length> bytes`
      - `    Blocks: <lista de blocos seguindo cadeia FAT>`
  - [ ] Se `mountState == 1`, usar `sb`, `fat[]`, `dir[]` em RAM para exibir mesmas informações


### 2. Operações de Diretório: Criar e Deletar Arquivos
- [ ] Implementar `fat_create(char *name)`
  - [ ] Verificar `mountState == 1`; se não, erro
  - [ ] Verificar que `strlen(name) ≤ 6`; se não, erro
  - [ ] Verificar se já existe arquivo com mesmo nome em `dir[]`; se sim, erro
  - [ ] Buscar primeira `dir_item.used == NON_OK`
    - [ ] Se não encontrar, erro (“diretório cheio”)
  - [ ] Preencher `dir[i]`:
    - `used = OK`
    - `strncpy(name)`
    - `length = 0`
    - `first = EOFF` (arquivo vazio)
  - [ ] Gravar diretório atualizado em bloco 1 (`ds_write(1, buffer_dir)`)
  - [ ] Retornar 0
- [ ] Implementar `fat_delete(char *name)`
  - [ ] Verificar `mountState == 1`; se não, erro
  - [ ] Buscar arquivo em `dir[]`; se não encontrar, erro
  - [ ] Se `dir[i].first != EOFF`, liberar blocos:
    - [ ] `cur = dir[i].first`
    - [ ] Enquanto `cur != EOFF`:
      - `next = fat[cur]`
      - `fat[cur] = FREE`
      - Se `next == EOFF`, sair; senão, `cur = next`
  - [ ] Limpar entrada de diretório:
    - `used = NON_OK`
    - `name[0] = '\0'`
    - `length = 0`
    - `first = 0`
  - [ ] Gravar FAT (blocos 2…) e diretório (bloco 1) em disco
  - [ ] Retornar 0


### 3. Função de Tamanho de Arquivo
- [ ] Implementar `fat_getsize(char *name)`
  - [ ] Verificar `mountState == 1`; se não, erro
  - [ ] Buscar `dir[i]` com `name`; se não encontrar, erro
  - [ ] Retornar `dir[i].length`


### 4. Leitura de Arquivo (fat_read)
- [ ] Implementar `fat_read(char *name, char *buff, int length, int offset)`
  - [ ] Verificar `mountState == 1`; se não, erro
  - [ ] Buscar `dir[i]`; se não achar, erro
  - [ ] `filesize = dir[i].length`
  - [ ] Se `offset >= filesize`, retornar 0
  - [ ] Ajustar `length = min(length, filesize - offset)`
  - [ ] Calcular:
    - `block_idx = offset / BLOCK_SIZE`
    - `offset_in_block = offset % BLOCK_SIZE`
  - [ ] Percorrer cadeia FAT até obter bloco inicial (`for k in 0..block_idx–1: cur = fat[cur]`)
  - [ ] Loop de leitura:
    - [ ] Ler bloco `cur` via `ds_read(cur, block_buf)`
    - [ ] `start = (primeira iteração ? offset_in_block : 0)`
    - [ ] `can_read = min(bytes_restantes, BLOCK_SIZE - start)`
    - [ ] `memcpy(buff + total_lido, block_buf + start, can_read)`
    - [ ] Atualizar `bytes_restantes`, `total_lido`
    - [ ] Se `bytes_restantes == 0`, sair
    - [ ] `next = fat[cur]`; se `next == EOFF`, sair; senão `cur = next`
  - [ ] Retornar `total_lido`


### 5. Escrita em Arquivo (fat_write)
- [ ] Implementar `fat_write(char *name, const char *buff, int length, int offset)`
  - [ ] Verificar `mountState == 1`; se não, erro
  - [ ] Buscar `dir[i]`; se não achar, erro
  - [ ] `old_size = dir[i].length`
  - [ ] Se `offset > old_size`, retornar erro (pode estender para preencher buracos com zeros)
  - [ ] Contar blocos atuais do arquivo:
    - `cur = dir[i].first`; enquanto `cur != EOFF`, `n_blocos_ocupados++`, `cur = fat[cur]`
  - [ ] Calcular:
    - `start_block_idx = offset / BLOCK_SIZE`
    - `offset_in_start = offset % BLOCK_SIZE`
  - [ ] Se `old_size == 0` (arquivo vazio):
    - [ ] `new_block = find_free_block()`; se `-1`, sem espaço
    - [ ] `dir[i].first = new_block`
    - [ ] `fat[new_block] = EOFF`
    - [ ] `cur = new_block`, `n_blocos_ocupados = 1`
  - [ ] Senão, percorrer FAT até bloco de índice `start_block_idx` (ou apontar para último bloco se `start_block_idx == n_blocos_ocupados`)
  - [ ] Loop de escrita:
    - [ ] Se parcial ou bloco não vazio, `ds_read(cur, block_buf)`, senão `memset(block_buf, 0, BLOCK_SIZE)`
    - [ ] `start = (primeira iteração ? offset_in_start : 0)`
    - [ ] `can_write = min(bytes_restantes, BLOCK_SIZE - start)`
    - [ ] `memcpy(block_buf + start, buff + total_escrito, can_write)`
    - [ ] `ds_write(cur, block_buf)`
    - [ ] Atualizar `bytes_restantes`, `total_escrito`, `offset_in_start = 0`
    - [ ] Se ainda restam bytes:
      - [ ] Se `fat[cur] != EOFF`, `cur = fat[cur]`
      - [ ] Senão, alocar novo bloco:
        - `new_block = find_free_block()`; se `-1`, gravar FAT/diretório e retornar `total_escrito`
        - `fat[cur] = new_block`, `fat[new_block] = EOFF`, `cur = new_block`, `n_blocos_ocupados++`
  - [ ] Após loop, atualizar `dir[i].length = max(old_size, offset + total_escrito)`
  - [ ] Gravar FAT (blocos 2…) e diretório (bloco 1)
  - [ ] Retornar `total_escrito`

- [ ] Criar função auxiliar estática `find_free_block()`:
  - [ ] Percorre `j` de `(2 + sb.n_fat_blocks)` até `(sb.number_blocks - 1)`
  - [ ] Retorna primeiro `j` tal que `fat[j] == FREE`; se não, retorna `(unsigned int)-1`

- [ ] Criar funções auxiliares estáticas:
  - [ ] `static void write_fat_to_disk()` — copia `fat[]` em blocos físicos da FAT
  - [ ] `static void write_dir_to_disk()` — copia `dir[]` em bloco 1


### 6. Testes e Validação
- [ ] Recompilar com `make clean && make dev`
- [ ] Testar `formatar` em disco vazio (`make img`, depois `formatar`)
  - [ ] Verificar via `hexdump -C disco.img` se superbloco foi gravado corretamente
- [ ] Testar `montar` e `depurar` (sem arquivos)
- [ ] Testar `criar <nome>` para vários nomes (até 6 caracteres)
  - [ ] Verificar entrada no diretório via `depurar`
- [ ] Testar `deletar <nome>` e confirmar remoção via `depurar`
- [ ] Testar `medir <nome>` (deve retornar 0 para arquivo vazio)
- [ ] Testar escrita < 4096 bytes:
  - [ ] `escrever <nome> <offset=0> <length<4096>` e `ler <nome>` para verificar conteúdo
  - [ ] Confirmar que somente 1 bloco foi alocado (via `depurar`)
- [ ] Testar escrita > 4096 bytes:
  - [ ] `escrever <nome> <offset=0> <length=5000>` → 2 blocos alocados
  - [ ] `ler <nome> <offset=2000> <length=3000>` e comparar com dados originais
- [ ] Testar sobrescrita no meio do arquivo:
  - [ ] `escrever <nome> <offset=2000> <length=1000>` sobre arquivo existente
  - [ ] Verificar que apenas bytes corretos foram alterados
- [ ] Testar `deletar` em arquivo grande e confirmar libertação de blocos
- [ ] Tentar criar arquivo com nome > 6 caracteres (deve falhar)
- [ ] Tentar criar arquivo duplicado (deve falhar)
- [ ] Preencher disco completamente com múltiplos arquivos grandes
  - [ ] Verificar `escrever` retorna valor < `length` ao acabar espaço
- [ ] Validar estatísticas de `ds_close()` (nº de leituras e escritas) para garantir coerência


### 7. Documentação e Organização Final
- [ ] Adicionar comentários em cada função explicando parâmetros, retornos e efeitos
- [ ] Verificar vazamentos de memória (uso de `malloc`/`free`)
- [ ] Ajustar `Makefile` se necessário para incluir flags de compilação adicionais (ex.: `-Wall -Wextra`)
- [ ] Organizar código em seções lógicas (funções auxiliares estáticas, depois APIs FAT)
- [ ] Testar novamente todos casos após ajustes finais
- [ ] Comitar versão estável no controle de versão com mensagem clara (ex.: “Implementação completa do sistema de arquivos FAT”)


<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ROADMAP -->
## Roadmap

TBD

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTRIBUTING -->
## Contributors

<a href="https://github.com/Jeferson-Filho/ChestXRayClassification/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=Jeferson-Filho/ChestXRayClassification" alt="contrib.rocks image" />
</a>

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- CONTACT -->
## Contact

Gabriella <br>
[![LinkedIn][linkedin-shield]][gabriella-linkedin-url]

Jeferson Filho <br>
[![LinkedIn][linkedin-shield]][jeferson-linkedin-url]

Murilo Augusto <br>
[![LinkedIn][linkedin-shield]][murilo-linkedin-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN SHIELD & IMAGES -->
[C-shield]: https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white
[linkedin-shield]: https://img.shields.io/badge/-LinkedIn-black.svg?style=for-the-badge&logo=linkedin&colorB=555

<!-- MARKDOWN IMAGES -->
[gabriella-linkedin-url]: https://www.linkedin.com/in/gabriella-alves-de-oliveira-9267271b8/
[jeferson-linkedin-url]: https://www.linkedin.com/in/jdietrichfho/
[murilo-linkedin-url]: https://www.linkedin.com/in/murilo-venturato-7450a1207/
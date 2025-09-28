# File_System

A ideia do projeto partiu da criação de um **ambiente inspirado no sistema de arquivos FAT**, que foi objeto de estudo em sala de aula.  
A partir desse conceito, foi implementado um "mini" sistema de arquivos simples em **C**, com suporte a operações básicas de gerenciamento de arquivos.

---

## Como compilar

### Pré-requisitos
- Compilador **C** (ex: `gcc`)  
- Ambiente **Linux/Unix** (ou compatível)

---

### Como executar

Durante o andamento do projeto, foi utilizado **CMake** para compilar e organizar os arquivos.  
No entanto, para a entrega final, o uso do CMake foi deixado de lado e o repositório está configurado para rodar diretamente o **binário já compilado**.

## Execução
Basta rodar o binário disponibilizado no repositório:

```bash
./FURGfs3
```
## Comandos suportados

O sistema de arquivos simula um terminal com os seguintes comandos:
```bash
touch <nome_arquivo> → cria um arquivo vazio dentro do FS.

mkdir <nome_diretorio> → cria um diretório (inclusive dentro de outros diretórios).

cd <nome_diretorio> → entra em um diretório.

cd .. → retorna ao diretório anterior.

ls → lista os arquivos e diretórios no diretório atual.

space → mostra espaço total, espaço usado e espaço livre (em bytes).

rm <nome> → remove arquivos ou diretórios.

mv <arquivo> <novo_nome> → renomeia arquivos ou diretórios.

copy_in <caminho_absoluto> <nome_no_FS> → copia um arquivo do seu PC para dentro do FS.

copy_out <arquivo_FS> <caminho_diretorio_PC> → copia um arquivo do FS para o seu PC.

exit → encerra o programa.
```

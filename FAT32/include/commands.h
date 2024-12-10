#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>
#include "fat32.h"  // Atualizado para incluir o arquivo de cabeçalho FAT32

/*
 * Esta struct encapsula o resultado de find(), carregando informações sobre a
 * busca consigo.
 */
struct far_dir_searchres
{
    struct fat32_dir fdir; // Atualizado para usar a estrutura do diretório FAT32
    bool          found;   // Encontrou algo?
    int             idx;   // Índice relativo ao diretório de busca
};

/*
 * Esta struct encapsula o resultado de fat32_find_free_cluster()
 *
 */
struct fat32_newcluster_info
{
    uint32_t cluster; // Alterado para 32 bits
    uint32_t address;
};

/* listar arquivos no fat32_bpb */
struct fat32_dir *ls(FILE *, struct fat32_bpb *);

/* mover um arquivo da fonte ao destino */
void mv(FILE* fp, char* source, char* dest, struct fat32_bpb* bpb);

/* deletar o arquivo do diretório FAT32 */
void rm(FILE* fp, char* filename, struct fat32_bpb* bpb);

/* copiar o arquivo para o diretório FAT32 */
void cp(FILE* fp, char* source, char* dest, struct fat32_bpb* bpb);

/*
 * Esta função escreve no terminal os conteúdos de um arquivo.
 */
void cat(FILE* fp, char* filename, struct fat32_bpb* bpb);

/* função auxiliar: encontrar um filename específico no fat_dir */
struct far_dir_searchres find_in_root(struct fat32_dir *dirs, char *filename, struct fat32_bpb *bpb);

/* procurar cluster vazio */
struct fat32_newcluster_info fat32_find_free_cluster(FILE* fp, struct fat32_bpb* bpb);

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

/* Funções de escrita (comentadas para referência futura) */
/* int write_dir (FILE *, char *, struct fat_dir *); */
/* int write_data(FILE *, char *, struct fat_dir *, struct fat32_bpb *); */

#endif
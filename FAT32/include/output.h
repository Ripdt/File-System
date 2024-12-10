#ifndef OUTPUT_H
#define OUTPUT_H

#include "fat32.h" // Alterado para incluir o header do FAT32

/*
 * Exibe os arquivos presentes no diretório, agora considerando a estrutura de FAT32
 */
void show_files(struct fat32_dir *dir_entries, struct fat32_bpb *bpb);

/*
 * Exibe as informações detalhadas da estrutura do BPB, com novos campos do FAT32
 */
void verbose(struct fat32_bpb *bpb);

#endif
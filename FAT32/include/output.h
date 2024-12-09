#ifndef OUTPUT_H
#define OUTPUT_H

#include "fat16.h"

/*
 * Exibe os arquivos presentes no diretório, agora considerando a estrutura de FAT32
 */
void show_files(struct fat_dir *dir_entries, struct fat_bpb *bpb);

/*
 * Exibe as informações detalhadas da estrutura do BPB, com novos campos do FAT32
 */
void verbose(struct fat_bpb *bpb);

#endif

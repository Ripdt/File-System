#ifndef SUPPORT_H
#define SUPPORT_H

#include <stdbool.h>
#include <string.h>  // Adicionado para funções strlen e strcpy
#include "fat32.h"  // Alterado para incluir a header do FAT32

#define FAT32_MAX_LFN_SIZE 255

bool cstr_to_fat32_lfn(char *filename, char output[FAT32_MAX_LFN_SIZE]);

#endif
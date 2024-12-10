#ifndef SUPPORT_H
#define SUPPORT_H

#include <stdbool.h>
#include "fat32.h"  // Alterado para incluir o header do FAT32

#define FAT32_MAX_LFN_SIZE 255

bool cstr_to_fat16wnull(char *filename, char output[FAT16STR_SIZE_WNULL]);

bool cstr_to_fat32_lfn(char *filename, char output[FAT32_MAX_LFN_SIZE]) {
    // A função pode ser implementada para lidar com os nomes longos de arquivos no formato adequado para FAT32
    if (strlen(filename) > FAT32_MAX_LFN_SIZE) {
        return false;  // Nome muito longo para FAT32
    }
    
    strcpy(output, filename);  // Copia o nome do arquivo para o buffer de saída
    return true;
}

#endif
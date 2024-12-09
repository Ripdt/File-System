#include "support.h"
#include <stdio.h>
#include <string.h>


#include <ctype.h>

#include <stdbool.h>
#include "fat16.h"


/* Manipulate the path to lead com name, extensions and special characters */
bool cstr_to_fat16wnull(char *filename, char output[FAT16STR_SIZE_WNULL])
{


	char* strptr = filename;
	char* dot;
	dot = strchr(filename, '.');

	if (dot == NULL) return true;

	int i;
	for(i=0; strptr != dot; strptr++, i++){
		if(i==8)
			break;
		output[i] = *strptr;
	}

	int trail = 8 - i;
	for(; trail > 0; trail--, i++){
		output[i] = ' ';
	}

	strptr = dot;
	strptr++;
	for(i=8; i < 11; strptr++, i++){
		output[i] = *strptr;
	}

	output[11] = '\0';
	for(i = 0; output[i] != '\0'; i++){
		output[i] = toupper(output[i]);
	}

	return false;
}

/* Manipula o nome longo de arquivos para o formato FAT32 LFN */
bool cstr_to_fat32_lfn(char *filename, char output[FAT32_MAX_LFN_SIZE])
{
    // A função pode ser implementada para lidar com os nomes longos de arquivos no formato adequado para FAT32
    if (strlen(filename) > FAT32_MAX_LFN_SIZE) {
        return false;  // Nome muito longo para FAT32
    }
    
    // Copia o nome do arquivo para o buffer de saída
    strcpy(output, filename);  
    
    // Converte o nome para maiúsculas, conforme o padrão do FAT32
    for (int i = 0; output[i] != '\0'; i++) {
        output[i] = toupper(output[i]);
    }

    return true;
}
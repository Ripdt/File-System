#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "fat32.h"  // Alterado para incluir o header do FAT32
#include "commands.h"
#include "output.h"
#include "support.h"

/* Exibe a ajuda de uso */
void usage(char *executable)
{
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "\t%s -h | --help for help\n", executable);
    fprintf(stdout, "\t%s ls <fat32-img> - List files from the FAT32 image\n", executable);
    fprintf(stdout, "\t%s cp <path> <dest> <fat32-img> - Copy files from the image path to local dest.\n", executable);
    fprintf(stdout, "\t%s mv <path> <dest> <fat32-img> - Move files from the path to the FAT32 path\n", executable);
    fprintf(stdout, "\t%s rm <path> <file> <fat32-img> - Remove files from the path to the FAT32 path\n", executable);
    fprintf(stdout, "\t%s cat <path> <fat32-img> - Display the content of the file from the FAT32 path\n", executable);
    fprintf(stdout, "\t%s create <filename> <fat32-img> - Create a file in the FAT32 image\n", executable);
    fprintf(stdout, "\t%s write <filename> <data> <fat32-img> - Write data to a file in the FAT32 image\n", executable);
    fprintf(stdout, "\n");
    fprintf(stdout, "\tfat32-img needs to be a valid FAT32 image.\n\n");
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, getenv("LANG"));

    if (argc <= 1)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        usage(argv[0]);
        exit(EXIT_SUCCESS);
    }

    else if (argc >= 3)
    {
        FILE *fp = fopen(argv[argc - 1], "rb+");

        if (!fp)
        {
            fprintf(stdout, "Could not open file %s\n", argv[argc - 1]);
            exit(1);
        }

        struct fat32_bpb bpb;
        rfat(fp, &bpb);  // Leitura do BPB, ajustada para FAT32

        // Verificar e definir root_entry_count se estiver incorreto
        if (bpb.root_entry_count == 0)
        {
            bpb.root_entry_count = 512; // Definir valor padrão, ajuste conforme necessário
        }

        verbose(&bpb);  // Exibe as informações do BPB

        char *command = argv[1];

        if (strcmp(command, "ls") == 0)
        {
            struct fat32_dir *dirs = ls(fp, &bpb);  // Listagem de arquivos, ajustada para FAT32
            show_files(dirs, &bpb);  // Exibe os arquivos, ajustada para FAT32
        }

        else if (strcmp(command, "cp") == 0)
        {
            cp(fp, argv[2], argv[3], &bpb);  // Copia arquivos, ajustada para FAT32
            fclose(fp);
        }

        else if (strcmp(command, "mv") == 0)
        {
            mv(fp, argv[2], argv[3], &bpb);  // Move arquivos, ajustada para FAT32
            fclose(fp);
        }

        else if (strcmp(command, "rm") == 0)
        {
            rm(fp, argv[2], &bpb);  // Remove arquivos, ajustada para FAT32
            fclose(fp);
        }

        else if (strcmp(command, "cat") == 0)
        {
            cat(fp, argv[2], &bpb);  // Exibe conteúdo de arquivo, ajustada para FAT32
            fclose(fp);
        }

        else if (strcmp(command, "create") == 0)
        {
            create(fp, argv[2], &bpb);  // Cria um novo arquivo, ajustada para FAT32
            fclose(fp);
        }

        else if (strcmp(command, "write") == 0)
        {
            if (argc != 5) {
                fprintf(stderr, "Usage: %s write <filename> <data> <fat32-img>\n", argv[0]);
                return EXIT_FAILURE;
            }
            write_to_file(fp, argv[2], argv[3], &bpb);  // Escreve dados em um arquivo, ajustada para FAT32
            fclose(fp);
        }

        else
        {
            usage(argv[0]);
            fclose(fp);
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
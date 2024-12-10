#include <unistd.h>
#include <string.h>
#include <strings.h> // Incluindo para usar strncasecmp
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <ctype.h>
#include "commands.h"
#include "fat32.h" // Alterado para incluir a header do FAT32
#include "support.h"

#include <errno.h>
#include <err.h>
#include <error.h>
#include <assert.h>

#include <sys/types.h>

#include "commands.h"
#include "support.h"
#include "fat32.h"
#include <stdio.h>

#include "commands.h"
#include "support.h"
#include "fat32.h"
#include <stdio.h>

struct fat32_newcluster_info fat32_find_free_cluster(FILE* fp, struct fat32_bpb* bpb) {
    struct fat32_newcluster_info info = {0, 0};

    uint32_t fat_start = bpb->reserved_sect * bpb->bytes_p_sect;
    uint32_t fat_size = bpb->sector_p_clust * bpb->bytes_p_sect; // Ajuste para usar membros corretos

    // Percorrer a tabela FAT para encontrar um cluster livre
    for (uint32_t offset = 0; offset < fat_size; offset += 4) {
        uint32_t cluster_entry;
        if (fseek(fp, fat_start + offset, SEEK_SET) != 0) {
            perror("Erro ao posicionar o ponteiro na tabela FAT");
            return info;
        }
        if (fread(&cluster_entry, sizeof(cluster_entry), 1, fp) != 1) {
            perror("Erro ao ler a tabela FAT");
            return info;
        }

        if (cluster_entry == 0) {  // Cluster livre encontrado
            uint32_t cluster_num = offset / 4;
            info.cluster = cluster_num;
            info.address = fat_start + offset;

            // Atualizar a tabela FAT para marcar o cluster como utilizado
            uint32_t end_of_cluster_chain = 0x0FFFFFFF; // End of cluster chain marker
            if (fseek(fp, fat_start + offset, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro na tabela FAT para atualização");
                return info;
            }
            if (fwrite(&end_of_cluster_chain, sizeof(end_of_cluster_chain), 1, fp) != 1) {
                perror("Erro ao atualizar a tabela FAT");
                return info;
            }

            return info;
        }
    }

    fprintf(stderr, "Nenhum cluster livre disponível.\n");
    return info;
}

void write_to_file(FILE* fp, char* filename, char* data, struct fat32_bpb* bpb) {
    char fat32_filename[11];
    memset(fat32_filename, ' ', 11); // Preencher com espaços
    int len = strlen(filename);
    for (int i = 0; i < 11 && i < len; i++) {
        fat32_filename[i] = toupper(filename[i]);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    printf("Nome convertido para FAT32: '%.*s'\n", 11, fat32_filename);

    // Exibir todas as entradas do diretório para depuração
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        printf("Entrada do diretório %u: nome='%.*s'\n", i, 11, root[i].name);
    }

    // Procurar o arquivo no diretório
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        printf("Verificando entrada do diretório %u: nome='%.*s', comparando com '%.*s'\n", i, 11, root[i].name, 11, fat32_filename);
        if (strncmp((const char*)root[i].name, fat32_filename, 11) == 0) {
            printf("Arquivo %s encontrado no índice %u.\n", filename, i);

            uint32_t cluster = root[i].low_starting_cluster;
            uint32_t cluster_address = (bpb->reserved_sect + (cluster - 2) * bpb->sector_p_clust) * bpb->bytes_p_sect;

            printf("Cluster do arquivo: %u, endereço do cluster: 0x%08X\n", cluster, cluster_address);

            // Posicionar o ponteiro no cluster do arquivo
            if (fseek(fp, cluster_address, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no cluster do arquivo");
                return;
            }

            // Escrever os dados no arquivo
            if (fwrite(data, strlen(data), 1, fp) != 1) {
                perror("Erro ao escrever os dados no arquivo");
                return;
            }

            // Atualizar o tamanho do arquivo no diretório
            root[i].file_size = strlen(data);

            // Escrever a entrada do diretório de volta ao disco
            if (fseek(fp, root_address + sizeof(struct fat32_dir) * i, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no diretório para atualizar o tamanho do arquivo");
                return;
            }
            if (fwrite(&root[i], sizeof(struct fat32_dir), 1, fp) != 1) {
                perror("Erro ao atualizar o diretório com o tamanho do arquivo");
                return;
            }

            printf("Dados escritos com sucesso no arquivo %s.\n", filename);
            return;
        }
    }

    fprintf(stderr, "Não foi possível encontrar o arquivo %s.\n", filename);
}

struct fat32_dir_searchres find_in_root(struct fat32_dir *dirs, char *filename, struct fat32_bpb *bpb) {
    struct fat32_dir_searchres res;
    res.found = 0;
    for (unsigned int i = 0; i < bpb->root_entry_count; i++) {
        if (strncmp((const char*)dirs[i].name, filename, 11) == 0) {
            res.found = 1;
            res.dir = &dirs[i];
            break;
        }
    }
    return res;
}

void create(FILE* fp, char* filename, struct fat32_bpb* bpb) {
    char fat32_filename[11];
    memset(fat32_filename, ' ', 11); // Preencher com espaços
    int len = strlen(filename);
    for (int i = 0; i < 11 && i < len; i++) {
        fat32_filename[i] = toupper(filename[i]);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    // Verificar se o arquivo já existe
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        if (strncmp((const char*)root[i].name, fat32_filename, 11) == 0) {
            printf("Arquivo %s já existe.\n", filename);
            return;
        }
    }

    // Encontrar uma entrada livre no diretório
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        if (root[i].name[0] == DIR_FREE_ENTRY || root[i].name[0] == 0xE5) {
            printf("Entrada livre encontrada no índice %u.\n", i);

            // Preencher os dados da nova entrada de diretório
            strncpy((char*)root[i].name, fat32_filename, 11);
            root[i].attr = 0x20; // Atributo de arquivo

            // Encontrar um cluster livre
            struct fat32_newcluster_info cluster_info = fat32_find_free_cluster(fp, bpb);
            if (cluster_info.cluster == 0) {
                fprintf(stderr, "Nenhum cluster livre disponível.\n");
                return;
            }

            root[i].low_starting_cluster = cluster_info.cluster;
            root[i].file_size = 0;

            if (fseek(fp, root_address + sizeof(struct fat32_dir) * i, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no arquivo");
                return;
            }

            if (fwrite(&root[i], sizeof(struct fat32_dir), 1, fp) != 1) {
                perror("Erro ao escrever a nova entrada do diretório");
                return;
            }

            printf("Arquivo %s criado com sucesso.\n", filename);
            return;
        }
    }

    fprintf(stderr, "Diretório raiz cheio. Não foi possível criar o arquivo %s.\n", filename);
}

struct fat32_dir *ls(FILE *fp, struct fat32_bpb *bpb)
{
    uint32_t root_addr = bpb_froot_addr(bpb);
    uint32_t max_entries = bpb->sector_p_clust * bpb->bytes_p_sect / sizeof(struct fat32_dir);

    struct fat32_dir *dirs = malloc(sizeof(struct fat32_dir) * max_entries);
    for (uint32_t i = 0; i < max_entries; i++)
    {
        uint32_t offset = root_addr + i * sizeof(struct fat32_dir);
        read_bytes(fp, offset, &dirs[i], sizeof(dirs[i]));
    }
    
    return dirs;
}

void mv(FILE* fp, char* source, char* dest, struct fat32_bpb* bpb) {
    char source_rname[11], dest_rname[11];
    memset(source_rname, ' ', 11); // Preencher com espaços
    memset(dest_rname, ' ', 11);   // Preencher com espaços
    int srclen = strlen(source), destlen = strlen(dest);
    for (int i = 0; i < 11 && i < srclen; i++) {
        source_rname[i] = toupper(source[i]);
    }
    for (int i = 0; i < 11 && i < destlen; i++) {
        dest_rname[i] = toupper(dest[i]);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    // Correção do tipo da variável
    struct fat32_dir_searchres dir1 = find_in_root(root, source_rname, bpb);
    struct fat32_dir_searchres dir2 = find_in_root(root, dest_rname, bpb);

    if (dir1.found && !dir2.found) {
        printf("Movendo arquivo %s para %s\n", source, dest);
        strncpy((char*)dir1.dir->name, dest_rname, 11);
        if (fseek(fp, root_address + sizeof(struct fat32_dir) * (dir1.dir - root), SEEK_SET) != 0) {
            perror("Erro ao posicionar o ponteiro no arquivo");
            return;
        }
        if (fwrite(dir1.dir, sizeof(struct fat32_dir), 1, fp) != 1) {
            perror("Erro ao escrever a entrada do diretório");
            return;
        }
        printf("Arquivo movido com sucesso.\n");
    } else if (!dir1.found) {
        fprintf(stderr, "Arquivo %s não encontrado.\n", source);
    } else if (dir2.found) {
        fprintf(stderr, "Arquivo de destino %s já existe.\n", dest);
    }
}

void cp(FILE* fp, char* source, char* dest, struct fat32_bpb* bpb) {
    char source_rname[11], dest_rname[11];
    memset(source_rname, ' ', 11); // Preencher com espaços
    memset(dest_rname, ' ', 11);   // Preencher com espaços
    int srclen = strlen(source), destlen = strlen(dest);
    for (int i = 0; i < 11 && i < srclen; i++) {
        source_rname[i] = toupper(source[i]);
    }
    for (int i = 0; i < 11 && i < destlen; i++) {
        dest_rname[i] = toupper(dest[i]);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    // Correção do tipo da variável
    struct fat32_dir_searchres dir1 = find_in_root(root, source_rname, bpb);

    if (!dir1.found) {
        fprintf(stderr, "Arquivo %s não encontrado.\n", source);
        return;
    }

    if (find_in_root(root, dest_rname, bpb).found) {
        fprintf(stderr, "Arquivo %s já existe no destino.\n", dest);
        return;
    }

    // Encontre uma entrada livre no diretório
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        if (root[i].name[0] == DIR_FREE_ENTRY || root[i].name[0] == 0xE5) {
            printf("Copiando arquivo %s para %s\n", source, dest);
            strncpy((char*)root[i].name, dest_rname, 11);
            root[i].attr = dir1.dir->attr;
            root[i].low_starting_cluster = dir1.dir->low_starting_cluster;
            root[i].file_size = dir1.dir->file_size;

            if (fseek(fp, root_address + sizeof(struct fat32_dir) * i, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no arquivo");
                return;
            }
            if (fwrite(&root[i], sizeof(struct fat32_dir), 1, fp) != 1) {
                perror("Erro ao escrever a entrada do diretório");
                return;
            }
            printf("Arquivo copiado com sucesso.\n");
            return;
        }
    }

    fprintf(stderr, "Não foi possível encontrar uma entrada livre no diretório.\n");
}

void rm(FILE* fp, char* filename, struct fat32_bpb* bpb, int debug) {
    char fat32_filename[11];
    memset(fat32_filename, ' ', 11); // Preencher com espaços
    int len = strlen(filename);
    for (int i = 0; i < 11 && i < len; i++) {
        fat32_filename[i] = toupper(filename[i]);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    if (debug) {
        printf("Iniciando a remoção do arquivo %s...\n", filename);
        printf("Nome convertido para FAT32: '%.*s'\n", 11, fat32_filename);
    }
    int found = 0;
    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        if (debug) {
            printf("Verificando entrada do diretório %u: nome='%.*s', comparando com '%.*s'\n", i, 11, root[i].name, 11, fat32_filename);
        }
        if (strncmp((const char*)root[i].name, fat32_filename, 11) == 0) {
            found = 1;
            uint32_t cluster = root[i].low_starting_cluster;

            if (debug) {
                printf("Removendo entrada do diretório no índice %u...\n", i);
            }
            // Limpar a entrada do diretório
            memset(&root[i], 0, sizeof(struct fat32_dir));
            root[i].name[0] = DIR_FREE_ENTRY;

            if (fseek(fp, root_address + sizeof(struct fat32_dir) * i, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no arquivo");
                return;
            }
            if (fwrite(&root[i], sizeof(struct fat32_dir), 1, fp) != 1) {
                perror("Erro ao remover a entrada do diretório");
                return;
            }

            // Limpar a FAT
            if (debug) {
                printf("Limpando a FAT para o cluster %u...\n", cluster);
            }
            uint32_t fat_offset = cluster * 4;
            uint32_t fat_address = (bpb->reserved_sect * bpb->bytes_p_sect) + fat_offset;
            uint32_t fat_entry = 0x00000000; // Cluster livre
            if (fseek(fp, fat_address, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro na FAT");
                return;
            }
            if (fwrite(&fat_entry, sizeof(fat_entry), 1, fp) != 1) {
                perror("Erro ao limpar a FAT");
                return;
            }

            if (debug) {
                printf("Cluster %u limpo na FAT.\n", cluster);
            }
        }
    }

    if (found) {
        printf("Arquivo %s removido com sucesso.\n", filename);
    } else {
        fprintf(stderr, "Arquivo %s não encontrado.\n", filename);
    }
}

void cat(FILE* fp, char* filename, struct fat32_bpb* bpb) {
    char fat32_filename[11];
    memset(fat32_filename, ' ', 11); // Preencher com espaços
    for (int i = 0; i < 11 && filename[i] != '\0'; i++) {
        fat32_filename[i] = toupper(filename[i]); // Copiar e converter para maiúsculas
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = bpb->root_entry_count * sizeof(struct fat32_dir);
    struct fat32_dir root[root_size / sizeof(struct fat32_dir)];

    if (fseek(fp, root_address, SEEK_SET) != 0) {
        perror("Erro ao posicionar o ponteiro no arquivo");
        return;
    }

    if (fread(root, sizeof(struct fat32_dir), root_size / sizeof(struct fat32_dir), fp) != root_size / sizeof(struct fat32_dir)) {
        perror("Erro ao ler o diretório raiz");
        return;
    }

    for (unsigned int i = 0; i < root_size / sizeof(struct fat32_dir); i++) {
        if (strncmp((const char*)root[i].name, fat32_filename, 11) == 0) {
            uint32_t data_address = bpb_fdata_addr(bpb) + (root[i].low_starting_cluster - 2) * bpb->sector_p_clust * bpb->bytes_p_sect;
            char buffer[root[i].file_size + 1];
            memset(buffer, 0, sizeof(buffer));

            if (fseek(fp, data_address, SEEK_SET) != 0) {
                perror("Erro ao posicionar o ponteiro no arquivo");
                return;
            }

            if (fread(buffer, sizeof(char), root[i].file_size, fp) != root[i].file_size) {
                perror("Erro ao ler o arquivo");
                return;
            }

            printf("Conteúdo do arquivo %s:\n%s\n", filename, buffer);
            return;
        }
    }

    fprintf(stderr, "Não foi possível encontrar o arquivo %s.\n", filename);
}
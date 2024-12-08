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

void mv(FILE *fp, char *source, char* dest, struct fat32_bpb *bpb)
{
    char source_rname[FAT32STR_SIZE_WNULL], dest_rname[FAT32STR_SIZE_WNULL];

    bool badname = cstr_to_fat32_lfn(source, source_rname)
                || cstr_to_fat32_lfn(dest,   dest_rname);

    if (badname)
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat32_dir) * bpb->root_entry_count;

    struct fat32_dir root[root_size];

    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat32_dir");

    struct far_dir_searchres dir1 = find_in_root(root, source_rname, bpb);
    struct far_dir_searchres dir2 = find_in_root(root, dest_rname, bpb);

    if (dir2.found == true)
        error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via mv.", dest);

    if (dir1.found == false)
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

    memcpy(dir1.fdir.name, dest_rname, sizeof(char) * FAT32STR_SIZE);

    uint32_t source_address = sizeof(struct fat32_dir) * dir1.idx + root_address;

    (void) fseek(fp, source_address, SEEK_SET);
    (void) fwrite(&dir1.fdir, sizeof(struct fat32_dir), 1, fp);

    printf("mv %s → %s.\n", source, dest);
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

void cp(FILE *fp, char* source, char* dest, struct fat32_bpb *bpb)
{
    char source_rname[FAT32STR_SIZE_WNULL], dest_rname[FAT32STR_SIZE_WNULL];

    bool badname = cstr_to_fat32_lfn(source, source_rname) // Alterado para FAT32
                || cstr_to_fat32_lfn(dest, dest_rname);  // Alterado para FAT32

    if (badname)
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat32_dir) * bpb->root_entry_count;

    struct fat32_dir root[root_size];

    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat32_dir");

    struct far_dir_searchres dir1 = find_in_root(root, source_rname, bpb);

    if (!dir1.found)
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

    if (find_in_root(root, dest_rname, bpb).found)
        error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via cp.", dest);

    struct fat32_dir new_dir = dir1.fdir;
    memcpy(new_dir.name, dest_rname, FAT32STR_SIZE); // Alterado para FAT32

    bool dentry_failure = true;

    for (int i = 0; i < bpb->root_entry_count; i++) 
        if (root[i].name[0] == DIR_FREE_ENTRY || root[i].name[0] == '\0')
        {
            uint32_t dest_address = sizeof(struct fat32_dir) * i + root_address;

            (void) fseek(fp, dest_address, SEEK_SET);
            (void) fwrite(&new_dir, sizeof(struct fat32_dir), 1, fp);

            dentry_failure = false;
            break;
        }

    if (dentry_failure)
        error_at_line(EXIT_FAILURE, ENOSPC, __FILE__, __LINE__, "Não foi possível alocar uma entrada no diretório raiz.");

    int count = 0;
    struct fat32_newcluster_info next_cluster,
                                prev_cluster = { .cluster = FAT32_EOF }; // Alterado para FAT32

    uint32_t cluster_count = dir1.fdir.file_size / bpb->bytes_p_sect / bpb->sector_p_clust + 1;

    while (cluster_count--)
    {
        prev_cluster = next_cluster;
        next_cluster = fat32_find_free_cluster(fp, bpb); // Alterado para FAT32

        if (next_cluster.cluster == 0x0)
            error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "Disco cheio (imagem foi corrompida)");

        uint32_t fat_entry_address = bpb->sect_per_fat_32 + next_cluster.cluster * sizeof(uint32_t); // Alterado para 32 bits
        (void) fseek(fp, fat_entry_address, SEEK_SET);
        (void) fwrite(&prev_cluster.cluster, sizeof(uint32_t), 1, fp); // Alterado para 32 bits

        prev_cluster = next_cluster;
        count++;
    }

    uint32_t cluster_address = bpb_fdata_addr(bpb);
    uint32_t cur_addr, src_addr;

    for (int i = 0; i < count; i++) {
        cur_addr = cluster_address + i * bpb->sector_p_clust * bpb->bytes_p_sect;
        src_addr = dir1.fdir.low_starting_cluster * bpb->sector_p_clust * bpb->bytes_p_sect; // Alterado para considerar low e high clusters

        uint8_t buffer[bpb->sector_p_clust * bpb->bytes_p_sect]; // Buffer para ler os dados
        (void) read_bytes(fp, src_addr, buffer, sizeof(buffer));
        (void) write_bytes(fp, cur_addr, buffer, sizeof(buffer)); // Escrever o buffer no endereço de destino
    }

    printf("cp %s → %s\n", source, dest);

    return;
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
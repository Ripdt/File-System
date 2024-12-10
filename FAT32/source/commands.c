#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>
#include "commands.h"
#include "fat32.h" // Alterado para incluir a header do FAT32
#include "support.h"

#include <errno.h>
#include <err.h>
#include <error.h>
#include <assert.h>

#include <sys/types.h>

struct far_dir_searchres find_in_root(struct fat32_dir *dirs, char *filename, struct fat32_bpb *bpb)
{
    struct far_dir_searchres res = { .found = false };

    for (size_t i = 0; i < bpb->root_entry_count; i++)
    {
        if (dirs[i].name[0] == '\0') continue;

        if (memcmp((char *) dirs[i].name, filename, FAT32STR_SIZE) == 0)
        {
            res.found = true;
            res.fdir  = dirs[i];
            res.idx   = i;
            break;
        }
    }

    return res;
}

struct fat32_dir *ls(FILE *fp, struct fat32_bpb *bpb)
{
    uint32_t root_addr = bpb_froot_addr(bpb);
    uint32_t max_entries = bpb->sector_p_clust * bpb->bytes_p_sect / sizeof(struct fat32_dir);

    struct fat32_dir *dirs = malloc(sizeof(struct fat32_dir) * max_entries);

    for (uint32_t i = 0; i < max_entries; i++) {
        uint32_t offset = root_addr + i * sizeof(struct fat32_dir);
        read_bytes(fp, offset, &dirs[i], sizeof(dirs[i]));
    }

    return dirs;
}

struct fat32_newcluster_info fat32_find_free_cluster(FILE *fp, struct fat32_bpb *bpb)
{
    uint32_t cluster = 0x0;
    uint32_t fat_address = bpb_faddress(bpb);
    uint32_t total_clusters = bpb_fdata_cluster_count(bpb);

    for (cluster = 0x2; cluster < total_clusters; cluster++) {
        uint32_t entry;
        uint32_t entry_address = fat_address + cluster * sizeof(uint32_t);
        read_bytes(fp, entry_address, &entry, sizeof(uint32_t));

        if (entry == 0x0) {
            struct fat32_newcluster_info res = { .cluster = cluster, .address = entry_address };
            return res;
        }
    }

    struct fat32_newcluster_info res = { .cluster = 0, .address = 0 }; // Não encontrou espaço livre
    return res;
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

void rm(FILE* fp, char* filename, struct fat32_bpb* bpb)
{
    char fat32_rname[FAT32STR_SIZE_WNULL];

    if (cstr_to_fat32_lfn(filename, fat32_rname))
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat32_dir) * bpb->root_entry_count;

    struct fat32_dir root[root_size];
    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
    {
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat32_dir");
    }

    struct far_dir_searchres dir = find_in_root(root, fat32_rname, bpb);

    if (dir.found == false)
    {
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", filename);
    }

    dir.fdir.name[0] = DIR_FREE_ENTRY;

    uint32_t file_address = sizeof(struct fat32_dir) * dir.idx + root_address;

    (void) fseek(fp, file_address, SEEK_SET);
    (void) fwrite(&dir.fdir, sizeof(struct fat32_dir), 1, fp);

    uint32_t fat_address = bpb_faddress(bpb);
    uint32_t cluster_number = dir.fdir.low_starting_cluster | (dir.fdir.high_starting_cluster << 16);
    uint32_t null = 0x0;
    size_t count = 0;

    while (cluster_number < FAT32_EOF)
    {
        uint32_t infat_cluster_address = fat_address + cluster_number * sizeof(uint32_t);
        read_bytes(fp, infat_cluster_address, &cluster_number, sizeof(uint32_t));

        (void) fseek(fp, infat_cluster_address, SEEK_SET);
        (void) fwrite(&null, sizeof(uint32_t), 1, fp);

        count++;
    }

    printf("rm %s, %li clusters apagados.\n", filename, count);
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
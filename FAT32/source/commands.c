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


struct far_dir_searchres find_in_root(struct fat_dir *dirs, char *filename, struct fat_bpb *bpb)
{
    struct far_dir_searchres res = { .found = false };

    for (size_t i = 0; i < bpb->possible_rentries; i++)
    {
        if (dirs[i].name[0] == '\0') continue;

        if (memcmp((char *) dirs[i].name, filename, FAT32STR_SIZE) == 0) // Alterado para FAT32
        {
            res.found = true;
            res.fdir  = dirs[i];
            res.idx   = i;
            break;
        }
    }

    return res;
}


struct fat_dir *ls(FILE *fp, struct fat_bpb *bpb)
{
    uint32_t root_addr = bpb_froot_addr(bpb);
    uint32_t max_entries = bpb->sector_p_clust * bpb->bytes_p_sect / sizeof(struct fat_dir);

    struct fat_dir *dirs = malloc(sizeof(struct fat_dir) * max_entries);

    for (uint32_t i = 0; i < max_entries; i++) {
        uint32_t offset = root_addr + i * sizeof(struct fat_dir);
        read_bytes(fp, offset, &dirs[i], sizeof(dirs[i]));
    }

    return dirs;
}

struct fat32_newcluster_info fat32_find_free_cluster(FILE *fp, struct fat_bpb *bpb)
{
    uint32_t cluster = 0x0;
    uint32_t fat_address = bpb->fat32_faddress;
    uint32_t total_clusters = bpb_fdata_cluster_count(bpb);

    for (cluster = 0x2; cluster < total_clusters; cluster++) {
        uint32_t entry;
        uint32_t entry_address = fat_address + cluster * sizeof(uint32_t);
        read_bytes(fp, entry_address, &entry, sizeof(uint32_t));

        if (entry == 0x0) {
            return cluster;
        }
    }

    return 0; // Não encontrou espaço livre
}

void mv(FILE *fp, char *source, char* dest, struct fat_bpb *bpb)
{
    char source_rname[FAT32STR_SIZE_WNULL], dest_rname[FAT32STR_SIZE_WNULL];

    bool badname = cstr_to_fat32_lfn(source, source_rname) // Alterado para FAT32
                || cstr_to_fat32_lfn(dest,   dest_rname);  // Alterado para FAT32

    if (badname)
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;

    struct fat_dir root[root_size];

    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");

    struct far_dir_searchres dir1 = find_in_root(root, source_rname, bpb);
    struct far_dir_searchres dir2 = find_in_root(root, dest_rname, bpb);

    if (dir2.found == true)
        error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via mv.", dest);

    if (dir1.found == false)
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

    memcpy(dir1.fdir.name, dest_rname, sizeof(char) * FAT32STR_SIZE); // Alterado para FAT32

    uint32_t source_address = sizeof(struct fat_dir) * dir1.idx + root_address;

    (void) fseek(fp, source_address, SEEK_SET);
    (void) fwrite(&dir1.fdir, sizeof(struct fat_dir), 1, fp);

    printf("mv %s → %s.\n", source, dest);

    return;
}

void rm(FILE* fp, char* filename, struct fat_bpb* bpb)
{
    char fat32_rname[FAT32STR_SIZE_WNULL];

    if (cstr_to_fat32_lfn(filename, fat32_rname)) // Alterado para FAT32
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;

    struct fat_dir root[root_size];
    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
    {
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");
    }

    struct far_dir_searchres dir = find_in_root(root, fat32_rname, bpb);

    // Verifica se o arquivo foi encontrado.
    if (dir.found == false)
    {
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", filename);
    }

    // Marca a entrada como livre.
    dir.fdir.name[0] = DIR_FREE_ENTRY; // DIR_FREE_ENTRY é definido como 0xE5 que é o valor que indica que a entrada está livre.

    // Calcula o endereço da entrada do diretório a ser deletada.
    uint32_t file_address = sizeof(struct fat_dir) * dir.idx + root_address;

    // Escreve a entrada atualizada de volta ao disco.
    (void) fseek(fp, file_address, SEEK_SET);
    (void) fwrite(&dir.fdir, sizeof(struct fat_dir), 1, fp);

    /* Após zerar a entrada de diretório, liberar espaço em disco */

    /* Leitura da tabela FAT explicada na função cat() */
    uint32_t fat_address    = bpb_faddress(bpb);
    uint16_t cluster_number = dir.fdir.starting_cluster;
    uint16_t null           = 0x0;
    size_t   count          = 0;

    /* Continua a zerar os clusters até chegar no End Of File */
    while (cluster_number < FAT32_EOF_LO)
    {
        uint32_t infat_cluster_address = fat_address + cluster_number * sizeof(uint32_t); // Alterado para 32 bits
        read_bytes(fp, infat_cluster_address, &cluster_number, sizeof(uint32_t)); // Alterado para 32 bits

        /* Setar o cluster number como NULL */
        (void) fseek(fp, infat_cluster_address, SEEK_SET);
        (void) fwrite(&null, sizeof(uint32_t), 1, fp); // Alterado para 32 bits

        count++;
    }

    printf("rm %s, %li clusters apagados.\n", filename, count);

    return;
}

void cp(FILE *fp, char* source, char* dest, struct fat_bpb *bpb)
{
    char source_rname[FAT32STR_SIZE_WNULL], dest_rname[FAT32STR_SIZE_WNULL];

    bool badname = cstr_to_fat32_lfn(source, source_rname) // Alterado para FAT32
                || cstr_to_fat32_lfn(dest,   dest_rname);  // Alterado para FAT32

    if (badname)
    {
        fprintf(stderr, "Nome de arquivo inválido.\n");
        exit(EXIT_FAILURE);
    }

    uint32_t root_address = bpb_froot_addr(bpb);
    uint32_t root_size = sizeof(struct fat_dir) * bpb->possible_rentries;

    struct fat_dir root[root_size];

    if (read_bytes(fp, root_address, &root, root_size) == RB_ERROR)
        error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "erro ao ler struct fat_dir");

    struct far_dir_searchres dir1 = find_in_root(root, source_rname, bpb);

    if (!dir1.found)
        error(EXIT_FAILURE, 0, "Não foi possível encontrar o arquivo %s.", source);

    if (find_in_root(root, dest_rname, bpb).found)
        error(EXIT_FAILURE, 0, "Não permitido substituir arquivo %s via cp.", dest);

    struct fat_dir new_dir = dir1.fdir;
    memcpy(new_dir.name, dest_rname, FAT32STR_SIZE); // Alterado para FAT32

    bool dentry_failure = true;

    for (int i = 0; i < bpb->possible_rentries; i++) 
        if (root[i].name[0] == DIR_FREE_ENTRY || root[i].name[0] == '\0')
        {
            uint32_t dest_address = sizeof(struct fat_dir) * i + root_address;

            (void) fseek(fp, dest_address, SEEK_SET);
            (void) fwrite(&new_dir, sizeof(struct fat_dir), 1, fp);

            dentry_failure = false;
            break;
        }

    if (dentry_failure)
        error_at_line(EXIT_FAILURE, ENOSPC, __FILE__, __LINE__, "Não foi possível alocar uma entrada no diretório raiz.");

    int count = 0;
    struct fat32_newcluster_info next_cluster,
                                prev_cluster = { .cluster = FAT32_EOF_HI }; // Alterado para FAT32

    uint32_t cluster_count = dir1.fdir.file_size / bpb->bytes_p_sect / bpb->sector_p_clust + 1;

    while (cluster_count--)
    {
        prev_cluster = next_cluster;
        next_cluster = fat32_find_free_cluster(fp, bpb); // Alterado para FAT32

        if (next_cluster.cluster == 0x0)
            error_at_line(EXIT_FAILURE, EIO, __FILE__, __LINE__, "Disco cheio (imagem foi corrompida)");

        uint32_t fat_entry_address = bpb->fat32_faddress + next_cluster.cluster * sizeof(uint32_t); // Alterado para 32 bits
        (void) fseek(fp, fat_entry_address, SEEK_SET);
        (void) fwrite(&prev_cluster.cluster, sizeof(uint32_t), 1, fp); // Alterado para 32 bits

        prev_cluster = next_cluster;
        count++;
    }

    uint32_t cluster_address = bpb_fdata_addr(bpb);
    for (int i = 0; i < count; i++) {
        uint32_t cur_addr = cluster_address + i * bpb->sector_p_clust * bpb->bytes_p_sect;
        uint32_t src_addr = dir1.fdir.starting_cluster * bpb->sector_p_clust * bpb->bytes_p_sect;

        (void) read_bytes(fp, src_addr, (void*) cur_addr, bpb->sector_p_clust * bpb->bytes_p_sect);
    }

    printf("cp %s → %s\n", source, dest);

    return;
}

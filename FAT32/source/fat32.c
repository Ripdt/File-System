#include "fat32.h"
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <err.h>

/* calcular endereço da FAT */
uint32_t bpb_faddress(struct fat32_bpb *bpb)
{
    return bpb->reserved_sect * bpb->bytes_p_sect;
}

/* calcular endereço do Root Directory */
uint32_t bpb_froot_addr(struct fat32_bpb *bpb) {
    uint32_t root_cluster = bpb->root_cluster;
    uint32_t first_data_sector = bpb_fdata_addr(bpb) / bpb->bytes_p_sect;
    return (root_cluster - 2) * bpb->sector_p_clust * bpb->bytes_p_sect + first_data_sector * bpb->bytes_p_sect;
}

/* calcular endereço dos dados */
uint32_t bpb_fdata_addr(struct fat32_bpb *bpb) {
    uint32_t fat_size = bpb->sect_per_fat_32 * bpb->bytes_p_sect;
    uint32_t first_data_sector = bpb->reserved_sect + (bpb->n_fat * fat_size / bpb->bytes_p_sect);
    return first_data_sector * bpb->bytes_p_sect;
}

/* calcular quantidade de setores de dados */
uint32_t bpb_fdata_sector_count(struct fat32_bpb *bpb) {
    uint32_t fat_size = bpb->sect_per_fat_32 * bpb->bytes_p_sect;
    uint32_t total_fat_size = bpb->n_fat * fat_size;
    uint32_t total_reserved_sectors = bpb->reserved_sect * bpb->bytes_p_sect;
    uint32_t data_sectors = (bpb->total_sectors_32 * bpb->bytes_p_sect) - total_reserved_sectors - total_fat_size;
    return data_sectors / bpb->bytes_p_sect;
}


/* calcular quantidade de clusters de dados */
uint32_t bpb_fdata_cluster_count(struct fat32_bpb* bpb) {
    uint32_t data_sectors = bpb_fdata_sector_count(bpb);
    return data_sectors / bpb->sector_p_clust;
}


/*
 * permite ler a partir de um deslocamento específico e escrever os dados em buff
 * retorna RB_ERROR se a busca ou leitura falhar e RB_OK se for bem-sucedida
 */
int read_bytes(FILE *fp, unsigned int offset, void *buff, unsigned int len)
{
    if (fseek(fp, offset, SEEK_SET) != 0)
    {
        error_at_line(0, errno, __FILE__, __LINE__, "warning: error when seeking to %u", offset);
        return RB_ERROR;
    }
    if (fread(buff, 1, len, fp) != len)
    {
        error_at_line(0, errno, __FILE__, __LINE__, "warning: error reading file");
        return RB_ERROR;
    }

    return RB_OK;
}

void rfat(FILE *fp, struct fat32_bpb *bpb) {
    if (read_bytes(fp, 0x0, bpb, sizeof(struct fat32_bpb)) == RB_ERROR) {
        fprintf(stderr, "Erro ao ler o BPB do FAT32\n");
        exit(EXIT_FAILURE);
    }
}


#include "fat16.h"
#include <stdlib.h>
#include <errno.h>
#include <error.h>
#include <err.h>

/* calculate FAT address */
uint32_t bpb_faddress(struct fat_bpb *bpb)
{
    return bpb->reserved_sect * bpb->bytes_p_sect;
}

/* calculate FAT root address */
uint32_t bpb_froot_addr(struct fat_bpb *bpb)
{
    // FAT32 tem um cálculo diferente, incluindo o uso de root cluster
    uint32_t first_data_sector = bpb->reserved_sect + (bpb->n_fat * bpb->sect_per_fat_32);
    uint32_t sector_offset = (bpb->root_cluster - 2) * bpb->sector_p_clust;
    return (first_data_sector + sector_offset) * bpb->bytes_p_sect;
}

/* calculate data address */
uint32_t bpb_fdata_addr(struct fat_bpb *bpb)
{
    // A função de cálculo para FAT32 é semelhante, mas agora considera o root cluster.
    return bpb_froot_addr(bpb) + bpb->possible_rentries * sizeof(struct fat_dir);
}

/* calculate data sector count */
uint32_t bpb_fdata_sector_count(struct fat_bpb *bpb)
{
    return bpb->large_n_sects - (bpb_fdata_addr(bpb) / bpb->bytes_p_sect);
}

static uint32_t bpb_fdata_sector_count_s(struct fat_bpb* bpb)
{
    return bpb->snumber_sect - (bpb_fdata_addr(bpb) / bpb->bytes_p_sect);
}

/* calculate data cluster count */
uint32_t bpb_fdata_cluster_count(struct fat_bpb* bpb)
{
    uint32_t sectors = bpb_fdata_sector_count_s(bpb);
    return sectors / bpb->sector_p_clust;
}

/*
 * allows reading from a specific offset and writing the data to buff
 * returns RB_ERROR if seeking or reading failed and RB_OK if success
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

/* read FAT32's BIOS parameter block */
void rfat(FILE *fp, struct fat_bpb *bpb)
{
    read_bytes(fp, 0x0, bpb, sizeof(struct fat_bpb));
    return;
}

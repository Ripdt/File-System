# Log de mudanças do FAT16 para o FAT32

## campos do fat32 adicionados no bpb

```cpp
/* FAT32-specific fields */
uint32_t sect_per_fat_32;       /* Sectors per FAT (32-bit) */
uint16_t flags;                 /* Flags (e.g., active FAT and mirroring info) */
uint16_t version;               /* FAT32 version (typically 0x0000) */
uint32_t root_cluster;          /* Cluster number of the root directory (typically 2) */
uint16_t fs_info;               /* Sector number of the FSInfo structure (typically 1) */
uint16_t backup_boot_sector;    /* Sector number of the backup boot sector (typically 6) */
uint8_t reserved[12];           /* Reserved for future use (set to 0) */

/* Boot sector signature */
uint8_t drive_number;           /* Drive number (e.g., 0x80 for hard disk) */
uint8_t reserved1;              /* Reserved (set to 0) */
uint8_t boot_signature;         /* Extended boot signature (0x29 indicates presence of next fields) */
uint32_t volume_id;             /* Volume ID (serial number) */
char volume_label[11];          /* Volume label (padded with spaces) */
char fs_type[8];                /* File system type ("FAT32   ") */

/* Bootstrap code and signature */
uint8_t bootstrap[420];         /* Bootstrap code */
uint16_t signature;             /* Boot sector signature (always 0x55AA) */
```

## cálculo do endereço raiz:

```cpp
/* calculate FAT root address */
uint32_t bpb_froot_addr(struct fat_bpb *bpb)
{
	uint32_t first_data_sector = bpb->reserved_sect + (bpb->n_fat * bpb->sect_per_fat_32);
	uint32_t sector_offset = (bpb->root_cluster - 2) * bpb->sector_p_clust;
	return (first_data_sector + sector_offset) * bpb->bytes_p_sect;
}
```

## função ls

```cpp
/*
 * Função de ls
 *
 * Ela itéra todas as bpb->possible_rentries do diretório raiz
 * e as lê via read_bytes().
 */
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
```
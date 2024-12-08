#include "output.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static struct pretty_int { int num; char* suff; } pretty_print(int value)
{

	static char* B   = "B";
	static char* KiB = "KiB";
	static char* MiB = "MiB";

	struct pretty_int res =
	{
		.num  = value,
		.suff = B
	};

	if (res.num == 0) return res;

	if (res.num >= 1024)
	{
		res.num  /= 1024;
		res.suff = KiB;
	}

	if (res.num >= 1024)
	{
		res.num  /= 1024;
		res.suff = MiB;
	}

	return res;
}

void show_files(struct fat_dir *dirs)
{

	struct fat_dir *cur;

	fprintf(stdout, "ATTR  NAME    FMT    SIZE\n-------------------------\n");

    while ((cur = dirs++) != NULL)
	{

		if (cur->name[0] == 0)
			break;

        else if ((cur->name[0] == DIR_FREE_ENTRY) || (cur->attr == DIR_FREE_ENTRY))
            continue;

		else if (cur->attr == 0xf)
			continue;

		struct pretty_int num = pretty_print(cur->file_size);

        fprintf(stdout, "0x%-.*x  %.*s  %4i %s\n", 2, cur->attr, FAT16STR_SIZE, cur->name, num.num, num.suff);
    }

    return;
}

void verbose(struct fat_bpb *bios_pb)
{

    fprintf(stdout, "Bios parameter block:\n");
    fprintf(stdout, "Jump instruction: ");

    for (int i = 0; i < 3; i++)
        fprintf(stdout, "%hhX ", bios_pb->jmp_instruction[i]);

    fprintf(stdout, "\n");

    fprintf(stdout, "OEM ID: %s\n", bios_pb->oem_id);
    fprintf(stdout, "Bytes per sector: %d\n", bios_pb->bytes_p_sect);
    fprintf(stdout, "Sector per cluster: %d\n", bios_pb->sector_p_clust);
    fprintf(stdout, "Reserved sector: %d\n", bios_pb->reserved_sect);
    fprintf(stdout, "Number of FAT copies: %d\n", bios_pb->n_fat);
    fprintf(stdout, "Number of possible entries: %d\n", bios_pb->possible_rentries);
    fprintf(stdout, "Small number of sectors: %d\n", bios_pb->snumber_sect);
    fprintf(stdout, "Media descriptor: %hhX\n", bios_pb->media_desc);
    fprintf(stdout, "Sector per fat: %d\n", bios_pb->sect_per_fat);

    fprintf(stdout, "Sector per track: %d\n", bios_pb->sect_per_track);
    fprintf(stdout, "Number of heads: %d\n", bios_pb->number_of_heads);

    fprintf(stdout, "FAT Address: 0x%x\n", bpb_faddress(bios_pb));
    fprintf(stdout, "Root Address: 0x%x\n", bpb_froot_addr(bios_pb));
    fprintf(stdout, "Data Address: 0x%x\n", bpb_fdata_addr(bios_pb));

    /* FAT32-specific fields */
    fprintf(stdout, "Sector per FAT32: %d\n", bios_pb->sect_per_fat_32);
    fprintf(stdout, "Flags: %d\n", bios_pb->flags);
    fprintf(stdout, "Version: %hhX\n", bios_pb->version);
    fprintf(stdout, "Cluster number of the root directory: %d\n", bios_pb->root_cluster);
    fprintf(stdout, "Sector number of the FSInfo structure: %d\n", bios_pb->fs_info);
    fprintf(stdout, "Reserved for future use: ");
    for (int i = 0; i < 12; i++)
        fprintf(stdout, "%d ", bios_pb->reserved[i]);
    fprintf(stdout, "\n");

    /* Boot sector signature */
    fprintf(stdout, "Drive number: %hhX\n", bios_pb->drive_number);
    fprintf(stdout, "Reserved: %d\n", bios_pb->reserved1);
    fprintf(stdout, "Extended boot signature: %hhX\n", bios_pb->boot_signature);
    fprintf(stdout, "Volume ID: %d\n", bios_pb->volume_id);
    fprintf(stdout, "Volume label: ");
    for (int i = 0; i < 11; i++)
        fprintf(stdout, "%c ", bios_pb->volume_label[i]);
    fprintf(stdout, "\n");
    fprintf(stdout, "File system type: ");
    for (int i = 0; i < 8; i++)
        fprintf(stdout, "%c", bios_pb->fs_type[i]);
    fprintf(stdout, "\n");

    /* Bootstrap code and signature */
    fprintf(stdout, "Bootstrap code: ");
    for (int i = 0; i < 420; i++)
        fprintf(stdout, "%i", bios_pb->bootstrap[i]);
    fprintf(stdout, "\n");
    fprintf(stdout, "Boot sector signature: %d\n", bios_pb->signature);
    fprintf(stdout, "\n");
    fprintf(stdout, "\n");

	return;
}

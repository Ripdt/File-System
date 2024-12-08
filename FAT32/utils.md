> Parcial git clone from original repository

```
git clone --no-checkout https://github.com/VielF/SO-Codes/
cd SO-Codes/
git sparse-checkout init
git sparse-checkout set "File System/Fat16_update"
git checkout
```

> Create image FAT32

```
dd if=/dev/zero of=disk.img bs=1M count=64
mkfs.fat -F 32 disk.img 
fsck.fat -v disk.img
```

> Test disk.img mount

```
mkdir mnt
mkdir mnt/fat32
sudo mount -o loop disk.img /mnt/fat32
ls /mnt/fat32
sudo umount /mnt/fat32
```

> Write text file in disk.img

```
mkdir mnt
mkdir mnt/fat32
sudo mount -o loop disk.img /mnt/fat32
echo "Teste FAT32" > /mnt/fat32/teste.txt
ls -l /mnt/fat32
df -h /mnt/fat32
sudo umount /mnt/fat32
fsck.fat -v disk.img
```
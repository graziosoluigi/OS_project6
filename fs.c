
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define DISK_BLOVK_SIZE 4096
#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5			//number of direct pointers in each inode
#define POINTERS_PER_BLOCK 1024			//pointers to be found in an indirect block

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

void fs_debug()
{
	int i, j, k, ninodes, ninodeblocks, nblocks;
	int check;
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC)
		printf("\tmagic number is valid\n");
	else{
		printf("\tmagic number is NOT valid\n");
		return;
	}
	ninodes = block.super.ninodes;
	ninodeblocks = block.super.ninodeblocks;
	nblocks = block.super.nblocks;

	printf("\t%d blocks\n",block.super.nblocks);
	printf("\t%d inode blocks\n",block.super.ninodeblocks);
	printf("\t%d inodes\n",block.super.ninodes);
	for(i = 0; i < ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){
			//printf ("j = %d\t", j); printf("size = %d\n", block.inode[j].size);
			if(block.inode[j].isvalid == 1){
				printf("inode %d:\n", (i*INODES_PER_BLOCK)+j);
				printf("\tsize: %d bytes\n", block.inode[j].size);
				
				check = 0;
				for(k = 0; k < POINTERS_PER_INODE; k++){
					if(block.inode[j].direct[k] != 0){
						if (check == 0){
							printf("\tdirect blocks:");
							check = 1;
						}
						printf(" %d", block.inode[j].direct[k]);
					}
				}
				if (check == 1) 
					printf("\n");
				
				if (block.inode[j].indirect > 0){
					printf("\tindirect block: %d\n", block.inode[j].indirect);
					disk_read(block.inode[j].indirect, block.data);
					check = 0;
					for (k = 0; k < POINTERS_PER_BLOCK; k++){
						if (block.pointers[k] == 0)
							break;
						if (check == 0){
							printf("\tindirect data blocks:");
							check = 1;
						}
						printf(" %d", block.pointers[k]);
					}
					if (check == 1)
						printf("\n");
				}

			}
		}
	}
}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}

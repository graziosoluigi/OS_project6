#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024
int IS_MOUNTED = 0;
int ninodeblocks = 0;
int nblocks = 0;
int ninodes = 0;
int* bitmap = NULL;

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
	if (IS_MOUNTED == 1)
		return 0;

	const char blank_block[DISK_BLOCK_SIZE] = {0};
	union fs_block block;
	block.super.magic = FS_MAGIC;
	block.super.nblocks = disk_size();
	block.super.ninodeblocks = disk_size() / 10;
	if(disk_size() % 10 > 0)
		block.super.ninodeblocks++;
	block.super.ninodes = block.super.ninodeblocks * INODES_PER_BLOCK;
	
	int i;
	disk_write(0, block.data);
	for(i = 0; i < block.super.ninodeblocks; i++){
		disk_write(i+1, blank_block);
	}
	
	return 1;
}

void fs_debug()
{
	int i, j, k, ninodes, ninodeblocks, nblocks;
	int check;
	union fs_block block;
	union fs_block indirect;
	
	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC)
		printf("    magic number is valid\n");
	else{
		printf("    magic number is NOT valid\n");
		return;
	}
	ninodes = block.super.ninodes;
	ninodeblocks = block.super.ninodeblocks;
	nblocks = block.super.nblocks;

	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
	for(i = 0; i < ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){

			if(block.inode[j].isvalid == 1){
				printf("inode %d:\n", (i*INODES_PER_BLOCK)+j);
				printf("    size: %d bytes\n", block.inode[j].size);
				if(block.inode[j].size == 0)
					continue;
				
				check = 0;
				for(k = 0; k < POINTERS_PER_INODE; k++){
					if(block.inode[j].direct[k] != 0){
						if (check == 0){
							printf("    direct blocks:");
							check = 1;
						}
						printf(" %d", block.inode[j].direct[k]);
					}
				}
				if (check == 1) 
					printf("\n");
				
				if (block.inode[j].indirect > 0){
					printf("    indirect block: %d\n", block.inode[j].indirect);
					disk_read(block.inode[j].indirect, indirect.data);
					check = 0;
					for (k = 0; k < POINTERS_PER_BLOCK; k++){
						if (indirect.pointers[k] == 0)
							break;
						if (check == 0){
							printf("    indirect data blocks:");
							check = 1;
						}
						printf(" %d", indirect.pointers[k]);
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
	int i, j, k;
	union fs_block block;
	union fs_block temp;
	disk_read(0,block.data);
	if(block.super.magic != FS_MAGIC)
		return 0;

	ninodeblocks = block.super.ninodeblocks;
	nblocks = block.super.nblocks;
	ninodes = block.super.ninodes;


	if(bitmap != NULL) free(bitmap);
	bitmap = malloc(sizeof(int)*nblocks);
	for(i = 0; i < nblocks; i++)
		bitmap[i] = 0;
	bitmap[0] = 1;
	for(i = 0; i < ninodeblocks; i++){
		bitmap[i+1] = 1;

		disk_read(i+1, block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){
			if(block.inode[j].isvalid == 1){
				for(k = 0; k < POINTERS_PER_INODE; k++){
					if(block.inode[j].direct[k] != 0) {
						bitmap[block.inode[j].direct[k]] = 1;
					}
				}
				if (block.inode[j].indirect > 0){
					bitmap[block.inode[j].indirect] = 1;
					disk_read(block.inode[j].indirect, temp.data);
					for (k = 0; k < POINTERS_PER_BLOCK; k++){
						if (temp.pointers[k] == 0)
							break;
						bitmap[temp.pointers[k]] = 1;
					}
				}

			}
		}
	}

	IS_MOUNTED = 1;
	return 1;
}

int fs_create()
{
	if(IS_MOUNTED != 1)
		return 0;
	union fs_block block;
	int i, j;

	for(i = 0; i < ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){
			if(block.inode[j].isvalid == 0 && (i*INODES_PER_BLOCK)+j != 0){
				block.inode[j].isvalid = 1;
				block.inode[j].size = 0;
				disk_write(i+1, block.data);
				return (i*INODES_PER_BLOCK) + j;
			}
		}
	}
	return 0;
}

int fs_delete( int inumber )
{
	if(IS_MOUNTED != 1)
		return 0;
	if (inumber >= ninodes || inumber <= 0)
		return 0;
	union fs_block block;
	union fs_block indirect;
	int inode, ishift;

	inode = inumber / INODES_PER_BLOCK;
	ishift = inumber % INODES_PER_BLOCK;

	disk_read(inode+1, block.data);
	if(block.inode[ishift].isvalid != 1)
		return 0;

	int i;
	for(i = 0; i < POINTERS_PER_INODE; i++){
		if(block.inode[ishift].direct[i] != 0)
			bitmap[block.inode[ishift].direct[i]] = 0;
	}
	if(block.inode[ishift].indirect > 0){
		disk_read(block.inode[ishift].indirect, indirect.data);
		for(i = 0; i < POINTERS_PER_BLOCK; i++){
			if(indirect.pointers[i] == 0)
				break;
			bitmap[indirect.pointers[i]] = 0;
			indirect.pointers[i] = 0;
		}
		bitmap[block.inode[ishift].indirect] = 0;
		disk_write(block.inode[ishift].indirect, indirect.data);
	}

	block.inode[ishift].isvalid = 0;
	block.inode[ishift].size = 0;

	disk_write(inode+1, block.data);
	return 1;
}

int fs_getsize( int inumber )
{
	if(IS_MOUNTED != 1)
		return -1;
	if (inumber >= ninodes || inumber <= 0)
		return -1;
	union fs_block block;
	int inode, ishift;
	inode = inumber / INODES_PER_BLOCK;
	ishift = inumber % INODES_PER_BLOCK;
	disk_read(inode+1, block.data);
	if(block.inode[ishift].isvalid != 1)
		return -1;
	return block.inode[ishift].size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	if(IS_MOUNTED != 1)
		return 0;
	if(inumber >= ninodes || inumber <= 0)
		return 0;
	union fs_block block;
	int inode, ishift;
	inode = inumber / INODES_PER_BLOCK;
	ishift = inumber % INODES_PER_BLOCK;
	disk_read(inode+1, block.data);
	if(block.inode[ishift].isvalid != 1)
		return 0;
	if(block.inode[ishift].size <= offset || block.inode[ishift].size == 0)
		return 0;
	
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}

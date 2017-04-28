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
	union fs_block temp;

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
					break;
				
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
					disk_read(block.inode[j].indirect, temp.data);
					check = 0;
					for (k = 0; k < POINTERS_PER_BLOCK; k++){
						if (temp.pointers[k] == 0)
							break;
						if (check == 0){
							printf("    indirect data blocks:");
							check = 1;
						}
						printf(" %d", temp.pointers[k]);
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

	int ninodeblocks = block.super.ninodeblocks;
	int nblocks = block.super.nblocks;


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

	disk_read(0, block.data);
	struct fs_superblock super = block.super;

	for(i = 0; i < super.ninodeblocks; i++){
		disk_read(i+1, block.data);
		for(j = 0; j < INODES_PER_BLOCK; j++){
			if(block.inode[j].isvalid == 0){
				printf("j = %d\n", j);
				block.inode[j].isvalid = 1;
				block.inode[j].size = 0;
				disk_write(i+1, block.data);
				return (i*INODES_PER_BLOCK) + j + 1;
			}
		}
	}
	return 0;
}

int fs_delete( int inumber )
{
	inumber--;
	if(IS_MOUNTED != 1)
		return 0;
	union fs_block block;
	union fs_block temp;
	int inode, ishift;

	disk_read(0, block.data);
	if (inumber > block.super.ninodes || inumber < 0)
		return 0;

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
		disk_read(block.inode[ishift].indirect, temp.data);
		for(i = 0; i < POINTERS_PER_BLOCK; i++){
			if(temp.pointers[i] == 0)
				break;
			bitmap[temp.pointers[i]] = 0;
			temp.pointers[i] = 0;
		}
		bitmap[block.inode[ishift].indirect] = 0;
		disk_write(block.inode[ishift].indirect, temp.data);
	}

	block.inode[ishift].isvalid = 0;
	block.inode[ishift].size = 0;

	disk_write(inode+1, block.data);



	return 1;
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

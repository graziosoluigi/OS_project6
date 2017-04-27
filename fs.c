
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
	int i;
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	if (block.super.magic == FS_MAGIC)
		printf("    magic number is valid\n");
	else
		printf("    magic number is NOT valid\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
	for(i = 0; i < block.super.ninodeblocks; i++){
		if(block.inode[i].isvalid == 1){
			printf("inode %d:\n", i+2);
			printf("    size: %d bytes\n", block.inode[i].size);
			printf("    direct blocks: \n");//######\n");		//prob. going to need for loop
			printf("    indirect block: %d\n", block.inode[i].indirect);		//prob. going to need for loop
			printf("    inidrect data blocks: ######\n");	//prob goint to need for loop
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

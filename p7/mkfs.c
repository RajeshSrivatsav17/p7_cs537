// #include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "wfs.h"
// #define _FILE_OFFSET_BITS 64
char * disk_img;
int num_inodes =0;
int num_data_blocks =0;
int disk_file;
struct wfs_inode iNode;
struct wfs_sb superBlock;

int readCLA(int argc, char **argv)
{
	int op;
	while ((op = getopt(argc, argv, "d:i:b:")) != -1) {
		switch (op) {														           
            case 'd':
                disk_img = strdup(optarg);
                break;

            case 'i':
                num_inodes = atoi(optarg);
                break;

            case 'b':
                num_data_blocks = atoi(optarg);
                break;

            default:
                // PRINTV("Hmmm...seems like you have not provided the arguments right.\n");
                return 1;
		}
	}
	return 0;
}

int ceil_32(int x) {
    int mask = 31;
    return (x + mask) & ~mask;
}

void createSuperBlock(){
    superBlock.num_inodes = ceil_32(num_inodes);
    superBlock.num_data_blocks = ceil_32(num_data_blocks);
    superBlock.i_bitmap_ptr = sizeof(superBlock);
    superBlock.d_bitmap_ptr = superBlock.i_bitmap_ptr + ceil_32(num_inodes)/8; 
    superBlock.i_blocks_ptr = superBlock.d_bitmap_ptr + ceil_32(num_data_blocks)/8;
    superBlock.d_blocks_ptr = superBlock.i_blocks_ptr + BLOCK_SIZE * ceil_32(num_inodes);
    // printf("Superblock num_inodes =%ld, num_data_blocks = %ld i_bitmap_ptr = %ld d_bitmap_ptr = %ld i_blocks_ptr = %ld d_blocks_ptr = %ld ",
                // superBlock.num_inodes,superBlock.num_data_blocks,superBlock.i_bitmap_ptr,superBlock.d_bitmap_ptr, superBlock.i_blocks_ptr, superBlock.d_blocks_ptr);
    write(disk_file, &superBlock, sizeof(struct wfs_sb));
    lseek(disk_file, sizeof(struct wfs_sb),SEEK_SET);
}

void initializeBitMaps() {
    int inodeBitMap;
    int dataBitMap;
    int no_of_4B_inodes = ceil_32(num_inodes)/(sizeof(int)*8);
    int no_of_4B_dnodes = ceil_32(num_data_blocks)/(sizeof(int)*8);
    inodeBitMap = 1 << 31;
    dataBitMap = 1 << 31;
    for(int i=0; i <no_of_4B_inodes; i++){
        write(disk_file, &inodeBitMap, sizeof(int));
        inodeBitMap = 0;
    }
    for(int i=0; i <no_of_4B_dnodes; i++){
        write(disk_file, &dataBitMap, sizeof(int));
        dataBitMap = 0;
    }
    lseek(disk_file, superBlock.i_blocks_ptr,SEEK_SET);

}

void initializeInodes(){
    int i = 0;
    // Root directory 
    iNode.num       = i;                        /* Inode number */
    iNode.mode      = (mode_t) S_IFDIR | 0755;  /* File type and mode */
    iNode.uid       = (uid_t) getuid();         /* User ID of owner */
    iNode.gid       = (gid_t) getgid();         /* Group ID of owner */
    iNode.size      = BLOCK_SIZE;                /* Total size, in bytes */
    iNode.nlinks    = 0;                        /* Number of links */

    iNode.atim      = (time_t) 0;               /* Time of last access */
    iNode.mtim      = (time_t) 0;               /* Time of last modification */
    iNode.ctim      = (time_t) 0;               /* Time of last status change */
    
    iNode.blocks[0] = (off_t) superBlock.d_blocks_ptr;

    for(int b=1;b<N_BLOCKS;b++) 
        iNode.blocks[b] = (off_t)0;

    write(disk_file, &iNode, sizeof(struct wfs_inode));
    lseek(disk_file, BLOCK_SIZE - sizeof(struct wfs_inode),SEEK_CUR); // each inode is of size BLOCK_SIZE

    for(int i =1; i<ceil_32(num_inodes);i++){
        iNode.num       = i;                        /* Inode number */
        iNode.mode      = (mode_t) 0;               /* File type and mode */
        iNode.uid       = (uid_t) getuid();         /* User ID of owner */
        iNode.gid       = (gid_t) getgid();         /* Group ID of owner */
        iNode.size      = (off_t) 0;                /* Total size, in bytes */
        iNode.nlinks    = 0;                        /* Number of links */

        iNode.atim      = (time_t) 0;               /* Time of last access */
        iNode.mtim      = (time_t) 0;               /* Time of last modification */
        iNode.ctim      = (time_t) 0;               /* Time of last status change */
        
        for(int b=0;b<N_BLOCKS;b++) 
            iNode.blocks[b] = (off_t)0;

        write(disk_file, &iNode, sizeof(struct wfs_inode));
        lseek(disk_file, BLOCK_SIZE - sizeof(struct wfs_inode), SEEK_CUR); // each inode is of size BLOCK_SIZE
    }
}

void initializeRootDataBlock() {
    struct wfs_dentry de;
    strcpy(de.name,".");
    de.num = 0;
    lseek(disk_file, superBlock.d_blocks_ptr, SEEK_SET);
    write(disk_file, &de, sizeof(struct wfs_dentry));
}

int main(int argc, char *argv[]) {

	readCLA(argc, argv);
    disk_file = open(disk_img, O_WRONLY, 0644);

    createSuperBlock();
    initializeBitMaps();
    initializeInodes();
    initializeRootDataBlock();
    close(disk_file);
    return 0;
}
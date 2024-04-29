#include "wfs.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

char * disk_img;
char * mnt_dir;
int find_free_inode() {
    int inode_num = -1;
    struct stat st;
    struct wfs_sb sb;
    int inodeMap;

    int fd = open(disk_img, O_RDWR);
    fstat(fd, &st);
    char * mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));

    mmap_file = mmap_file + sb.i_bitmap_ptr;

    for(int i=0; i<sb.num_inodes/8;i++){
        memcpy(&inodeMap, mmap_file + i*sizeof(int), sizeof(int));
        printf("inode bitmap = %x", inodeMap);
        for (int j = (sizeof(int) * 8) - 1; j >= 0; j--) { // Start from the MSB
            if (!(inodeMap & (1 << j))) { // Check if the j-th bit is zero
                inode_num = i * sizeof(int) * 8 + (31-j); // Calculate the index of the zero bit
                inodeMap |= 1<<j;
                memcpy((void*)(mmap_file + i*sizeof(int)), &inodeMap, sizeof(inodeMap));
                break;
            }
        }
        if(inode_num != -1) break;
    }

    munmap(mmap_file, st.st_size);
    close(fd);
    return inode_num;
}

void printInode(struct wfs_inode * inode){
    printf("Inode number: %d\n", inode->num);
    printf("File type and mode: %o\n", inode->mode);
    printf("User ID of owner: %d\n", inode->uid);
    printf("Group ID of owner: %d\n", inode->gid);
    printf("Total size: %ld bytes\n", inode->size);
    printf("Number of links: %d\n", inode->nlinks);
    printf("Blocks: ");
    for (int i = 0; i < N_BLOCKS; i++) {
        printf("%ld ", inode->blocks[i]);
    }
    printf("\n");
}

static int wfs_getattr(const char *path, struct stat *stbuf) {
    int ret = 0;
    struct stat st;
    char *mmap_file;
    struct wfs_sb sb;
    struct wfs_inode inode;
    struct wfs_dentry de;
    int next_inode = 0;
    // int inode_num;
    printf("my getattr path is : %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    int fd = open(disk_img, O_RDONLY);
    fstat(fd, &st);
    mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    printf("File and Size : %s  %ld\n", disk_img, st.st_size);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    // we got the root inode. Now we have to get the data blocks of the root inode
    int found = 0;
    if(strcmp(path, "/") == 0) 
        // get attr for root dir
    {
        memcpy(&inode, mmap_file + sb.i_blocks_ptr, sizeof(struct wfs_inode));

        stbuf->st_mode  = inode.mode;
        stbuf->st_gid   = inode.gid;
        stbuf->st_uid   = inode.uid;
        stbuf->st_nlink = inode.nlinks;
        stbuf->st_mtime = inode.mtim;

        return ret;
    }
    char *token = strtok(strdup(path), "/");
    // printf("token is : %s\n", token);

    while (token != NULL) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        found = 0;
        memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
        for(int i=0; i<N_BLOCKS; i++){
            for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
                memcpy(&de, mmap_file + inode.blocks[i] + b *sizeof(de), sizeof(struct wfs_dentry));
                if(strcmp(de.name, token) == 0){
                    // inside next directory 
                    found = 1;
                    next_inode = de.num;
                    printInode(&inode);
                    printf("de.name : %s de.num : %d\n", de.name, de.num);
                    break;
                }
            }
            if(found ==1 ) break;
        }
        token = strtok(NULL, "/");
    }
    // next_inode has all the stats needed
    if(!found) {
        munmap(mmap_file, st.st_size);
        close(fd);
        return -ENOENT;
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));

    stbuf->st_mode  = inode.mode;
    stbuf->st_gid   = inode.gid;
    stbuf->st_uid   = inode.uid;
    stbuf->st_nlink = inode.nlinks;
    stbuf->st_mtime = inode.mtim;
    munmap(mmap_file, st.st_size);
    close(fd);
    return ret;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    return 0;

}

static int wfs_mkdir(const char* path, mode_t mode) {

    int ret = 0;
    struct stat st;
    char *mmap_file;
    struct wfs_sb sb;
    struct wfs_inode inode;
    struct wfs_dentry de;
    int next_inode = 0;
    // int inode_num;
    char *token = strtok(strdup(path), "/");
    //token = strtok(NULL, "/"); // Ignore mnt/

    char * dir_name = token;
    printf("directory is : %s\n", dir_name);
    while(token != NULL){
        dir_name = token;
        // Get the next token
        token = strtok(NULL, "/");
    }

    token = strtok(strdup(path), "/");
    printf("token is : %s directory is %s\n", token, dir_name);

    int fd = open(disk_img, O_RDWR);
    fstat(fd, &st);
    mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    // we got the root inode. Now we have to get the data blocks of the root inode

    memcpy(&inode, mmap_file + sb.i_blocks_ptr + 0*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);
    while ((strcmp(token, dir_name) != 0) && (token != NULL)) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + sb.d_blocks_ptr + BLOCK_SIZE * inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
            if(strcmp(de.name, token) == 0){
                // inside next directory 
                next_inode = de.num;
                break;
            }
        }
        memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
        token = strtok(NULL, "/");
    }
    printf("next inode %d \n", next_inode);
    printInode(&inode);

    // next_inode will be the placeholder to place the data entry of this new file
    for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
        memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
        printf("de.name : %s de.num : %d\n", de.name, de.num);
        if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
            strcpy(de.name, strdup(dir_name));
            de.num = find_free_inode();
            // printf("de.name : %s de.num : %d\n", de.name, de.num);
            memcpy((void*)(mmap_file + inode.blocks[0] + b *sizeof(de)), &de, sizeof(de));
            
            time_t current_time;
            time(&current_time);

            inode.num       = de.num;
            inode.mode      = (mode_t) mode;               /* File type and mode */
            inode.uid       = (uid_t) getuid();         /* User ID of owner */
            inode.gid       = (gid_t) getgid();         /* Group ID of owner */
            inode.size      = (off_t) 0;                /* Total size, in bytes */
            inode.nlinks    = 0;                        /* Number of links */

            inode.atim      = (time_t) current_time;               /* Time of last access */
            inode.mtim      = (time_t) current_time;               /* Time of last modification */
            inode.ctim      = (time_t) current_time;               /* Time of last status change */
            memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode ), &inode, sizeof(inode));
            break;
        }
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode , sizeof(inode));
    printInode(&inode);

    printf("End of mkdir\n");
    munmap(mmap_file, st.st_size);
    close(fd);
    return ret;

}

static int wfs_unlink(const char* path) {
    return 0;

}

static int wfs_rmdir(const char* path) {
    return 0;

}

static int wfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

    struct stat st;
    char *mmap_file;
    struct wfs_sb sb;
    struct wfs_inode inode;
    struct wfs_dentry de;
    int next_inode = 0;
    // int inode_num;
    printf("my read path is : %s\n size = %ld offset = %ld", path, size, offset);
    int fd = open(disk_img, O_RDONLY);
    fstat(fd, &st);
    mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    printf("File and Size : %s  %ld\n", disk_img, st.st_size);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    // we got the root inode. Now we have to get the data blocks of the root inode
    int found = 0;

    char *token = strtok(strdup(path), "/");
    printf("token is : %s\n", token);

    while (token != NULL) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        found = 0;
        memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
        for(int i=0; i<N_BLOCKS; i++){
            for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
                memcpy(&de, mmap_file + inode.blocks[i] + b *sizeof(de), sizeof(struct wfs_dentry));
                if(strcmp(de.name, token) == 0){
                    // inside next directory 
                    found = 1;
                    next_inode = de.num;
                    printInode(&inode);
                    printf("de.name : %s de.num : %d %d\n", de.name, de.num, next_inode);
                    break;
                }
            }
            if(found ==1 ) break;
        }
        token = strtok(NULL, "/");
    }
    // next_inode has all the stats needed
    if(!found) {
        munmap(mmap_file, st.st_size);
        close(fd);
        return size;
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);

    if(next_inode == -1) return size;
    memcpy(buf, mmap_file + inode.blocks[0], inode.size);

    printf("End of read %s %ld\n", buf, size);
    munmap(mmap_file, st.st_size);
    close(fd);
    return size;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    return 0;

}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    return 0;

}

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  .unlink  = wfs_unlink,
  .readdir = wfs_readdir,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
};

int main(int argc, char *argv[]) {

    disk_img = strdup(argv[1]);
    mnt_dir = strdup(argv[argc-1]);
    // printf("%s %s", disk_img, mnt_dir);
    argv[1] = argv[0];
    argv++;
    argc--;
    return fuse_main(argc, argv, &ops, NULL);
}


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
    close(fd);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));

    mmap_file = mmap_file + sb.i_bitmap_ptr;

    for (int i = 0; i < sb.num_inodes / 8; i++) {
        memcpy(&inodeMap, mmap_file + i * sizeof(int), sizeof(int));
        printf("inode bitmap = %x\n", inodeMap);
        for (int j = (sizeof(int) * 8 )-1; j >0 ; j--) { 
            if (!(inodeMap & (1 << j))) { 
                inode_num = i * sizeof(int) * 8 + j; 
                inodeMap |= 1 << j;
                printf("new inode bitmap = %x  i = %d\n", inodeMap, i);
                memcpy((void *)(mmap_file + i * sizeof(int)), &inodeMap, sizeof(inodeMap));
                break;
            }
        }
        if (inode_num != -1) break;
    }
    munmap(mmap_file, st.st_size);
    
    return inode_num;
}

int find_free_dnode() {
    int dnode_num = -1;
    struct stat st;
    struct wfs_sb sb;
    int dnodeMap;

    int fd = open(disk_img, O_RDWR);
    fstat(fd, &st);
    char * mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));

    mmap_file = mmap_file + sb.d_bitmap_ptr;

    for (int i = 0; i < sb.num_inodes / 8; i++) {
        memcpy(&dnodeMap, mmap_file + i * sizeof(int), sizeof(int));
        printf("dnodeMap bitmap = %x\n", dnodeMap);
        for (int j = 0; j < sizeof(int) * 8; j++) { 
            if (!(dnodeMap & (1 << j))) { 
                dnode_num = i * sizeof(int) * 8 + (31-j); 
                dnodeMap |= 1 << j;
                memcpy((void *)(mmap_file + i * sizeof(int)), &dnodeMap, sizeof(dnodeMap));
                break;
            }
        }
        if (dnode_num != -1) break;
    }
    munmap(mmap_file, st.st_size);
    printf("dnode = %lx\n",  dnode_num*BLOCK_SIZE + sb.d_blocks_ptr);
 
    return dnode_num*BLOCK_SIZE + sb.d_blocks_ptr;
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

void printSb(struct wfs_sb * sb) {
    printf("num_inodes: %zu\n", sb->num_inodes);
    printf("num_data_blocks: %zu\n", sb->num_data_blocks);
    printf("i_bitmap_ptr: %llx\n", (long long) sb->i_bitmap_ptr);
    printf("d_bitmap_ptr: %llx\n", (long long) sb->d_bitmap_ptr);
    printf("i_blocks_ptr: %llx\n", (long long) sb->i_blocks_ptr);
    printf("d_blocks_ptr: %llx\n", (long long) sb->d_blocks_ptr);
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
    close(fd);

    printf("File and Size : %s  %ld\n", disk_img, st.st_size);

    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    printSb(&sb);

    // we got the root inode. Now we have to get the data blocks of the root inode
    // Root inode
    memcpy(&inode, mmap_file + sb.i_blocks_ptr, sizeof(struct wfs_inode));
    printInode(&inode);
    printf("\nInode Map");
    int inodeMap;
    memcpy(&inodeMap, mmap_file + sb.i_bitmap_ptr, sizeof(int));
    printf("inode map = %x \n", inodeMap);
    printf("\nDnode Map");
    int dnode;
    memcpy(&dnode, mmap_file + sb.d_bitmap_ptr, sizeof(int));
    printf("%x\n", dnode);
    for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
        memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
        printf("de.name : %s de.num : %d\n", de.name, de.num);
    }
    int found = 0;
    if(strcmp(path, "/") == 0) 
        // get attr for root dir
    {
        stbuf->st_mode  = inode.mode;
        stbuf->st_gid   = inode.gid;
        stbuf->st_uid   = inode.uid;
        stbuf->st_nlink = inode.nlinks;
        stbuf->st_mtime = inode.mtim;
        stbuf->st_ctime = inode.ctim;
        stbuf->st_atime = inode.atim;
        stbuf->st_ino   = inode.num;
        stbuf->st_size  = inode.size;

        munmap(mmap_file, st.st_size);
        
        return ret;
    }
    char *token = strtok(strdup(path), "/");
    printf("token is : %s\n", token);

    while (token != NULL) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        printInode(&inode);
        found = 0;
        for(int i=0; i<N_BLOCKS; i++){
            if(inode.blocks[i] == 0) continue;
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
        printf("token is : %s\n", token);
        memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
    }
    // next_inode has all the stats needed
    if(!found) {
        munmap(mmap_file, st.st_size);
        
        return -ENOENT;
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);

    stbuf->st_mode  = inode.mode;
    stbuf->st_gid   = inode.gid;
    stbuf->st_uid   = inode.uid;
    stbuf->st_nlink = inode.nlinks;
    stbuf->st_mtime = inode.mtim;
    stbuf->st_ctime = inode.ctim;
    stbuf->st_atime = inode.atim;
    stbuf->st_ino   = inode.num;
    stbuf->st_size  = inode.size;

    munmap(mmap_file, st.st_size);

    return ret;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
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

    char * file_name = token;
    printf("directory is : %s\n", file_name);
    while(token != NULL){
        file_name = token;
        // Get the next token
        token = strtok(NULL, "/");
    }

    token = strtok(strdup(path), "/");
    printf("token is : %s directory is %s\n", token, file_name);

    int fd = open(disk_img, O_RDWR);
    fstat(fd, &st);
    mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    // we got the root inode. Now we have to get the data blocks of the root inode

    memcpy(&inode, mmap_file + sb.i_blocks_ptr + 0*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);
    while ((strcmp(token, file_name) != 0) && (token != NULL)) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);

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

    // next_inode will be the placeholder to place the data entry of this new file
    int block_not_allocated = 0;
    for(int i=0; i<N_BLOCKS; i++){
        if(inode.blocks[i] != 0)
            block_not_allocated = 1;
    }
    if(!block_not_allocated){
        inode.blocks[0]  = find_free_dnode();
        // WRITE back into inode area
        memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode ), &inode, sizeof(struct wfs_inode));
    }
    printInode(&inode);

    for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
        memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
        printf("de.name : %s de.num : %d\n", de.name, de.num);
        if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
            strcpy(de.name, strdup(file_name));
            de.num = find_free_inode();
            printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
            memcpy((void*)(mmap_file + inode.blocks[0] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
            
            time_t current_time;
            time(&current_time);

            inode.num       = de.num;
            inode.mode      = (mode_t) (S_IFREG | mode);            /* File type and mode */
            inode.uid       = (uid_t) getuid();         /* User ID of owner */
            inode.gid       = (gid_t) getgid();         /* Group ID of owner */
            inode.size      = (off_t) 0;                /* Total size, in bytes */
            inode.nlinks    = 1;                        /* Number of links */

            inode.atim      = (time_t) current_time;               /* Time of last access */
            inode.mtim      = (time_t) current_time;               /* Time of last modification */
            inode.ctim      = (time_t) current_time;               /* Time of last status change */
        
            //inode.blocks[0]  = find_free_dnode();
            for(int b=0;b<N_BLOCKS;b++) 
                inode.blocks[b] = (off_t)0;

            memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * de.num ), &inode, sizeof(struct wfs_inode));
            break;
        }
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * de.num , sizeof(struct wfs_inode));
    printInode(&inode);

    printf("End of mknod\n");
    munmap(mmap_file, st.st_size);
    
    return ret;

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
    close(fd);
    memcpy(&sb, mmap_file, sizeof(struct wfs_sb));
    // we got the root inode. Now we have to get the data blocks of the root inode

    memcpy(&inode, mmap_file + sb.i_blocks_ptr + 0*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);
    while ((strcmp(token, dir_name) != 0) && (token != NULL)) {
        // Traverse through the root inode and find out if there are any data block directory entry with the matching name
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);

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

    // next_inode will be the placeholder to place the data entry of this new file
    int block_not_allocated = 0;
    for(int i=0; i<N_BLOCKS; i++){
        if(inode.blocks[i] != 0)
            block_not_allocated = 1;
    }
    if(!block_not_allocated){
        inode.blocks[0]  = find_free_dnode();
        // WRITE back into inode area
        memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode ), &inode, sizeof(struct wfs_inode));
    }
    printInode(&inode);

    for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
        memcpy(&de, mmap_file + inode.blocks[0] + b *sizeof(de), sizeof(struct wfs_dentry));
        printf("de.name : %s de.num : %d\n", de.name, de.num);
        if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
            strcpy(de.name, strdup(dir_name));
            de.num = find_free_inode();
            printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
            memcpy((void*)(mmap_file + inode.blocks[0] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
            
            time_t current_time;
            time(&current_time);

            inode.num       = de.num;
            inode.mode      = (mode_t) (S_IFDIR | mode);            /* File type and mode */
            inode.uid       = (uid_t) getuid();         /* User ID of owner */
            inode.gid       = (gid_t) getgid();         /* Group ID of owner */
            inode.size      = (off_t) BLOCK_SIZE;        /* Total size, in bytes */
            inode.nlinks    = 1;                        /* Number of links */

            inode.atim      = (time_t) current_time;               /* Time of last access */
            inode.mtim      = (time_t) current_time;               /* Time of last modification */
            inode.ctim      = (time_t) current_time;               /* Time of last status change */
        
            //inode.blocks[0]  = find_free_dnode();
            for(int b=0;b<N_BLOCKS;b++) 
                inode.blocks[b] = (off_t)0;

            memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * de.num ), &inode, sizeof(struct wfs_inode));
            break;
        }
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * de.num , sizeof(struct wfs_inode));
    printInode(&inode);

    printf("End of mkdir\n");
    munmap(mmap_file, st.st_size);
    
    return ret;
}

static int wfs_unlink(const char* path) {
    return 4;

}

static int wfs_rmdir(const char* path) {
    return 3;

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
    close(fd);

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
        
        return 0;
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);

    if(next_inode == -1) return 0;
    int min_size = inode.size < size ? inode.size : size;

    memcpy(buf, mmap_file + inode.blocks[0],min_size);

    munmap(mmap_file, st.st_size);
    
    printf("End of read %s %ld\n", buf, size);

    return size;
}

static int wfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    struct stat st;
    char *mmap_file;
    struct wfs_sb sb;
    struct wfs_inode inode;
    struct wfs_dentry de;
    int next_inode = 0;
    // int inode_num;
    printf("my write path is : %s  buf = %s \n size = %ld offset = %ld", path, buf , size, offset);
    int fd = open(disk_img, O_RDWR);
    fstat(fd, &st);
    mmap_file = (char *) mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

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
        return 0;
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));
    printInode(&inode);

    if(next_inode == -1) return 0;

    // Now we have to see if the data block is allocated. 
    // How to do that? 
    //  if the size allocated < offset - if yes, then we need to allocate a block in that region
    int tmp_offset = offset+size;
    int block_index = 0;

    while(tmp_offset > inode.size) {
        // Need to allocate a block 
        // for now allocate the first block .later modify this logic such taht we allocate the nth block based on the offset / BLOCKSIZE value
        // if(b>DIRECT_BLOCK) put it in indirect block
        inode.blocks[block_index] = find_free_dnode();
        inode.size += BLOCK_SIZE;
        printf("dnode %lx size %ld\n",inode.blocks[block_index], inode.size);

        memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * inode.num ), &inode, sizeof(struct wfs_inode));

        tmp_offset -= BLOCK_SIZE;
        block_index++;
    }

    int num_blocks = (size % BLOCK_SIZE) != 0? (size / BLOCK_SIZE)+1 : (size / BLOCK_SIZE);
    printf("num blocks %d\n", num_blocks);
    printInode(&inode);

    for(int n=0; n<num_blocks;n++){
        memcpy((void *)(mmap_file + inode.blocks[n]), buf, BLOCK_SIZE);
    }

    munmap(mmap_file, st.st_size);

    return size;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    return 1;

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


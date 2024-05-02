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
#include <math.h>

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

    for (int i = 0; i < sb.num_data_blocks / 8; i++) {
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
    printf("mknod is : %s\n", file_name);
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
    printSb(&sb);
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
    int num_blocks = 0;
    for(int i=0; i<N_BLOCKS; i++){
        if(inode.blocks[i] != 0)
            num_blocks++;
    }
    printf("num blcoks %d\n", num_blocks);

    printInode(&inode);
    int found = 0;
    for(int i=0; i<num_blocks;i++) {
        if(inode.blocks[i] == 0) continue;
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[i] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);
            if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
                strcpy(de.name, strdup(file_name));
                de.num = find_free_inode();
                printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
                memcpy((void*)(mmap_file + inode.blocks[i] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
                
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
                found = 1;
                break;
            }
        }
    }

    if(!found){
        // allocate a new inode block and hold this new file's info
        inode.blocks[num_blocks]  = find_free_dnode();
        memset(mmap_file + inode.blocks[num_blocks] , 0, BLOCK_SIZE);
        memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode), &inode, sizeof(struct wfs_inode));
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[num_blocks] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);
            if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
                strcpy(de.name, strdup(file_name));
                de.num = find_free_inode();
                printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
                memcpy((void*)(mmap_file + inode.blocks[num_blocks] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
                
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
                found = 1;
                break;
            }
        }
    }
    memcpy(&inode, mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * next_inode , sizeof(struct wfs_inode));
    printInode(&inode);
    memcpy(&sb, mmap_file , sizeof(struct wfs_sb));
    printSb(&sb);
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

    int num_blocks = 0;
    int found_place = 0;
    for(int i=0; i<N_BLOCKS; i++){
        if(inode.blocks[i] != 0)
            num_blocks++;
    }
    printInode(&inode);

    for(int i=0; i<num_blocks;i++){
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[i] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);
            if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
                strcpy(de.name, strdup(dir_name));
                de.num = find_free_inode();
                printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
                memcpy((void*)(mmap_file + inode.blocks[i] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
                
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
                found_place = 1;
                break;
            }
        }
        if(found_place == 1) break;
    }

    if(!found_place) // need new block for new space 
    {
        for(int i=0; i<N_BLOCKS; i++){
            if(inode.blocks[i] == 0){
                block_not_allocated = i;
                break;
            }
        }
        inode.blocks[block_not_allocated]  = find_free_dnode();
        memset(mmap_file + inode.blocks[block_not_allocated] , 0, BLOCK_SIZE);
        // WRITE back into inode area
        memcpy((void*)(mmap_file + + sb.i_blocks_ptr + BLOCK_SIZE * next_inode), &inode, sizeof(struct wfs_inode));
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[block_not_allocated] + b *sizeof(de), sizeof(struct wfs_dentry));
            printf("de.name : %s de.num : %d\n", de.name, de.num);
            if(de.num == 0 && (strcmp(de.name, ".") != 0)){ // other entry other than "."
                strcpy(de.name, strdup(dir_name));
                de.num = find_free_inode();
                printf("Allocated de.name : %s de.num : %d\n", de.name, de.num);
                memcpy((void*)(mmap_file + inode.blocks[block_not_allocated] + b *sizeof(de)), &de, sizeof(struct wfs_dentry));
                
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
                found_place = 1;
                break;
            }
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
    // int min_size = inode.size < size ? inode.size : size;

    // memcpy(buf, mmap_file + inode.blocks[0],min_size);

    // munmap(mmap_file, st.st_size);
    
    // printf("End of read %s %ld\n", buf, size);

    // have to read size number of bytes into buf and also use offset in read operation here 
    int bytes_read = 0;
    int current_block = offset/ BLOCK_SIZE;
    while ((bytes_read < size) && (bytes_read < inode.size)) {
        off_t block_offset;
        if(current_block > D_BLOCK) { // find block offset
            memcpy(&block_offset, (mmap_file + inode.blocks[IND_BLOCK] + sizeof(int)/*num_indirect_blocks*/ + sizeof(off_t)*(current_block-D_BLOCK-1)),  sizeof(off_t));
        }
        else{
            block_offset = inode.blocks[current_block];
        }
        printf("bytes_read : %d block_offset : %ld current block = %d \n",bytes_read, block_offset, current_block);

        int bytes_to_read = inode.size < (size - bytes_read) ? inode.size : (size - bytes_read);
        int mem_cpy_size = bytes_to_read < BLOCK_SIZE ? bytes_to_read : BLOCK_SIZE;
        // Copy data from buffer to allocated block
        memcpy(buf + bytes_read, (void *)(mmap_file + block_offset), mem_cpy_size);

        // Update bytes written
        bytes_read += mem_cpy_size;
        block_offset = 0;
        current_block++;
    }
    // printf("buf %s\n", buf);
    munmap(mmap_file, st.st_size);
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
    int total_reqd_space = offset+size;
    // int block_index = offset/BLOCK_SIZE; // to see where to start from
    int num_blocks_to_be_allocated = 0;
    if(inode.size >= total_reqd_space) num_blocks_to_be_allocated = 0; 
    else {
        num_blocks_to_be_allocated = ((total_reqd_space-inode.size)%BLOCK_SIZE) ==0 ? (total_reqd_space-inode.size)/BLOCK_SIZE : (total_reqd_space-inode.size)/BLOCK_SIZE +1;
    }
    int block_index = 0;
    printf("num blocks %d total space reqd = %d inode size = %ld\n", num_blocks_to_be_allocated, total_reqd_space, inode.size);

    while(num_blocks_to_be_allocated>0) {
        if(block_index <= D_BLOCK) {
            if(inode.blocks[block_index] != 0) {
                block_index++;
                continue;
            }
            else{
                inode.blocks[block_index] = find_free_dnode();
                inode.size += BLOCK_SIZE;
                num_blocks_to_be_allocated--;

                printf("new block allocated : %ld , num_blocks_to_be_allocated = %d, block_index = %d \n",inode.blocks[block_index], num_blocks_to_be_allocated, block_index);
                block_index++;
            }
        }
        // Need to allocate a block 
        // for now allocate the first block .later modify this logic such taht we allocate the nth block based on the offset / BLOCKSIZE value
        // if(b>DIRECT_BLOCK) put it in indirect block
        //if(block_index > D_BLOCK) { // Time to insert indirect blocks
        else{
            off_t new_block_offset;
            int num_indirect_blocks = 0; // Specifies how many current indirect blocks are present
            if(inode.blocks[IND_BLOCK] == 0)
                inode.blocks[IND_BLOCK] = find_free_dnode();
            new_block_offset = find_free_dnode();

            // Increment number of indirect blocks
            memcpy(&num_indirect_blocks, (mmap_file + inode.blocks[IND_BLOCK]), sizeof(int));
            num_indirect_blocks++;
            memcpy((void*)(mmap_file + inode.blocks[IND_BLOCK]), &num_indirect_blocks,  sizeof(int));
            num_blocks_to_be_allocated--;
            // write back the block offset of the new indirect block
            memcpy((void*)(mmap_file+ inode.blocks[IND_BLOCK] + sizeof(int)/*num_indirect_blocks*/ + sizeof(off_t)*(num_indirect_blocks-1)), &new_block_offset, sizeof(off_t));
            printf("new block allocated : %ld , num_blocks_to_be_allocated = %d, block_index = %d \n",new_block_offset, num_blocks_to_be_allocated, num_indirect_blocks);
            inode.size += BLOCK_SIZE;
        }
        printf("dnode %lx size %ld\n",inode.blocks[block_index], inode.size);

        memcpy((void*)(mmap_file + sb.i_blocks_ptr + BLOCK_SIZE * inode.num ), &inode, sizeof(struct wfs_inode));

    }

    printInode(&inode);

    int bytes_written = 0;
    int current_block = offset/ BLOCK_SIZE;
    off_t offset_inside_block = offset%BLOCK_SIZE;
    while (bytes_written < size) {
        off_t block_offset;
        if(current_block > D_BLOCK) { // find block offset
            memcpy(&block_offset, (mmap_file + inode.blocks[IND_BLOCK] + sizeof(int)/*num_indirect_blocks*/ + sizeof(off_t)*(current_block-D_BLOCK-1)),  sizeof(off_t));
        }
        else{
            block_offset = inode.blocks[current_block];
        }
        printf("Bytes written : %d block_offset : %ld current block = %d \n",bytes_written, block_offset, current_block);

        int bytes_to_write = (size - offset_inside_block) - bytes_written;
        int mem_cpy_size = bytes_to_write < BLOCK_SIZE ? bytes_to_write : BLOCK_SIZE;
        // Copy data from buffer to allocated block
        memcpy((void *)(mmap_file + block_offset + offset_inside_block), buf + bytes_written, mem_cpy_size);

        // Update bytes written
        bytes_written += mem_cpy_size;
        block_offset = 0;
        offset_inside_block = 0;
        current_block++;
    }

    munmap(mmap_file, st.st_size);

    return size;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

/** Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 
typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
				const struct stat *stbuf, off_t off);*/
    int ret = 0;
    struct stat st;
    char *mmap_file;
    struct wfs_sb sb;
    struct wfs_inode inode;
    struct wfs_inode inode_readdir;
    struct wfs_dentry de;
    int next_inode = 0;
    struct stat *stbuf = (struct stat *) malloc(sizeof(struct stat));
    // int inode_num;
    printf("my readdir path is : %s\n", path);
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
    char *token = strtok(strdup(path), "/");
    printf("token is : %s\n", token);
    if(strcmp(path, "/") == 0) 
        // get attr for root dir
    {
        found = 1;
    }
    else {
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
    }

    // next_inode has all the stats needed
    if(!found) {
        munmap(mmap_file, st.st_size);
        return -ENOENT;
    }

    memcpy(&inode, mmap_file + sb.i_blocks_ptr + next_inode*BLOCK_SIZE, sizeof(struct wfs_inode));

    for(int i=0; i<N_BLOCKS; i++){
        if(inode.blocks[i] == 0) continue;
        printf("block #%d inode.blocks[i] %ld", i, inode.blocks[i]);
        for(int b=0; b<(BLOCK_SIZE/sizeof(de));b++){
            memcpy(&de, mmap_file + inode.blocks[i] + b *sizeof(de), sizeof(struct wfs_dentry));
            // if(strcmp(de.name, token) == 0){
            // inside next directory 
            // found = 1;
            if(de.num!=0){
                printf("File name : %s de.num : %d\n", de.name, de.num);
                memcpy(&inode_readdir, mmap_file + sb.i_blocks_ptr + de.num*BLOCK_SIZE, sizeof(struct wfs_inode));
                stbuf->st_mode  = inode_readdir.mode;
                stbuf->st_gid   = inode_readdir.gid;
                stbuf->st_uid   = inode_readdir.uid;
                stbuf->st_nlink = inode_readdir.nlinks;
                stbuf->st_mtime = inode_readdir.mtim;
                stbuf->st_ctime = inode_readdir.ctim;
                stbuf->st_atime = inode_readdir.atim;
                stbuf->st_ino   = inode_readdir.num;
                stbuf->st_size  = inode_readdir.size;
                // printf("Inode number: %d\n", inode_readdir.num);
                filler (buf, de.name, stbuf, offset);
            }
        }
    }


    munmap(mmap_file, st.st_size);

    return ret;
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


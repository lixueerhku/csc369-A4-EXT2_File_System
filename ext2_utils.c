
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ext2.h"
#include "ext2_utils.h"

unsigned char *disk;
struct ext2_super_block *sb;
struct ext2_group_desc *gd;

/**
 *This function reads the image from the input file path.
 *It initializes the disk, super block, group descihper if read is sucessful.
 */
void load_image(const char *image_path) {
  int fd = open(image_path, O_RDWR);

  disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
  gd = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);

  if(disk == MAP_FAILED) {
    perror("Error: load_image() mmap fail");
    exit(EXIT_FAILURE);
  }
}

/**
 *This function returns the pointer to the inode bitmap 
 */
unsigned char *get_inode_bitmap() {
    return (unsigned char*)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_bitmap);
}

/**
 *This function returns the pointer to the block bitmap 
 */
unsigned char *get_block_bitmap() {
    
    return (unsigned char*)(disk + EXT2_BLOCK_SIZE * gd->bg_block_bitmap);
}

/**
 *This function returns the pointer to the inode table
 */
struct ext2_inode *get_inode_table() {
    
    return (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);
}

/**
 *This function returns a character ('l', 'f', 'd' or 'o') represent the type of the given inode.
 */
char get_inode_type(struct ext2_inode *inode){
    
    unsigned short imode = inode->i_mode;

    if((imode & EXT2_S_IFLNK) == EXT2_S_IFLNK){
        return 'l'; //symbolic link
    }else if((imode & EXT2_S_IFREG) == EXT2_S_IFREG){
        return 'f'; //regular file
    }else if((imode & EXT2_S_IFDIR) == EXT2_S_IFDIR){
        return 'd'; //directory
    }else{
        return 'o'; //other types
    }
}

/**
 *This function returns the inode number of the given inode. (NUMBER = INDEX + 1)
 */
int inode_num_of(struct ext2_inode *inode){
    
    struct ext2_inode *inode_table_start = (struct ext2_inode *)(disk + EXT2_BLOCK_SIZE * gd->bg_inode_table);

    return (inode - inode_table_start)/sizeof(struct ext2_inode);
}


/**
 * Check whether the file path starts with a '/'
 */

//static int is_absolute_path(const char *path) {
//    return strncmp(path, "/", 1) == 0;
//}

/*
 * This function returns the file name form the path
 * Input: path to file 
 * e.g. If the path is '/home/level1/level2/file_name', it should return 'file_name'
 */
char *get_file_name(char *path_to_file) {

    char path_copy[strlen(path_to_file) + 1];
    strcpy(path_copy, path_to_file);
    
    char *token = strtok(path_copy, "/");
    char *next_token = token;
    
    while (next_token != NULL) {
        token = next_token;
        next_token = strtok(NULL, "/");
    }


    
    return token;
}

/*
 * This function checks whether the resource is in use
 * Input: bitmap of inodes or blocks; num: the resource number(index = number - 1)
 * It eturns 1 if the bit in the bitmap is 1, return 0 otherwise.
 */
int check_resource_in_use(unsigned char *bitmap, int num){
    int index = num - 1;
    int byte_idx = index / 8;
    int bit = index % 8;//The offset in the byte
    char byte = bitmap[byte_idx];//The byte in the bitmap
    return (byte) & (1 << bit);
}

/**
 * This function finds the first unused resource(inode or block) in the given bitmap and set it to 1.
 * Input: bitmap and num_bytes (the number of bytes in the bitmap, i.e. number of inodes or blocks / 8)
 * It returns the coresponding inode or block number (Number = Index + 1).
 */
int allocate_resource(unsigned char *bitmap, int num_bytes){
    int i;
    for(i = 0; i < num_bytes; i++){
        unsigned char byte = bitmap[i];

        int j;
        for(j = 0; j < 8; j++){
            int bit = byte & (1<<j) ? 1 : 0;

            //Find an unused resource
            if(bit == 0){
                //Set that bit to be 1
                bitmap[i] |= 1 << j;

                //Return the corresponding inode or block number
                //Number = Index + 1
                return (i * 8 + j + 1);
            }
        }
    }
    //No empty resources
    exit(ENOMEM);
}

/**
 * This function finds an empty inode and allocates it to the file
 * Input: the file type(EXT2_FT_UNKNOWN, EXT2_FT_REG_FILE, EXT2_FT_DIR, EXT2_FT_SYMLINK)
 * It returns the inode number
 */
int allocate_inode(unsigned char file_type){

    unsigned char *inode_bitmap = get_inode_bitmap();
    int inodes_count = sb->s_inodes_count;
    
    //The number of byte in the bitmap == inodes_count / 8 
    //One byte represents 8 inodes
    int inode_num = allocate_resource(inode_bitmap, inodes_count/8);

    //Get the corresponding imode
    unsigned short imode;
    if(file_type == EXT2_FT_DIR){
        imode = EXT2_S_IFDIR;
    }else if(file_type == EXT2_FT_REG_FILE){
        imode = EXT2_S_IFREG;
    }else{
        imode = EXT2_S_IFLNK;
    }
    
    unsigned int current_time = (unsigned int) time(NULL);

    // First clean the allocated inode
    struct ext2_inode *allocated_inode = get_inode_table() + inode_num - 1;
    memset(allocated_inode, 0, sizeof(struct ext2_inode));

    //Update inode information
    allocated_inode->i_mode  |= imode;
    allocated_inode->i_ctime = current_time;
    allocated_inode->i_atime = current_time;
    allocated_inode->i_mtime = current_time;

    //Update infomation in group descipher and super block
    gd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;

    return inode_num;
}

/**
 * This function finds an empty block and returns its number
 */
int allocate_block(){
    
    unsigned char *block_bitmap = get_block_bitmap();
    int blocks_count = sb->s_blocks_count;

    // Number of bytes of the bitmap is blocks_count/8
    int block_num = allocate_resource(block_bitmap, blocks_count/8);

    // Clean the allocated block
    unsigned char *new_block = disk + (block_num * EXT2_BLOCK_SIZE);
    memset(new_block, 0, EXT2_BLOCK_SIZE);

    //Update sb and gd information
    gd->bg_free_blocks_count--;
    sb->s_free_blocks_count--;

    return block_num;

}

/**
 * This function finds the second last directory in the path
 * It returns its inode number if successful
 * Return ENOENT if the path is invalid
 * e.g. If the path is 'home/level1/file1', it tries to find the inode number of 'level1'
 */
int second_last_dir_inode(char *path){

    //Check whether the path  is an absolute path
    if(strncmp(path, "/", 1) != 0){
        //It is not an absolute path
        exit(ENOENT);
    }

    //Check whether the path is root 
    if (strcmp("/", path) == 0) {
       //Path is root
        exit(EEXIST);
    }

    struct ext2_inode *inodes = get_inode_table();

    //Split the target path and store each directory in dir_name
    char *dir_name;
    dir_name = strtok(path, "/");


    struct ext2_inode *curr_inode = &inodes[EXT2_ROOT_INO - 1];//In root
    struct ext2_dir_entry *curr_entry = find_entry(curr_inode, dir_name);//Search in root

    int inode_num = EXT2_ROOT_INO;

    //After the loop, curr_inode would be the inode of the destination directory
    while(dir_name != NULL){
        
        char *next_dir = strtok(NULL, "/");

        // Haven't reach the bottom of the path
        if(next_dir != NULL){
            //Check if the entry exists
            if(curr_entry == NULL){
                //Invalid path
                exit(ENOENT);
            }
            //Check if current entry is a directory
            if(curr_entry->file_type != EXT2_FT_DIR){
                //Invalid path
                exit(ENOENT);
            }
        }else{
            return inode_num; 
        }

        //Go to next directory along the target path
        dir_name = next_dir;
        curr_inode =  &inodes[curr_entry->inode - 1];
        inode_num = curr_entry->inode;
        curr_entry = find_entry(curr_inode, dir_name);

    }

    return inode_num;

}



/**
 * This function initiate a directory entry at the location given by the input 'entry'
 *
 */
void init_dir_entry(struct ext2_dir_entry *entry, unsigned int inode_num, unsigned short rec_len, int name_len, char *name, unsigned char file_type){
    
    // Increment inode link count
    struct ext2_inode * inodes = get_inode_table();
    inodes[inode_num - 1].i_links_count += 1;
    //Initialize entry
    entry->inode = inode_num;
    entry->rec_len = rec_len;
    entry->name_len = name_len;
    entry->file_type = file_type;
    strcpy(entry->name, name);


}

/*
 * This function returns the entry given the file_name inside the given inode.
 */
struct ext2_dir_entry *find_entry(struct ext2_inode *inode, char *file_name) {

    //Check if the inode type is directory
    if(get_inode_type(inode) != 'd'){
        exit(EXIT_FAILURE);
    }


	int j;
    for (j = 0 ; j < inode->i_blocks / 2 ; j++) {
        int i_block = inode->i_block[j];
        int curr_len = 0;

        while (curr_len < EXT2_BLOCK_SIZE) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * i_block + curr_len);
            if (strlen(file_name) == ((int) entry->name_len)) {  
                if (strncmp(file_name, entry->name, strlen(file_name)) == 0) {//entry->name matches file_name
                    return entry;
                }
            }
            curr_len += entry->rec_len;
        }
    }

    return NULL;
}

/*
 * This function returns the last directory entry
 * inside the given block. (i_block is the block index)
 */
struct ext2_dir_entry *find_last_entry(int i_block){

    int curr_len = 0;
    struct ext2_dir_entry *entry;
    while (curr_len < EXT2_BLOCK_SIZE) {
        entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * i_block + curr_len);
        curr_len += entry->rec_len;
    }
    return entry;
}

/*
 * This function returns the actual entry length required by (ext2_dir_entry + dir_name)
 * It must be a multiple of 4
 */
int actual_entry_len(struct ext2_dir_entry *entry){
    int actual_len = sizeof(struct ext2_dir_entry) + entry->name_len;
    while(actual_len % 4 != 0){
        actual_len += 1;
    }
    return actual_len;
}

/*
 * This function inserts a new entry in the directoty
 * Input: dir_inode: the inode of the give directory where to insert a new file, finode: inode number of the new file (number = index + 1), 
 * fname: new file name, ftype: new file type(EXT2_FT_UNKNOWN, EXT2_FT_REG_FILE, EXT2_FT_DIR, EXT2_FT_SYMLINK)
 * Return: the new dir entry
 */
struct ext2_dir_entry *insert_dir_entry(struct ext2_inode *dir_inode, unsigned int finode, char *fname, unsigned char ftype){
    //Check if inode is allocated
    if(finode == 0){
        return NULL;
    }

    //Check if the file name already exists
	if(find_entry(dir_inode, fname) != NULL){
        exit(EEXIST);
	}

	//Get the name length and entry length
	int name_len = strlen(fname);
    if (name_len > EXT2_NAME_LEN){
        exit(EXIT_FAILURE);
    }
	
	//rec_len needs to be a multiple of 4.
	int rec_len = sizeof(struct ext2_dir_entry) + name_len;
	while(rec_len % 4 != 0){
		rec_len += 1;
	}

	int i;
    //The first 12 direct blocks
	for(i = 0; i < EXT2_DIRECT_BLOCK_NUM; i++){

        // The current block is not in use
        if((dir_inode->i_block)[i] == 0){
            int block_num = allocate_block();

            //Update inode information
            (dir_inode->i_block)[i] = block_num;
            dir_inode->i_size += EXT2_BLOCK_SIZE;
            //Allocate a new block increases 2 DISK SECTORS
            dir_inode->i_blocks += EXT2_BLOCK_SIZE / EXT2_SECTOR_SIZE;

            struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + block_num * EXT2_BLOCK_SIZE);

            init_dir_entry(entry, finode, EXT2_BLOCK_SIZE, name_len, fname, ftype);

            return entry;

        }
        // The current block is in use
        // Find the last entry and check whether we can put our new entry here
        else{
            
            
            unsigned char *block_start = disk + ((dir_inode->i_block)[i] * EXT2_BLOCK_SIZE);
            unsigned char *block_end = block_start + EXT2_BLOCK_SIZE;
            
            unsigned char *curr_pos = block_start;
            struct ext2_dir_entry *first_entry = (struct ext2_dir_entry *)block_start;
            curr_pos += first_entry->rec_len;

            //After the loop, last_entry would be the the last entry in the block
            struct ext2_dir_entry *last_entry = first_entry;
            while(curr_pos != block_end){
                last_entry = (struct ext2_dir_entry *)curr_pos;
                curr_pos += last_entry->rec_len;
            }

            int actual_len = actual_entry_len(last_entry);
            //There is enough place to put new entry at the end of the blcok
            if((last_entry->rec_len - actual_len) >= rec_len){
                
                //Insert a new entry after the last entry
                int new_ren_len = last_entry->rec_len - actual_len;
                
                //Update rec_len of the last entry
                last_entry->rec_len = actual_len;
                struct ext2_dir_entry *new_entry  = (struct ext2_dir_entry *)(block_end - new_ren_len);

                init_dir_entry(new_entry, finode, new_ren_len, name_len, fname, ftype);

                return new_entry;
            }
		}
	}

    // If all the 12 blocks are full
    return NULL;

}

//This function creates a file onto the specified location on the disk
// e.g. If path_to_dest is '/home/level1/new_file', it should create new_file in directory '/home/level1'.
// finode is the inode number for the newly created file. If finode is 0, it will allocate a new inode first.
// It returns the dir_entry of the newly created file if successful
// Return EEXIST if the file name already exists and ENOENT if the path is invalid
struct ext2_dir_entry* create_file(char *path_to_dest, unsigned int finode, char file_type){

    //Create a copy so that path_to_dest will not be changed
    char path_copy[strlen(path_to_dest) + 1];
    strcpy(path_copy, path_to_dest);

    //Get the inode of the second last directory
    int parent_inode_num = second_last_dir_inode(path_copy);
    struct ext2_inode *parent_inode = &get_inode_table()[parent_inode_num - 1];
    
    //Allocate a new inode for the new file
    if(finode == 0){
        finode = allocate_inode(file_type); 
    }
    
    //Create a copy of path_to_dest so that path_to_dest won't be changed
    char path_to_dest_copy[strlen(path_to_dest) + 1];
    strcpy(path_to_dest_copy, path_to_dest);

    char* file_name = get_file_name(path_to_dest_copy);
    char const_file_name[EXT2_NAME_LEN + 1];//Make the name constont
    int i;
    for(i = 0; i < strlen(file_name); i++){
        const_file_name[i] = file_name[i];
    }
    const_file_name[strlen(file_name)] = '\0';

    //Insert the entry for the new file into the current inode
    return insert_dir_entry(parent_inode, finode, const_file_name, file_type);

}

//This function marks the resource as in use in the bitmap
//Input: the number of the resource; is_inode: 1 if the resource is an inode
void set_resource_in_use(unsigned int resource_num, int is_inode){
    unsigned char *bitmap;
    if(is_inode == 1){
        bitmap = get_inode_bitmap();
        gd->bg_free_inodes_count--;
        sb->s_free_inodes_count--;
    }else{
        bitmap = get_block_bitmap();
        gd->bg_free_blocks_count--;
        sb->s_free_blocks_count--;
    }
    
    int idx = resource_num - 1;
    int byte_idx = idx / 8;
    int bit = idx % 8;
    
    // Set the corresponding bit to 1
    bitmap[byte_idx] |= 1 << bit;
    
}

/*
 * This function free the inode given its number
 * It updates the inode bitmap(1->0) and other information in sb and gd.
 */
void free_inode(unsigned int inode_num) {
    
    unsigned char *inode_bitmap = get_inode_bitmap();
    
    int index = inode_num - 1;
    
    int byte_index = index / 8;
    int bit_offset = index % 8;
    
    // Set the corresponding bit in bitmap to 0
    inode_bitmap[byte_index] &= (~(1 << bit_offset));
    
    gd->bg_free_inodes_count++;
    sb->s_free_inodes_count++;
}

/*
 * This function free the block given its number
 * It updates the block bitmap(1->0) and other information in sb and gd.
 */
void free_block(unsigned int block_num) {
    
    unsigned char *block_bitmap = get_block_bitmap();
    
    int index = block_num - 1;
    int byte_index = index / 8;
    int bit_offset = index % 8;
    
    // Set the corresponding bit in bitmap to 0
    block_bitmap[byte_index] &= (~(1 << bit_offset));
    
    gd->bg_free_blocks_count++;
    sb->s_free_blocks_count++;
}

/*
 * This function frees all data blocks in the given inode.
 * This function is called when the i_links_count of this inode becomes 0
 */
void free_data_blocks(struct ext2_inode *inode) {
    
    if(inode->i_links_count > 0) {
        //The inode still has links
        exit(EXIT_FAILURE);
    }
    
    int i;
    // Free direct blocks
    for (i = 0; i < EXT2_DIRECT_BLOCK_NUM; i++) {
        if(inode->i_block[i] != 0){//The block is in use
            unsigned int block_num = inode->i_block[i];
            free_block(block_num);
        }
    }
    
    //Since the disk only has 128 blocks, we will use one indirect block (12-th) at most
    unsigned int indirect_block_num = inode->i_block[EXT2_DIRECT_BLOCK_NUM];
    
    if (indirect_block_num != 0) {// The indirect block (12-th) is in use

        // Get the indirect block
        unsigned int *indirect_block = (unsigned int *)(disk + indirect_block_num * EXT2_BLOCK_SIZE);
        
        int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);//1024 / 4 = 256 blocks
        
        int j;
        for(j = 0; j < max_indirect_blocks; j++){
            //The direct block store in indirect_block
            unsigned int direct_block_num = indirect_block[j];
            if(direct_block_num != 0){
                free_block(direct_block_num);
            }
        }
        //Free the indirect block
        free_block(indirect_block_num);
    }

}

/*
 * This function deecrements the inode's i_links_count by 1.
 * Free all the data blocks if its links_count becomes 0.
 */
void unlink_inode(unsigned int inode_num) {
    
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[inode_num-1];
    
    if (inode->i_links_count == 0) {
        //The inode doesn't have any link
        exit(EXIT_FAILURE);
    }
    
    inode->i_links_count--;
    
    // If the i_link_count reaches 0, free all the data blocks of the inode
    if (inode->i_links_count == 0) {
        // Update deletion time
        unsigned int current_time = (unsigned int) time(NULL);
        inode->i_dtime = current_time;
        
        free_data_blocks(inode);
        free_inode(inode_num);
    }
}




#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include "ext2.h"
#include "ext2_utils.h"



//This function returns the number of bit 0 on the given bitmap 
//Input: bitmap and the number of bytes in the bitmap
int num_of_zero_in_bitmap(unsigned char *bitmap, int num_bytes){
    int count = 0;
    int i;
    for(i = 0; i < num_bytes; i++){
        unsigned char byte = bitmap[i];

        int j;//8 bits in one byte
        for(j = 0; j < 8; j++){
            int bit = byte & (1<<j) ? 1 : 0;
            //Find an bit 0
            if(bit == 0){
                count++;
            }
        }
    }
    return count;
}

/*
 * This function makes sure the superblock and block group counters for free inodes
 * matches the number of free inodes in the inode bitmap
 * It will fix the mismatches in the sb or bg and output the corresponding message
 * Return the the total number of fixes (in absolute value)
 */
unsigned int match_free_inodes_count(){

    //Get the number of free inodes in its bitmap
    unsigned char *bitmap = get_inode_bitmap();
    int num_bytes = sb->s_inodes_count / 8;
    int free_inode_count = num_of_zero_in_bitmap(bitmap, num_bytes);

    //Check super block
    int diff_in_sb = abs(free_inode_count - (int) sb->s_free_inodes_count);//the difference in absolute value
    if(diff_in_sb != 0){
        sb->s_free_inodes_count = free_inode_count;
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n", diff_in_sb);
    }
    
    //Check group descipher
    int diff_in_gd = abs(free_inode_count - (int) gd->bg_free_inodes_count);//the difference in absolute value
    if(diff_in_gd != 0){
        gd->bg_free_inodes_count = free_inode_count;
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", diff_in_gd);
    }

    return diff_in_sb + diff_in_gd;
}

/*
 * This function makes sure the superblock and block group counters for free blocks
 * matches the number of free blocks in the block bitmap
 * It will fix the mismatches in the sb or bg and output the corresponding message
 * Return the the total number of fixes (in absolute value)
 */
unsigned int match_free_blocks_count(){

    //Get the number of free blocks in its bitmap
    unsigned char *bitmap = get_block_bitmap();
    int num_bytes = sb->s_blocks_count / 8;
    int free_block_count = num_of_zero_in_bitmap(bitmap, num_bytes);

    //Check super block
    int diff_in_sb = abs(free_block_count - (int) sb->s_free_blocks_count);//the difference in absolute value
    if(diff_in_sb != 0){
        sb->s_free_blocks_count = free_block_count;
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", diff_in_sb);
    }
    
    //Check group descipher
    int diff_in_gd = abs(free_block_count - (int) gd->bg_free_blocks_count);//the difference in absolute value
    if(diff_in_gd != 0){
        gd->bg_free_blocks_count = free_block_count;
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", diff_in_gd);
    }

    return diff_in_sb + diff_in_gd;
}

//This function returns the file_type(entry) corresponding to the i_mode
unsigned char imode_to_fileType(unsigned short imode){
    if((imode & EXT2_S_IFLNK) == EXT2_S_IFLNK){
        return EXT2_FT_SYMLINK;//Symbolic Link
    }else if((imode & EXT2_S_IFREG) == EXT2_S_IFREG){
        return EXT2_FT_REG_FILE;//Regular file
    }else if((imode & EXT2_S_IFDIR) == EXT2_S_IFDIR){
        return EXT2_FT_DIR;//Directory
    }else{
        return EXT2_FT_UNKNOWN;//Unknown file type
    }
}

//This function checks the entry's file type matches its imode
//If there is a mismatch, it will output information, change file_type based on its i_mode and return 1.
//Otherwise, return 0.
int match_fileType(struct ext2_dir_entry *entry){
    
    //Make sure the entry is in use
    if(entry->inode == 0){
        //printf("match_fileType(): entry is not in use\n");
        exit(1);
    }

    //Get its i_mode from its inode
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[entry->inode - 1];
    unsigned short imode = inode->i_mode;

    unsigned char correct_fileType = imode_to_fileType(imode);
    unsigned char curr_fileType = entry->file_type;//File type in the entry
    
    if(curr_fileType != correct_fileType){//Mismatch
        entry->file_type = correct_fileType;
        printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", entry->inode);
        return 1;
    }
    return 0;
}

//This function makes sure the given inode is in use in its bitmap
//Input: inode number
//If there is a mismatch, it will:  1. output information, 
//                                  2. change the corresponding bit to 1 in the bitmap, 
//                                  3. return 1;
//Otherwise, return 0.
int match_inode_allocation_in_bitmap(int num){
    
    unsigned char *bitmap = get_inode_bitmap();
    int is_in_use = check_resource_in_use(bitmap, num);
    if(is_in_use == 0){//the given inode is marked as not in use in the bitmap
        set_resource_in_use(num, 1);
        printf("Fixed: inode [%d] not marked as in-use\n", num);
        return 1; 
    }
        return 0;

}

//This function makes sure all the blocks inside the given inode are marked as in use in block bitmap
//Input: inode number
//If there is any mismatch, it will:1. output information, 
//                                  2. change the corresponding bit to 1 in block bitmap, 
//                                  3. return the total number of mismatches;
//Otherwise, return 0.
int match_block_allocation_in_bitmap(int num){
    
    int mismatch_count = 0;

    unsigned char *block_bitmap = get_block_bitmap();
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[num-1];
    
    //12 direct blocks
    int n;
    for (n = 0; n < EXT2_DIRECT_BLOCK_NUM; n++) {
        unsigned int block_num = inode->i_block[n];
        //The block is in use but marked as 0 in the bitmap
        if (block_num != 0 && !check_resource_in_use(block_bitmap, block_num)) {
            set_resource_in_use(block_num, 0);
            mismatch_count++;
        }
    }

    //Check the 13-th block
    unsigned int indirect_block_num = inode->i_block[EXT2_DIRECT_BLOCK_NUM];
    
    if (indirect_block_num != 0) {

        //Check the 13-th block itself
        if (!check_resource_in_use(block_bitmap, indirect_block_num)) {
            set_resource_in_use(indirect_block_num, 0);
            mismatch_count++;
        }

        // Get the indirect block
        unsigned int *indirect_block = (unsigned int *)(disk + indirect_block_num * EXT2_BLOCK_SIZE);
        
        int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);//1024 / 4 = 256 blocks
        
        int j;
        for(j = 0; j < max_indirect_blocks; j++){
            unsigned int direct_block_num = indirect_block[j];//The direct block store in indirect_block
            if ((direct_block_num != 0) && (!check_resource_in_use(block_bitmap, direct_block_num))) {
                set_resource_in_use(direct_block_num, 0);
                mismatch_count++;
            }
        }
    }

    if(mismatch_count > 0){
        printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", mismatch_count, num);
    }

    return mismatch_count;
}

//This function makes sure the given inode's deletion time is 0
//If it is not 0, it will:  1. output information, 
//                          2. zero i_dtime, 
//                          3. return 1;
//Otherwise, return 0.
int zero_i_dtime(unsigned int inode_num) {
    //Get its i_dtime from its inode
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[inode_num - 1];
    unsigned int i_dtime = inode->i_dtime;
    
    if(i_dtime != 0){
        inode->i_dtime = 0;
        printf("Fixed: valid inode marked for deletion: [%d]\n", inode_num);
        return 1;
    }

    return 0;
}



//This function starts form the current enrty,
//recursively checks all the inconsistences in itself and its child files
//Input: the current entry
//Return: the total number of inconsistences
int fix_enrty_inconsis_recursively(struct ext2_dir_entry *entry) {
    
    if(entry->inode == 0){//The entry is not in use
        return 0;
    }
    //Get the inode of this entry
    struct ext2_inode *inode = &(get_inode_table()[entry->inode - 1]);
    int inconsis_count = 0;
    inconsis_count += match_fileType(entry);//Fix file type mismatch b)
    inconsis_count += match_inode_allocation_in_bitmap(entry->inode);//Fix inode allocation mismatch in bitmap c)
    inconsis_count += zero_i_dtime(entry->inode);//Fix inode deletion time d)
    inconsis_count += match_block_allocation_in_bitmap(entry->inode);//Fix block allocation mismatch in bitmap e)

    // If entry is not a directory, we have reached the end.
    if (entry->file_type != EXT2_FT_DIR) {
        return inconsis_count;
    }

    // No need to check "." and ".." 
    if (strncmp(entry->name, ".", strlen(".")) == 0 || strncmp(entry->name, "..", strlen("..")) == 0){
        return inconsis_count;
    }

    //Check its 12 blocks:
    int j;
    for (j = 0 ; j < 12; j++) {
        int i_block = inode->i_block[j];
        if(i_block != 0){//The block is in use
            int curr_len = 0;
            while (curr_len < EXT2_BLOCK_SIZE) {
                struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * i_block + curr_len);
                if(entry->inode == EXT2_ROOT_INO || entry->inode >= 12){//Only check inode2 and inodes after inode11
                    inconsis_count += fix_enrty_inconsis_recursively(entry);
                }
                curr_len += entry->rec_len;
            }
        }
    }
    //Check the 13-th block
    unsigned int indirect_block_num = inode->i_block[12];
    
    if (indirect_block_num != 0) {// The indirect block (12-th) is in use
        // Get the indirect block
        unsigned int *indirect_block = (unsigned int *)(disk + indirect_block_num * EXT2_BLOCK_SIZE);
        
        int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);//1024 / 4 = 256 blocks
        
        int j;
        for(j = 0; j < max_indirect_blocks; j++){
            //The direct block store in indirect_block
            unsigned int direct_block_num = indirect_block[j];
            if(direct_block_num != 0){
                int curr_len = 0;
                while (curr_len < EXT2_BLOCK_SIZE) {//search every entry
                    struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * direct_block_num + curr_len);

                    if(entry->inode == EXT2_ROOT_INO || entry->inode >= 12){//Only check inode2 and inodes after inode11
                        if (strncmp(entry->name, ".", strlen(".")) != 0 && strncmp(entry->name, "..", strlen("..")) != 0){
                            inconsis_count += fix_enrty_inconsis_recursively(entry);
                        }
                    }
                    
                    curr_len += entry->rec_len;
                }
            }
        }
    }


    return inconsis_count;
}

//This function checks root itself and every entry in root directory
//It returns the total number of inconsistences
int fix_root_dir(){
    
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[EXT2_ROOT_INO - 1];//Root inode
    int inconsis_count = 0;

    //Check if the root i_mode is correct b)
    if ((inode->i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR) {
        inode->i_mode = inode->i_mode | EXT2_S_IFDIR;
        inconsis_count += 1;
        //Root inode's i_mode is not marked as EXT2_S_IFDIR
        printf("Fixed: Entry type vs inode mismatch: inode [2]\n");
    }

    inconsis_count += match_inode_allocation_in_bitmap(EXT2_ROOT_INO);//Fix inode allocation mismatch in bitmap c)
    inconsis_count += zero_i_dtime(EXT2_ROOT_INO);//Fix inode deletion time d)
    inconsis_count += match_block_allocation_in_bitmap(EXT2_ROOT_INO);//Fix block allocation mismatch in bitmap e)
    
    //Check its 12 direct blocks
    for (int j = 0 ; j < 12 ; j++){
        int i_block = inode->i_block[j];
        if(i_block != 0){//This blcok is in use
            
            int curr_len = 0;
            while (curr_len < EXT2_BLOCK_SIZE) {//search every entry
                struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * i_block + curr_len);

                if(entry->inode != 0){
                    if (strncmp(entry->name, ".", strlen(".")) != 0 && strncmp(entry->name, "..", strlen("..")) != 0){
                        inconsis_count += fix_enrty_inconsis_recursively(entry);
                    }
                }
                
                curr_len += entry->rec_len;
            }
        }
    }
    //Check the 13-th block
    unsigned int indirect_block_num = inode->i_block[12];
    
    if (indirect_block_num != 0) {// The indirect block (12-th) is in use
        // Get the indirect block
        unsigned int *indirect_block = (unsigned int *)(disk + indirect_block_num * EXT2_BLOCK_SIZE);
        
        int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);//1024 / 4 = 256 blocks
        
        int j;
        for(j = 0; j < max_indirect_blocks; j++){
            //The direct block store in indirect_block
            unsigned int direct_block_num = indirect_block[j];
            if(direct_block_num != 0){
                int curr_len = 0;
                while (curr_len < EXT2_BLOCK_SIZE) {//search every entry
                    struct ext2_dir_entry *entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * direct_block_num + curr_len);
                    if(entry->inode != 0){
                        if (strncmp(entry->name, ".", strlen(".")) != 0 && strncmp(entry->name, "..", strlen("..")) != 0){
                            inconsis_count += fix_enrty_inconsis_recursively(entry);
                        }
                    }
                    curr_len += entry->rec_len;
                }
            }
        }
    }

    return inconsis_count;
}


int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }

    char *image_file_name = argv[1];
    load_image(image_file_name);

    unsigned int inconsis_count = 0;

    //Start from root inode, check inconsistences
    inconsis_count += fix_root_dir();//Check every entry in roots
    inconsis_count += match_free_inodes_count();//Fix a) inodes counter in sb and gd
    inconsis_count += match_free_blocks_count();//Fix a) blocks counter in sb and gd

    if (inconsis_count > 0) {
        printf("%d file system inconsistencies repaired!\n", inconsis_count);
    } else {
        printf("No file system inconsistencies detected!\n");
    }
    return 0;

}

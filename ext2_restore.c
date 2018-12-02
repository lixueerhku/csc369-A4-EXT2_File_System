#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"


 // This function checks if all its data blocks are not in use.
 // It returns TRUE if all of them are free, FALSE otherwise.
 // In the assignment, we will not try to recover files that had hardlinks at the time of removal. 
 // Therefore, all the data blocks and the inode should be free when restoring the file
int inode_and_all_data_blocks_free(unsigned int inode_num){
    
    unsigned char *block_bitmap = get_block_bitmap();
    unsigned char *inode_bitmap = get_inode_bitmap();
    
    //First check this inode
    if(check_resource_in_use(inode_bitmap, inode_num)){
        return FALSE;
    }

    //The inode is not in use, check all its data blocks
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[inode_num - 1];
    int i;

    //Check 12 direct blocks
    for (i = 0; i < EXT2_DIRECT_BLOCK_NUM; i++) {
        int block_num = inode->i_block[i];
        
        if (block_num == 0) {//Reach the end of the block list and all the blocks before are not in use
            return TRUE;
        }else{
            if (check_resource_in_use(block_bitmap, block_num)) {
                return FALSE;
            }
        }
    }

    //Check the 13-th block (indirect block)
    int indirect_block_num = inode->i_block[EXT2_DIRECT_BLOCK_NUM];
    if(indirect_block_num == 0){
        return TRUE;
    }
    unsigned int* indirect_block = (unsigned int*)(disk + EXT2_BLOCK_SIZE * indirect_block_num);
    int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);
    
    //Search inside the indirect block
    int j;
    for(j = 0; j < max_indirect_blocks; j++){
        int direct_block_num = indirect_block[j];
        if (direct_block_num == 0) {//Reach the end of the block list
            return TRUE;
        }else{
            if (check_resource_in_use(block_bitmap, direct_block_num)) {
                return FALSE;
            }
        }
    }

    //Have checked all the data blocks and none of them is in use
    return TRUE;
}

//This function set the given inode and all its data blocks to be in use
//It will first check whether all of them are not in use
//It returns EXIT_SUCCESS if the execution is successful.
//It returns ENOENT if the file cannot be fully restored.
int reset_inode_and_all_data_blocks_in_use(unsigned int inode_num){
    

    //Some of the blocks may be reused, the file can't be fully restored;
    if(inode_and_all_data_blocks_free(inode_num) == FALSE){
        return ENOENT;
    }

    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[inode_num - 1];

    set_resource_in_use(inode_num, 1);//Set the inode in use
    inode->i_links_count = 1;//Update link count
    inode->i_dtime = 0;//Update deletion time

    //Set all its data blocks in use
    int i;
    for (i = 0; i < EXT2_DIRECT_BLOCK_NUM; i++) {//For 12 direct blocks
        int block_num = inode->i_block[i];
        
        if (block_num == 0) {//Reach the end of the block list
            return EXIT_SUCCESS;
        }
        //Set the block in use
        set_resource_in_use(block_num, 0);
    }


    //For the 13-th block (indirect block)
    int indirect_block_num = inode->i_block[EXT2_DIRECT_BLOCK_NUM];
    if(indirect_block_num == 0){//The indirect block is not in use
        return EXIT_SUCCESS;
    }

    //Set the indirect block in use
    set_resource_in_use(indirect_block_num, 0);
    unsigned int* indirect_block = (unsigned int*)(disk + EXT2_BLOCK_SIZE * indirect_block_num);
    int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);
    
    //Loop over the indirect block
    int j;
    for(j = 0; j < max_indirect_blocks; j++){
        int direct_block_num = indirect_block[j];
        if (direct_block_num == 0) {//Reach the end of the block list
            return EXIT_SUCCESS;
        }
        //Set the direct block in use
        set_resource_in_use(direct_block_num, 0);
    }
    return EXIT_SUCCESS;
}

//This function searches the file in one block including entry 'gaps'
//If the entry exists, the function will return its entry postion
//Notice: the function will not unhidden the entry.
//        the function doesn't check whether the file entry is hidden
//It returns NULL if there is no such entry in this block.
struct ext2_dir_entry *enhanced_entry_search(int block_num, char *file_name){
    
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block_num);
    
    int name_length = strlen(file_name);
    if(name_length > EXT2_NAME_LEN){
        exit(EXIT_FAILURE);
    }
    
    //The first entry in the block cannot be restored since we zeroed its inode number when removing it
    //The first entry with inode 0 doesn't necessarily mean the block is unused
    //e.g. the first entry has been removed before but there are still other entries in the block
    if(entry->inode == 0 && entry->rec_len == 0){//The block is unused
        return NULL;
    }

    int len_count = 0;
    //Search the whole block
    while(len_count <= EXT2_BLOCK_SIZE && entry->inode != 0){

        entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * block_num + len_count);

        if(strncmp(file_name, entry->name, name_length) == 0){//Find our hidden entry!
            return entry;
        }

        //Update current entry by its actual entry length so that we can search 'gaps'
        int actual_len = actual_entry_len(entry);
        len_count += actual_len;
    }   
    
    return NULL;

}

//This function finds and returns the previous entry the given hidden_entry in one block
//There may be multiple hidden entries before the given entry. We will ignored them.
//Therefore, the previous entry the function returns must be an unhidden entry.
//Notice: the function will not check if the hidden_entry is a valid entry (Simply regard it as a position in the block)
//It returns NULL if the block is not used or the input is invalid
struct ext2_dir_entry *get_prev_entry(int block_num, struct ext2_dir_entry *hidden_entry){
    
    struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * block_num);
    
    if(entry->inode == 0 && entry->rec_len == 0){//The block is unused
        return NULL;
    }
    if(hidden_entry - entry > EXT2_BLOCK_SIZE){
        //Invalid input. hidden_entry should be inside the block
        return NULL;
    }

    int curr_len = 0;

    while (curr_len < EXT2_BLOCK_SIZE) {
        entry = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * block_num + curr_len);

        //The distance between the given entry and the current entry
        int distance = (unsigned char *)hidden_entry - (unsigned char *)entry;
        if(distance < entry->rec_len){//The given entry is covered by the current entry
            return entry;
        }
        //Update the entry by ren_len since we only checks unhidden entries
        curr_len += entry->rec_len;
    }  
    //Should not reach here
    return NULL;

}

//This function uncovers the hidden entry from the previous entry where it hides
void uncover_entry(struct ext2_dir_entry *prev_entry, struct ext2_dir_entry *hidden_entry){
    
    if(prev_entry == NULL || hidden_entry == NULL){
        exit(EXIT_FAILURE);
    }

    int prev_entry_new_len = (unsigned char *)hidden_entry - (unsigned char *)prev_entry;
    hidden_entry->rec_len = prev_entry->rec_len - prev_entry_new_len;
   
    //Previous entry points to the hidden entry
    prev_entry->rec_len = prev_entry_new_len;

}

//This function restores the file in parent inode
//It returns EXIT_SUCCESS(0) if the execution is sucessful
//It returns EEXIST if the file exists in the parent directory
//It returns ENOENT if the file cannot be restored
//It returns EISDIR if the file is a directory
int restore_file(struct ext2_inode *parent_inode, char*file_name){
    
    //Check whether the file exists in the parent directory
    if(find_entry(parent_inode, file_name) != NULL){
        return EEXIST;
    }

    //Search within 12 direct blocks
    int i ;
    for(i = 0; i < EXT2_DIRECT_BLOCK_NUM; i++){
        int block_num = parent_inode->i_block[i];
        if(block_num != 0){
            //Search within the block
            struct ext2_dir_entry *hidden_entry = enhanced_entry_search(block_num, file_name); 
            if(hidden_entry != NULL){//The hidden entry is in this block!
                
                //Check whether the hidden file is a directory
                if(hidden_entry->file_type == EXT2_FT_DIR){
                    
                    return EISDIR;
                }

                if(reset_inode_and_all_data_blocks_in_use(hidden_entry->inode) != ENOENT){//Reset its inode and all data blocks sucessfully. 
                    //Uncover the hidden entry
                    
                    struct ext2_dir_entry *prev_entry = get_prev_entry(block_num, hidden_entry);
                    if(prev_entry != NULL){
                        
                        
                        int prev_entry_new_len = (unsigned char *)hidden_entry - (unsigned char *)prev_entry;

                        hidden_entry->rec_len = prev_entry->rec_len - prev_entry_new_len;
                        //Previous entry points to the hidden entry
                        prev_entry->rec_len = prev_entry_new_len;
                        
                        return EXIT_SUCCESS;
                    }
                }else{
                    //Reset fails. The file cannot be restored
              
                    return ENOENT;
                }
            }
            
        }
    }

    //For the 13-th block (indirect block)
    int indirect_block_num = parent_inode->i_block[EXT2_DIRECT_BLOCK_NUM];

    //Reaching here only when the file is not hidden in the first 12 blocks
    if(indirect_block_num == 0){//The indirect block is not in use,i.e. we cannot find the hidden file
        return ENOENT;
    }

    unsigned int* indirect_block = (unsigned int*)(disk + EXT2_BLOCK_SIZE * indirect_block_num);
    int max_indirect_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);
    
    //Loop over the indirect block
    int j;
    for(j = 0; j < max_indirect_blocks; j++){
        int direct_block_num = indirect_block[j];
        if (direct_block_num != 0) {
            //Search within the block
            struct ext2_dir_entry *hidden_entry = enhanced_entry_search(direct_block_num, file_name);
            if(hidden_entry != NULL){//The hidden entry is in this block!
                //Check whether the hidden file is a directory
                if(hidden_entry->file_type == EXT2_FT_DIR){
                    return EISDIR;
                }

                if(reset_inode_and_all_data_blocks_in_use(hidden_entry->inode) != ENOENT){//Reset its inode and all data blocks sucessfully. 
                    //Uncover the hidden entry
                    struct ext2_dir_entry *prev_entry = get_prev_entry(direct_block_num, hidden_entry);
                    uncover_entry(prev_entry, hidden_entry);
                    return EXIT_SUCCESS;
                }else{
                    //Reset fails. The file cannot be restored
                    return ENOENT;
                }
            }
        }
    }

    //Reach here only when the file is not found in all data blocks
    return ENOENT;

}

int main(int argc, char *argv[]) {
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <path to file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *image_file_name = argv[1];
    char *path_to_file = argv[2];
    load_image(image_file_name);

    //Get file_name
    //Create a copy of path_to_file so that path_to_file won't be changed
    char path_to_file_copy[strlen(path_to_file) + 1];
    strcpy(path_to_file_copy, path_to_file);
    char *file_name = get_file_name(path_to_file_copy);
    char const_file_name[EXT2_NAME_LEN + 1];//Make the name constont
    int i;
    for(i = 0; i < strlen(file_name); i++){
        const_file_name[i] = file_name[i];
    }
    const_file_name[strlen(file_name)] = '\0';

    //Get parent inode
    //Create a copy of path_to_file so that path_to_file won't be changed
    char path_to_file_copy2[strlen(path_to_file) + 1];
    strcpy(path_to_file_copy2, path_to_file);
    int parent_inode_num = second_last_dir_inode(path_to_file_copy2);
    struct ext2_inode *parent_inode = &get_inode_table()[parent_inode_num - 1];
    
    //Restore file
    return restore_file(parent_inode, const_file_name);

}

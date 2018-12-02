#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"

/*
 * This function returns the previous entry of the input file in the block
 * It returns NULL if it is the first entry in the block
 * It returns ENOENT if the file entry doesn't exist
 */
struct ext2_dir_entry *find_prev_entry(struct ext2_inode *inode, char *file_name) {

	//Check if the inode type is directory
    if(get_inode_type(inode) != 'd'){
        exit(EXIT_FAILURE);
    }

	struct ext2_dir_entry * file_entry = find_entry(inode, file_name);

	//The file doesn't exist
    if(file_entry == NULL){
        exit(ENOENT);	
    }


	int j;
    for (j = 0 ; j < inode->i_blocks / 2 ; j++) {
        int i_block = inode->i_block[j];
        int curr_len = 0;
        struct ext2_dir_entry *entry = NULL;
        struct ext2_dir_entry *prev_entry = NULL;
        
        while (curr_len < EXT2_BLOCK_SIZE) {

            entry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * i_block + curr_len);
            if (strlen(file_name) == ((int) entry->name_len)) {  
                if (strncmp(file_name, entry->name, strlen(file_name)) == 0) {//entry->name matches file_name
                    return prev_entry;
                }
            }
            prev_entry = entry;
            curr_len += entry->rec_len;
        }
    }

	//Can't find a match entry for file_name;
    exit(ENOENT);
}

/*
 * This function delete the file in the given inode
 * It returns ENOENT if the file doesn't exist
 * It returns EISDIR if the file is directory
 */
void delete_file(int parent_inode_num, char* file_name){
   
    struct ext2_inode *parent_dir_inode = &get_inode_table()[parent_inode_num - 1];
	struct ext2_dir_entry * file_entry = find_entry(parent_dir_inode, file_name);

	//The file that needs to be removed doesn't exist
    if(file_entry == NULL){
        exit(ENOENT);	
    }
    //The file that needs to be removed is directory
    if(file_entry->file_type == EXT2_FT_DIR){
        exit(EISDIR);
    }

    struct ext2_dir_entry * prev_entry = find_prev_entry(parent_dir_inode, file_name);
    
    //Remove the entry form its parent inode
    if(prev_entry == NULL){//This is the first entry in the block
    	file_entry->inode = 0;//Set the block to be not in use
    }else{
    	//Link the previous entry to the next entry
    	prev_entry->rec_len += file_entry->rec_len;
    }

   	//Decrease the link count for the file
    unlink_inode(file_entry->inode);


}

int main(int argc, char *argv[]) {
    
    //Check if the number of arguments is correct
	if (argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <path to link>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *image_file_name = argv[1];
    char *path_to_link = argv[2];

    load_image(image_file_name);

    //Create a copy of path_to_link so that path_to_link won't be changed
    char path_to_link_copy[strlen(path_to_link) + 1];
    strcpy(path_to_link_copy, path_to_link);
    char *file_name = get_file_name(path_to_link_copy);
    char const_file_name[EXT2_NAME_LEN + 1];//Make the name constont
    int i;
    for(i = 0; i < strlen(file_name); i++){
        const_file_name[i] = file_name[i];
    }
    const_file_name[strlen(file_name)] = '\0';

    //Get inode of the parent directory
    //Create a copy of path_to_link so that path_to_link won't be changed
    char path_to_link_copy2[strlen(path_to_link) + 1];
    strcpy(path_to_link_copy2, path_to_link);
    int parent_dir_inode_num = second_last_dir_inode(path_to_link_copy2);

    //We can't remove the current directory or the parent direntory
    if(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0){
        fprintf(stderr, "Can't remove current entry or parent entry\n");
        exit(EXIT_FAILURE);
    }

    delete_file(parent_dir_inode_num, const_file_name);

    return 0;

}

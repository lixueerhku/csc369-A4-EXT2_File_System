#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "ext2_utils.h"


int main(int argc, char *argv[]) {

    //Check if the number of arguments is correct
	if (argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *image_file_name = argv[1];
    char *target_path = argv[2];
    load_image(image_file_name);//Initialize disk, sb and gd

    //Get the name of the newly added directory
    //Create a copy of target_path so that target_path won't be changed
    char target_path_copy[strlen(target_path) + 1];
    strcpy(target_path_copy, target_path);
    char *dir_name = get_file_name(target_path_copy);
    char const_dir_name[EXT2_NAME_LEN + 1];//Make the name constont
    int i;
    for(i = 0; i < strlen(dir_name); i++){
        const_dir_name[i] = dir_name[i];
    }
    const_dir_name[strlen(dir_name)] = '\0';

    //Get the inode of the second last directory
    char target_path_copy2[strlen(target_path) + 1];
    strcpy(target_path_copy2, target_path); //Create a copy of target_path so that target_path won't be changed
    struct ext2_inode *inodes = get_inode_table();
    int parent_inode_num = second_last_dir_inode(target_path_copy2);
    struct ext2_inode * parent_inode = &inodes[parent_inode_num - 1];

    //Allocate a new inode for the directory
    unsigned int new_inode_num = allocate_inode(EXT2_FT_DIR);

    //Insert the new directory into the parent inode
    insert_dir_entry(parent_inode, new_inode_num, const_dir_name, EXT2_FT_DIR);

    struct ext2_inode *new_inode = &inodes[new_inode_num - 1];
    
    //Insert '.' and '..' into the new directory
    insert_dir_entry(new_inode, new_inode_num, ".", EXT2_FT_DIR);
    insert_dir_entry(new_inode, parent_inode_num, "..", EXT2_FT_DIR);
    
    //Update directories count
    gd->bg_used_dirs_count += 1;

    return 0;


}
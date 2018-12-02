#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ext2_utils.h"

//This function stores the absolute path (source_path) in a data block (regardless of length) of the file
//Input: file_entry is the symbolic file that stores source_path
void store_symbolic_link(struct ext2_dir_entry *file_entry, char *source_path){

    //Get the inode of the file
    struct ext2_inode *inode_table = get_inode_table();
    struct ext2_inode *inode = &inode_table[file_entry->inode - 1];

    //Allocate a new block and store absolute path to link
    int block_num = allocate_block();
    unsigned char* block = (unsigned char*)(disk + block_num * EXT2_BLOCK_SIZE);

    //Store the absolute path in the block
    memcpy(block, source_path, strlen(source_path));
   
    inode->i_block[0] = block_num; //The 1-st block pointer points to the newly created block
    inode->i_size = strlen(source_path);//The size of the inode is the length if the path
    inode->i_blocks = 2; //There are 2 sectors in the inode
}

/*
 * This program takes three command line arguments. 
 * The first is the name of an ext2 formatted virtual disk. 
 * The second is the file that already exists
 * The third one is the file that is being created
 * The program works like ln, creating a link from the first specified file to the second specified path.
 */

int main(int argc, char *argv[]) {
    
    //Check if the number of arguments is correct
    if(argc < 4 || argc > 5){//Invalid argument number
        fprintf(stderr, "Usage: %s  <image file name> <source path> <dest path> or\
         <image file name> -s <source path> <dest path>", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    char *image_file_name = argv[1];
    char *source_path;//The file that already exists
    char *dest_path;//The file that is being created
    unsigned char file_type;



    if (argc == 4) {//Greate a hard link
        source_path = argv[2];
        dest_path = argv[3];
        file_type = EXT2_FT_REG_FILE;
    }else if(argc == 5){//Create a symlink link
        char *flag = argv[2];
        if(strcmp(flag, "-s") != 0){//flag is not '-s'
            fprintf(stderr, "Error: flag %s is not '-s'", flag);
            exit(EXIT_FAILURE);
        }
        source_path = argv[3];
        dest_path = argv[4];
        file_type = EXT2_FT_SYMLINK;
    }

    load_image(image_file_name);

    //Get file name
    //Create a copy of path_to_source so that path_to_source won't be changed
    char path_to_source_copy[strlen(source_path) + 1];
    strcpy(path_to_source_copy, source_path);
    char * source_file_name = get_file_name(path_to_source_copy);
    char const_file_name[EXT2_NAME_LEN + 1];//Make the name constont
    int i;
    for(i = 0; i < strlen(source_file_name); i++){
        const_file_name[i] = source_file_name[i];
    }
    const_file_name[strlen(source_file_name)] = '\0';

    //Get the entry of the source file
    //Create a copy of path_to_source so that path_to_source won't be changed
    char path_to_source_copy2[strlen(source_path) + 1];
    strcpy(path_to_source_copy2, source_path);
    int parent_inode_num = second_last_dir_inode(path_to_source_copy2);
    struct ext2_inode *parent_inode = &get_inode_table()[parent_inode_num - 1];
    
    struct ext2_dir_entry * source_entry = find_entry(parent_inode, const_file_name);

    //Return ENOENT if the source file doesn't exist
    if(source_entry == NULL){
        //printf("Error: source file %s doesn't exist", source_path);
        return ENOENT;
    }

    //Return EISDIR if a hardlink refers to a directory
    if(source_entry->file_type == EXT2_FT_DIR && file_type == EXT2_FT_REG_FILE){
        return EISDIR;
    }

    if(file_type == EXT2_FT_REG_FILE){
        //Greate a hard link
        //The newly created file shares the same inode number with the source file
        create_file(dest_path, source_entry->inode, EXT2_FT_REG_FILE);
    }else{
        //Greate a symbolic link
        //The newly created file needs a new inode
        struct ext2_dir_entry* new_entry = create_file(dest_path, 0, EXT2_FT_SYMLINK);

        //Store the absolute path in the newly created file
        store_symbolic_link(new_entry, source_path);
    }



}

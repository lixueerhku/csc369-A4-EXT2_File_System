#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ext2_utils.h"


//This function copies the data from the source file src to the inode
void cope_data_from_file(struct ext2_inode *inode, FILE *stream){
	unsigned int bytes_num;//The total number of bytes that is read by fread()
    unsigned char buffer[EXT2_BLOCK_SIZE];
    int block_idx = 0;

    //Read date with the size of one block each time and copy it to one block
    //Copy data into direct blocks (EXT2_DIRECT_BLOCK_NUM == 12)
    while(block_idx < EXT2_DIRECT_BLOCK_NUM){
    	
    	bytes_num = fread(buffer, 1, EXT2_BLOCK_SIZE, stream);
    	
    	if(bytes_num <= 0){//All data in stream has been read
    		return;
    	}else{
	    	//Allocate a new block
	        unsigned int block_num = allocate_block();
	        unsigned char *new_block = disk + (block_num * EXT2_BLOCK_SIZE);
	        
	        //Update inode information
	        inode->i_block[block_idx] = block_num;
	        inode->i_blocks += EXT2_BLOCK_SIZE / EXT2_SECTOR_SIZE;//Increase two sectors(512 byte * 2 = one block)
	        inode->i_size += bytes_num;

	        //Copy data into the new block
	        memcpy(new_block, buffer, bytes_num);
	        block_idx++;
    	}
    }

    // After 12 blocks are full, if there is data unread in stream, 
    // continue the work in the indirect blocks
    if ((bytes_num = fread(buffer, 1, EXT2_BLOCK_SIZE, stream)) <= 0) {
        return;
    }

    //Copy data to indirect blocks
    //With 1 indirect block, we can address up to 1024 / 4 = 256 blocks
    //Since our file system has only 128 blocks, we will not use double indirect block
    unsigned int indirect_block_num = allocate_block();//The indirect block
    inode->i_block[EXT2_DIRECT_BLOCK_NUM] = indirect_block_num;//The 13-th block stores the indirect block
    inode->i_blocks += EXT2_BLOCK_SIZE / EXT2_SECTOR_SIZE;

    int max_indir_blocks = EXT2_BLOCK_SIZE / sizeof(unsigned int);//1024 / 4 = 256 blocks
    int indir_blocks_count = 0;
    unsigned int *indirect_block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * indirect_block_num);

    while(indir_blocks_count <= max_indir_blocks){
    	if(bytes_num <= 0){
    		return;
    	}else{
    		// Allocate direct block to store data from stream
        	unsigned int block_num = allocate_block();
        	unsigned char *new_block = disk + (block_num * EXT2_BLOCK_SIZE);

        	//Update inode information
	        *indirect_block = block_num;//Store block number in the indirect block
	        indirect_block += 1;//Pointer moves to next int in the indirect block
	        inode->i_blocks += EXT2_BLOCK_SIZE / EXT2_SECTOR_SIZE;//Increase two sectors
	        inode->i_size += bytes_num;

	        //Copy data into the new block
	        memcpy(new_block, buffer, bytes_num);
	        
	        indir_blocks_count++;
    	}

    	bytes_num = fread(buffer, 1, EXT2_BLOCK_SIZE, stream);

    }

    //The function should not reach this point
    //("Error: cope_data_from_file() fails\n");
    return;

}

int main(int argc, char *argv[]) {

	//Check if the number of arguments is correct
	if (argc != 4) {
        fprintf(stderr, "Usage: %s <image file name> <path to source file> <path to dest>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *image_file_name = argv[1];
    char *path_to_source = argv[2];
    char *path_to_dest = argv[3];


    //‘rb’: read in binary mode 
    FILE *source_file = fopen(path_to_source, "rb");
    if (source_file == NULL) {
        exit(ENOENT);
    }

    load_image(image_file_name);

    //Create a new file and get its entry
    struct ext2_dir_entry *new_entry = create_file(path_to_dest, 0, EXT2_FT_REG_FILE);

    //Get the inode for the newly created file
    struct ext2_inode *inodes = get_inode_table();
	struct ext2_inode * new_inode = &inodes[new_entry->inode - 1];
	
	//Copy data from soure file to the new inode
	cope_data_from_file(new_inode, source_file);

	//Close the source file
    fclose(source_file);
    
    return 0;


}
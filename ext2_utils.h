#ifndef CSC369_EXT2_UTILS_H
#define CSC369_EXT2_UTILS_H

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ext2.h"

#define EXIT_FAILURE 1 
#define EXIT_SUCCESS 0
#define TRUE 1
#define FALSE 0
#define EXT2_SECTOR_SIZE 512
#define EXT2_DIRECT_BLOCK_NUM 12

extern unsigned char *disk;
extern struct ext2_super_block *sb;
extern struct ext2_group_desc *gd;

void load_image(const char *file);
unsigned char *get_block_bitmap();
unsigned char *get_inode_bitmap();
struct ext2_inode *get_inode_table();
char get_inode_type(struct ext2_inode *inode);
int inode_num_of(struct ext2_inode *inode);

//static int is_absolute_path(const char *path);

char *get_file_name(char *path_to_file);

int check_resource_in_use(unsigned char *bitmap, int num);

int allocate_resource(unsigned char *bitmap, int num_bytes);

int allocate_inode(unsigned char file_type);

int allocate_block();

int second_last_dir_inode(char *path);

void init_dir_entry(struct ext2_dir_entry *entry, unsigned int inode_num, 
					unsigned short rec_len, int name_len, char *name, 
					unsigned char file_type);

struct ext2_dir_entry* create_file(char *path_to_dest, unsigned int finode, char file_type);

struct ext2_dir_entry *find_entry(struct ext2_inode *inode, char *file_name);

struct ext2_dir_entry *find_last_entry(int i_block);

int actual_entry_len(struct ext2_dir_entry *entry);

struct ext2_dir_entry *insert_dir_entry(struct ext2_inode *dir_inode, unsigned int finode, 
										char *fname, unsigned char ftype);

void set_resource_in_use(unsigned int resource_num, int is_inode);

void free_inode(unsigned int inode_num);

void free_block(unsigned int block_num);

void free_data_blocks(struct ext2_inode *inode);

void unlink_inode(unsigned int inode_num);


#endif







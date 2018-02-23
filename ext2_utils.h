#ifndef EXT2_UTILS_H
#define EXT2_UTILS_H

#include "ext2.h"
#include <sys/time.h>

#define SINGLE_INDIR_BLOCK_INDEX 12
#define DISK_SECTOR_SIZE 512
#define DIR_STRUCT_SIZE (unsigned int)sizeof(struct ext2_dir_entry)

unsigned char *image_setup(char * command_line_arg);

int check_path(char *path);

unsigned int traverse_path(struct ext2_inode *inode_table, char *path);

unsigned int find_next_inode(struct ext2_inode *inode_table, char *next, unsigned int current_inode);

unsigned int print_dir_entry(struct ext2_dir_entry *dir_entry);

void reset_bit(int index, unsigned int *bitmap);

void set_bit(int index, unsigned int *bitmap);

void add_direntry(struct ext2_dir_entry *dir_entry, struct ext2_inode *inode, char * name);

unsigned int find_first_free_block();

int find_first_free_inode_num();

unsigned short nearest_multiple_four(unsigned char size);
#endif
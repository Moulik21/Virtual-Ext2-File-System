#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "ext2.h"
#include "ext2_utils.h"
#include <libgen.h>

unsigned char *disk;

int main(int argc, char **argv){

	// check for correct number of arugments
	if (argc != 3){
		fprintf(stderr, "Usage: ext2_mkdir <image file name> <path/folder_to_create>\n");
		return -EINVAL;
	}

	int arg_path_length = strlen(argv[2]);
	char arg_path_name[arg_path_length + 1];
	strcpy(arg_path_name, argv[2]);
	arg_path_name[arg_path_length] = '\0';
	
	//check if path is valid in terms of format
	if (check_path(arg_path_name) !=0 || strlen(arg_path_name) == 1){
		fprintf(stderr,"Invalid path\n");
		return -EINVAL;
	}

	//remove the trailing '/', if there is one 
	if(arg_path_length > 1){		
		if (arg_path_name[arg_path_length-1] == '/')
		{						
			arg_path_name[arg_path_length-1] = '\0';
		}
	}
	
	//setup disk image
	disk = image_setup(argv[1]);

	//extract the path until the parent
	// eg. /../home/afolder -> parent path = /../home
	char *temp = malloc(arg_path_length + 1);
	strcpy(temp, arg_path_name);
	temp[arg_path_length] = '\0';	
	char parent_path_name[arg_path_length];
	strcpy(parent_path_name,dirname(temp));
	free(temp);
	
	

	struct ext2_super_block *super_block = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *block_group_desc = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (block_group_desc->bg_inode_table * EXT2_BLOCK_SIZE));

	//check if the parent path actually exists or not


	//use this inode_table and find the inode number through recursive function
	//if the inode number does not exist, return an error
	unsigned int parent_inode_number = traverse_path(inode_table, parent_path_name);

	if(parent_inode_number == 0){
		fprintf(stderr,"No such file or directory\n");
		return -ENOENT;
	}
	if((inode_table[parent_inode_number - 1].i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR){
		fprintf(stderr,"Parent path not a directory\n");
		return -ENOTDIR;
	}

	//we know the parent path is valid and exists, so now we want to check 
	//if the folder/file with the same name exists already at the location
	//if it does, return an error
	
	unsigned int dest_inode = traverse_path(inode_table, arg_path_name);

	if(dest_inode != 0){
		fprintf(stderr, "File exists\n");
		return -EEXIST;
	}

	//check for free inodes or blocks, in both super block and block group
	//if none exist, there is no space	
	if (super_block->s_free_inodes_count == 0 || super_block->s_free_blocks_count == 0 || 
		block_group_desc->bg_free_blocks_count == 0|| block_group_desc->bg_free_inodes_count == 0){
		fprintf(stderr," No space left on device \n");
		return -ENOSPC;
	}	
	
	int first_free_inode_num = find_first_free_inode_num();
	//using the inode bitmap as an added layer to checking for space
	if(first_free_inode_num == 0){
		fprintf(stderr," No space left on device \n");
		return -ENOSPC;
	}
	dest_inode = first_free_inode_num;
	block_group_desc->bg_used_dirs_count++;
	
	//pointers to the inodes in the inode table
	char * inode_start = (char *)(disk + EXT2_BLOCK_SIZE * block_group_desc->bg_inode_table);
	struct ext2_inode *new_inode = (struct ext2_inode *) (inode_start + (sizeof(struct ext2_inode)) * (dest_inode - 1));
	struct ext2_inode *parent_inode = (struct ext2_inode *) (inode_start + (sizeof(struct ext2_inode)) * (parent_inode_number - 1));
	
	//set the initial settings of the inode itself
	//set all the data blocks to 0	
	struct timeval current_time;

	new_inode->i_mode = EXT2_S_IFDIR;	
	new_inode->i_uid = 0;
	new_inode->i_size = EXT2_BLOCK_SIZE;

	gettimeofday(&current_time, NULL);	
	new_inode->i_ctime = current_time.tv_sec;

	new_inode->i_gid = 0;
	new_inode->i_links_count = 1;
	new_inode->i_blocks = EXT2_BLOCK_SIZE / DISK_SECTOR_SIZE;	
	new_inode->osd1 = 0;

	int j;
	for (j = 0; j < 15; j++){
		new_inode->i_block[j] = 0;
	}
	new_inode->i_generation = 0;
	new_inode->i_file_acl = 0;
	new_inode->i_dir_acl = 0;
	new_inode->i_faddr = 0;
	for(j = 0; j < 3; j++){
		new_inode->extra[j] = 0;
	}

	//when we create a folder, we need to create a .. and . dir entries as well
	struct ext2_dir_entry pointer_to_same; // .
	struct ext2_dir_entry pointer_to_parent; // ..

	//the inode we created is a directory entry, so we need to setup its struct
	struct ext2_dir_entry new_dirent;
	
	new_dirent.inode = dest_inode;
	new_dirent.file_type = EXT2_FT_DIR;

	char *path_name = malloc(arg_path_length + 1);
	strcpy(path_name,arg_path_name);
	path_name[arg_path_length] = '\0';
	char dir_name[arg_path_length];
	strcpy(dir_name,basename(path_name));
	free(path_name);
	
	new_dirent.name_len = strlen(dir_name);
	

	//add new dir to it's parent
	add_direntry(&new_dirent, parent_inode,dir_name);	
	

	pointer_to_same.inode = dest_inode;
	pointer_to_same.name_len = 1;	
	pointer_to_same.file_type = EXT2_FT_DIR;	
	add_direntry(&pointer_to_same, new_inode,".");
	new_inode->i_links_count++;

	pointer_to_parent.inode = parent_inode_number;
	pointer_to_parent.name_len = 2;	
	pointer_to_parent.file_type = EXT2_FT_DIR;	
	add_direntry(&pointer_to_parent, new_inode,"..");	
	parent_inode->i_links_count++;
	
	return 0;
}

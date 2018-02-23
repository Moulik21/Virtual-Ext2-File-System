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


int main(int argc, char **argv) {

	// if you don't have the arguments for image file/path then erro
	if (argc != 4){
		//error -- EINVAL
		fprintf(stderr, "Usage: ext2_cp <image file name> <absolute path> <absolute path> \n");
        exit(EINVAL);
	}

	int first_arg_path_length = strlen(argv[2]);
	char first_arg_path_name[first_arg_path_length + 1];
	strcpy(first_arg_path_name, argv[2]);
	first_arg_path_name[first_arg_path_length] = '\0';
	

	if (check_path(first_arg_path_name) !=0 || strlen(first_arg_path_name) == 1){
		fprintf(stderr,"First Path is an Invalid path\n");
		return -EINVAL;
	}

	if(first_arg_path_length > 1){
		if (first_arg_path_name[first_arg_path_length-1] == '/')
		{			
			first_arg_path_name[first_arg_path_length-1] = '\0';
		}
	}

	//mount the image itself
	disk = image_setup(argv[1]);

	struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
	struct ext2_group_desc *ab = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (ab->bg_inode_table * EXT2_BLOCK_SIZE));
	
	//the first path should return a valid inode, as it needs to exist
	//if the inode number does not exist, return an error

	unsigned int first_path_inode_num = traverse_path(inode_table, disk, first_arg_path_name);
	if(first_path_inode_num == 0){
		fprintf(stderr, "First Path -- No such file or directory\n");
      	exit(ENOENT);
	}

	//first path passed in must be a file, and not a directory
	if((inode_table[first_path_inode_num - 1].i_mode & EXT2_S_IFREG) != EXT2_S_IFREG){
		fprintf(stderr,"First path not a file\n");
		return -ENOTDIR;
	}	
	//do similar checks and sets on the second path passed in

	int second_arg_path_length = strlen(argv[3]);
	char second_arg_path_name[second_arg_path_length + 1];
	strcpy(second_arg_path_name, argv[3]);
	second_arg_path_name[second_arg_path_length] = '\0';
	

	if (check_path(second_arg_path_name) !=0 || strlen(second_arg_path_name) == 1){
		fprintf(stderr,"Second Path is an Invalid path\n");
		return -EINVAL;
	}

	if(second_arg_path_length > 1){
		if (second_arg_path_name[second_arg_path_length-1] == '/')
		{			
			second_arg_path_name[second_arg_path_length-1] = '\0';
		}
	}
	

	//isolate the parent path for the second path
	//we need to check if it exists, and is a directory, b/c we are going to
	//be creating a file there
	char *temp = malloc(second_arg_path_length + 1);
	strcpy(temp, second_arg_path_name);
	temp[second_arg_path_length] = '\0';
	char second_parent_path_name[second_arg_path_length];
	strcpy(second_parent_path_name,dirname(temp));
	free(temp);


	//for our second path, find the node of the parent
	//if the parent doesn't exist, we won't be able to copy stuff to its child
	//path... also if the parent of the destination isn't a directory (it's a file)
	//then we can't copy stuff either, so return errors in both cases
	unsigned int second_parent_inode_number = traverse_path(inode_table, disk, second_parent_path_name);
	if(second_parent_inode_number == 0){
		fprintf(stderr,"Second Path - No such file or directory\n");
		return -ENOENT;
	}
	if((inode_table[second_parent_inode_number - 1].i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR){
		fprintf(stderr,"Second Path - Parent path not a directory\n");
		return -ENOTDIR;
	}

	//we know the parent path is valid and exists, so now we want to check 
	//if the file exists already at the location
	//if it does, return an error
	
	unsigned int second_path_inode_num = traverse_path(inode_table, disk, second_arg_path_name);
	if(second_path_inode_num != 0){
		fprintf(stderr, "File exists\n");
		return -EEXIST;
	}

	//check for free inodes
	//use the struct superblock count to check for this
	if (sb->s_inodes_count == 0 || sb->s_blocks_count == 0){
		fprintf(stderr," No space left on device \n");
		return -ENOSPC;
	}

	//make a copy of the second path becasue want only the basename (the file name portion) of it
	char *path_name = malloc(second_arg_path_length + 1);
	strcpy(path_name,second_arg_path_name);
	path_name[second_arg_path_length] = '\0';
	
	char dir_name[second_arg_path_length];
	strcpy(dir_name,basename(second_arg_path_name));
	//we don't need path_name anymore -- it was just a temp variable
	free(path_name);
	
	unsigned int *block_bitmap = (unsigned int *)(disk + (ab->bg_block_bitmap * EXT2_BLOCK_SIZE));

	//findi the location of the first free inode, we will use this later
	int first_free_inode_num = find_first_free_inode_num();
	struct ext2_dir_entry dirent_new_node;

	//each pointer is 4B, 1024 for ext2_block_size, so 1024/4B will get us number
	//of pointers we need to iterate over
	unsigned int number_of_indirect_pointers = (EXT2_BLOCK_SIZE / sizeof (unsigned int));
	
	//given that we already have the inode number for both the parent and the child, we want
	// to find the disk block location itself. 
	//start is the start of the inode table, and new_indoe is the position (number) of
	//the inode itself
	char * inode_start = (char *)(disk + EXT2_BLOCK_SIZE * ab->bg_inode_table);
	struct ext2_inode *new_inode = (struct ext2_inode *) (inode_start 
		+ (sizeof(struct ext2_inode)) * (first_free_inode_num));
	
	//first path inode is the position (block #) of the first argument (source) path itself
	struct ext2_inode *first_path_inode = (struct ext2_inode *) (inode_start 
		+ (sizeof(struct ext2_inode)) * (first_path_inode_num - 1));

	//insantiate all of the new_inode struct fields here
	*new_inode = *first_path_inode;
	new_inode->i_links_count = 1;
	new_inode->i_mode = EXT2_S_IFREG;

	struct timeval current_time;
	gettimeofday(&current_time, NULL);	
	new_inode->i_ctime = current_time.tv_sec;
	new_inode->i_uid = 0;
	new_inode->i_gid = 0;
	new_inode->osd1 = 0;
	new_inode->i_generation = 0;
	new_inode->i_file_acl = 0;
	new_inode->i_dir_acl = 0;
	new_inode->i_faddr = 0;

	int j = 0;

	for(j = 0; j < 3; j++){
		new_inode->extra[j] = 0;
	}

	//initially set all of the blocks in new inode to empty
	for (j = 0; j < 15; j++){
		new_inode->i_block[j] = 0;
	}

	//for the singly directed blocks in the first  path inode, you want to 
	//copy any of the populated blocks to the new inode

	for (j = 0; j < 12; j++) {
		// block is used in source then copy
		if (first_path_inode->i_block[j]) {
			//find the first free inode, and set the bit in memory to be 1
			//to show its not free anymore
			new_inode->i_block[j] = find_first_free_block();
			set_bit(first_path_inode->i_block[j] - 1, block_bitmap);

			//copy over that old block to our new block			
			memcpy(disk + new_inode->i_block[j] * EXT2_BLOCK_SIZE, disk + 
				first_path_inode->i_block[j] * EXT2_BLOCK_SIZE, EXT2_BLOCK_SIZE);
		} else {
			new_inode->i_block[j] = 0;
		}
	}


	//if the indirect blocks are actually populated, then execute this loop
	if (first_path_inode->i_block[12]) {
		
		//find the first free block, and instantiate the block which
		//houses the indirect inodes in the new_inode
		new_inode->i_block[12] = find_first_free_block();

		//these pointers are the starting point for our indirect
		//pointers for first path inode and 2nd path inode
		//we can then access the indivitual indirect pointers through a 
		//index
		unsigned int *pointer_to_second_block = (unsigned int *) (disk 
			+ (new_inode->i_block[12]) * EXT2_BLOCK_SIZE);
		unsigned int *pointer_to_first_block = (unsigned int *) (disk 
			+ (first_path_inode->i_block[12]) * EXT2_BLOCK_SIZE);
		
		//decrement the blocks count, and set that block in bitmap to used status
		sb->s_free_blocks_count--;
		ab->bg_free_blocks_count--;
		set_bit(pointer_to_second_block[12] - 1, block_bitmap);		
		
		//iterate over number of indirect pointers
		for(j = 0; j < number_of_indirect_pointers; j++) {
			if (!(pointer_to_first_block[j])) {
				pointer_to_second_block[j] = 0;
			} 
			else {
			//if the indirect pointer is populated, and there are still free blocks to use
			//then copy each block from the source to the destination				
				if (ab->bg_free_blocks_count == 0)
				{	
					   fprintf(stderr," No space left on device \n");
  					  return -ENOSPC;
  				}

				pointer_to_second_block[j] = find_first_free_block();
				sb->s_free_blocks_count--;
				ab->bg_free_blocks_count--;
				set_bit(pointer_to_second_block[j] - 1, block_bitmap);
				memcpy(disk + pointer_to_second_block[j] * EXT2_BLOCK_SIZE, disk 
					+ pointer_to_first_block[j] * EXT2_BLOCK_SIZE, EXT2_BLOCK_SIZE);
			}
		}
	}

	//now that our new inode is setup, we need to instantiate the directory entry itself
	struct ext2_inode *dest_parent_inode = (struct ext2_inode *) (inode_start 
		+ (sizeof(struct ext2_inode)) * (second_parent_inode_number - 1));	
	
	//set the file type, and create the dirent using the helper function
	dirent_new_node.file_type = EXT2_FT_REG_FILE;
    dirent_new_node.inode = first_free_inode_num + 1;
    dirent_new_node.name_len = strlen(dir_name); 
	add_direntry(&dirent_new_node, dest_parent_inode, dir_name);
 	

	return 0;
}
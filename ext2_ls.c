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

unsigned char *disk;


int main(int argc, char **argv) {

	// check for correct number of arugments
	if (argc != 3){		
		fprintf(stderr, "Usage: ext2_ls <image file name> <absolute path> \n");
        return -EINVAL;
	}

	int path_length = strlen(argv[2]);
	char *path_name = malloc(sizeof(char)*(path_length + 1));
	strcpy(path_name, argv[2]);
	path_name[path_length] = '\0';
	
	//check if path is valid in terms of format
	if (check_path(path_name) !=0){
		fprintf(stderr,"Invalid path\n");
		return -EINVAL;
	}

	//remove the trailing '/', if there is one 
	if(path_length > 1 && path_name[path_length-1] == '/'){		
		path_name[path_length-1] = '\0';		
	}
	
	//setup disk image
	disk = image_setup(argv[1]);

	
	struct ext2_group_desc *block_group_desc = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
	struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (block_group_desc->bg_inode_table * EXT2_BLOCK_SIZE));
	
	
	//if the inode number does not exist, return an error
	unsigned int dest_inode = traverse_path(inode_table, path_name);
	if(dest_inode == 0){
		fprintf(stderr, "No such file or directory\n");
      	return -ENOENT;
	}
	//inode is of a file
	if((inode_table[dest_inode - 1].i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR){
		fprintf(stderr, "Not a directory\n");
      	return -ENOTDIR;
	}
	
	unsigned int size = inode_table[dest_inode - 1].i_size;
	int dir_block_num;
	unsigned int buf_size = 0;

	
	int j;
	for(j = 0 ; j < SINGLE_INDIR_BLOCK_INDEX ; j++){
		// if nothing more to read from the directory
		if(size == 0){
	      break;
	    }    
		dir_block_num = inode_table[dest_inode - 1].i_block[j];
		struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num));
		if(size > EXT2_BLOCK_SIZE){
			
			while(buf_size < EXT2_BLOCK_SIZE){				
				buf_size += print_dir_entry(dir_entry);
				dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num) + buf_size);

			}
			size -= (unsigned int) EXT2_BLOCK_SIZE;

		}
		else{
			
			while(buf_size < size){				
				buf_size += print_dir_entry(dir_entry);
				dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num) + buf_size);
			}
			size -= buf_size;
		}
	}

	free(path_name);
	return 0;
}
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
    fprintf(stderr, "Usage: ext2_rm <image file name> <absolute path> \n");
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
  
  struct ext2_super_block *super_block = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
  struct ext2_group_desc *block_group_desc = (struct ext2_group_desc *)(disk + 2 * EXT2_BLOCK_SIZE);
  unsigned int *block_bitmap = (unsigned int *)(disk + (block_group_desc->bg_block_bitmap * EXT2_BLOCK_SIZE));
  unsigned int *inode_bitmap = (unsigned int *)(disk + (block_group_desc->bg_inode_bitmap * EXT2_BLOCK_SIZE));
  struct ext2_inode *inode_table = (struct ext2_inode *)(disk + (block_group_desc->bg_inode_table * EXT2_BLOCK_SIZE));
  

  
  //if the inode number does not exist, return an error
  unsigned int dest_inode = traverse_path(inode_table, path_name);
  if(dest_inode == 0){
    fprintf(stderr, "No such file or directory\n");
    return -ENOENT;
  }

  //inode table
  char * inode_start = (char *)(disk + EXT2_BLOCK_SIZE * block_group_desc->bg_inode_table);

  //pointer to dest inode in inode table
  struct ext2_inode *dest_inode_pt = (struct ext2_inode *) (inode_start + (sizeof(struct ext2_inode)) * (dest_inode - 1));
  
  
  if((dest_inode_pt->i_mode & EXT2_S_IFREG) != EXT2_S_IFREG){
    fprintf(stderr, "Not a file\n");
    return -EISDIR;
  }
  
  //if dest file is valid, then parent dir must exist
  //no need to error check for the parent path and parent inode num
  char *parent_path_name = malloc(path_length + 1);
  strcpy(parent_path_name, path_name);
  parent_path_name[strrchr(parent_path_name, '/') - parent_path_name] = '\0';

  unsigned int parent_inode_number = traverse_path(inode_table,parent_path_name);

  free(path_name);
  free(parent_path_name);  
  

  unsigned int curr_dir_block;
  struct ext2_dir_entry *prev_dirent;
  struct ext2_dir_entry *curr_dirent;
  
  //pointer to parent inode in inode table
  struct ext2_inode *parent_inode = (struct ext2_inode *) (inode_start + (sizeof(struct ext2_inode)) * (parent_inode_number - 1));

  int file_data_block_index;
  int blocks_freed = 0;

  struct timeval current_time;

  //go through each directory entry, to find the file needed to be removed
  int block_index,block_offset;

  for(block_index = 0; block_index < SINGLE_INDIR_BLOCK_INDEX; block_index++){
    
    //skip empty block in parent directory
    if(parent_inode->i_block[block_index] == 0){      
      continue;
    }
    
    curr_dir_block = parent_inode->i_block[block_index];
    curr_dirent = (struct ext2_dir_entry *)(disk + (curr_dir_block * EXT2_BLOCK_SIZE ));
    block_offset = 0;
    while(block_offset < EXT2_BLOCK_SIZE){

      //found the dir entry
      if(curr_dirent->inode == dest_inode){

        // if there are no more links of the file, delete
        if(dest_inode_pt->i_links_count == 1){
          reset_bit(dest_inode-1, inode_bitmap);
          
          for(file_data_block_index = 0; file_data_block_index < SINGLE_INDIR_BLOCK_INDEX; file_data_block_index++){
            if(dest_inode_pt->i_block[file_data_block_index] != 0){
              dest_inode_pt->i_block[file_data_block_index] = 0;
              reset_bit(dest_inode_pt->i_block[file_data_block_index]-1, block_bitmap);
              blocks_freed++;
            }
          }

          dest_inode_pt->i_size = 0;
          dest_inode_pt->i_blocks = 0;
          super_block->s_free_blocks_count += blocks_freed;
          super_block->s_free_inodes_count++;
          block_group_desc->bg_free_inodes_count++;
          block_group_desc->bg_free_blocks_count += blocks_freed;
          gettimeofday(&current_time, NULL);
          dest_inode_pt->i_dtime = current_time.tv_sec;
        }

        //decrese the link count of the file
        dest_inode_pt->i_links_count--;

        //if at the beginning of the block, create a blank record
        if(block_offset == 0){
          curr_dirent->inode = 0;
          curr_dirent->file_type = EXT2_FT_UNKNOWN;
        }

        //else just update the rec_len of previous dir entry to
        // point to the next and create a "gap"
        else{
          prev_dirent->rec_len += curr_dirent->rec_len;
        }
        
        return 0; 
      }

      //update the block offset to go on to next dir entry, if there is one
      block_offset += curr_dirent->rec_len;
      prev_dirent = curr_dirent;
      curr_dirent = (struct ext2_dir_entry *)(disk + (EXT2_BLOCK_SIZE * curr_dir_block) + block_offset);
    }
  }

  return 0;
}

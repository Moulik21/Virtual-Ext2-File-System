#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "ext2_utils.h"
#include "limits.h"

/*useful error code:
  - EPERM - Operation not permitted
  - ENOENT - No such file or directory
  - EACCES - Permission denied
  - ENOTDIR - Not a directory
  - EISDIR - Is a directory
  - EEXIST - File exists
  - ENOTEMPTY - Directory not empty
  - EFAULT - Bad address
  -
*/

//error checking fpr path
/*possible design choice:
  - return an error string instead of number
  - return a struct, with error_code 1/0 and error_string ""/ "error"
*/

unsigned char *disk;
struct ext2_super_block *super_block;
struct ext2_group_desc *block_group_desc;

/*
Set up the disk, super block and block group decriptor
*/
unsigned char *image_setup(char *img){ 
  
  //reading disk image, and returning error if not opening -- from ex8
  int fd = open(img, O_RDWR);
  disk = mmap(NULL, 128 * EXT2_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }
  super_block = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
  block_group_desc = (struct ext2_group_desc *) (disk + 2 * EXT2_BLOCK_SIZE);
  return disk;
}

/*
Returns 0 if the path is of valid format, else returns -EINVAL
*/
int check_path(char *path){
  /* Check that length of path is greater than 1*/

  if(strlen(path) < 1){
    return -EINVAL;
  }

  /* Check that path starts with '/' */
  if(path[0] != '/'){
    return -EINVAL;
  }

  /* Check for consecutive '//' in path */
  if(strstr(path, "//") != NULL){
    return -EINVAL;
  }

  /*  Check for '.' in path  */
  if(path[0] == '.'){
    return -EINVAL;
  }

  //only gets to this point in execution if there are no errors
  return 0;
}

/*
Return inode of the last level in path, else return 0

eg. /home/lru.c - return the inode of lru.c if lru.c exits, else return 0

pre-condition: path is a valid formatted absoulte path
               path does not have a trailing '/'
*/

unsigned int traverse_path(struct ext2_inode *inode_table, char *path){
  // make a copy of path, so we dont change the original path string
  char *path_cpy = malloc(sizeof(char)*(strlen(path)+ 1));
  if(!path_cpy){
    perror("malloc");
    exit(1);
  }
  memcpy(path_cpy,path,strlen(path));
  path_cpy[strlen(path)] = '\0';

  //removes the '/' at the beginning of the path if not just '/'
  if(strlen(path_cpy) > 1 && path_cpy[0] == '/'){
    path_cpy += sizeof(char);
    
  } 

  int diff;
  int null_space;
  int last_index;
  
  unsigned int current_inode = (unsigned int)EXT2_ROOT_INO;//satrting from root
  
  int len;
  // loop until end of path string
  while(path_cpy[0] != '\0'){   
    
    //get next name in path
    if(strchr(path_cpy, '/') != NULL){      
      diff = strchr(path_cpy,'/') - path_cpy;
      //if path only contains '/'
      if(diff == 0){
        
        return current_inode;        
      }
      null_space = diff == 0 ? 1 : 0;
      last_index = diff == 0 ? 1 : diff;    
      char *next = malloc(sizeof(char)*(diff + 1 + null_space));
      if(!next){
        perror("malloc");
        exit(1);
      }    
      strncpy(next,path_cpy,diff);         
      next[last_index] = '\0';           
      path_cpy += (diff + 1)*sizeof(char);
      current_inode = find_next_inode(inode_table, next, current_inode);
      free(next);
    }

    else{      
      char *next = malloc(sizeof(char)*(strlen(path_cpy) + 1));
      if(!next){
        perror("malloc");
        exit(1);
      }
      len = strlen(path_cpy);   
      strcpy(next, path_cpy);
      next[len] = '\0';      
      path_cpy += len * sizeof(char);      
      current_inode = find_next_inode(inode_table, next, current_inode);
      free(next);
    }
    if(current_inode == 0){
      
      return current_inode;
    }
    
    //if not at the end of the path, check that the current_inode is a dir
    //i.e. cannot traverse through a file type other than a dir
    if(path_cpy[0] != '\0'){
      //check that current inode is a directory
      if((inode_table[current_inode - 1].i_mode & EXT2_S_IFDIR) != EXT2_S_IFDIR){        
        fprintf(stderr, "Invalid path\n");
        exit(EINVAL);
      }
    }    
    
  }
  
  return current_inode;
}
/*
Return inode number for next entry, else return 0
*/
unsigned int find_next_inode(struct ext2_inode *inode_table, char *next, unsigned int current_inode){
  //printf("utils 153 %s %u\n",next, current_inode);
  unsigned int size = inode_table[current_inode - 1].i_size;
  int dir_block_num;
  
  //dir_block_num = inode_table[current_inode - 1].i_block[0];
  int j;
  for(j = 0 ; j < SINGLE_INDIR_BLOCK_INDEX ; j++){
    if(size == 0){
      break;
    }
    dir_block_num = inode_table[current_inode - 1].i_block[j];
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num));

    if(size > EXT2_BLOCK_SIZE){
      unsigned int buf_size = 0;

      while(buf_size < EXT2_BLOCK_SIZE){
        char name[dir_entry->name_len + 1]; //need to append null terminator
        strcpy(name, dir_entry->name);
        name[dir_entry->name_len] = '\0';
        
        //found the entry
        if(strcmp(name, next) == 0){
          
          return dir_entry->inode;
        }
        buf_size += dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num) + buf_size);

      }
      size -= (unsigned int) EXT2_BLOCK_SIZE;

    }
    else{
      unsigned int buf_size = 0;
      while(buf_size < size){
        char name[dir_entry->name_len + 1]; //need to append null terminator
        strcpy(name, dir_entry->name);
        name[dir_entry->name_len] = '\0';
        
        //found the entry
        if(strcmp(name, next) == 0){
          
          return dir_entry->inode;
        }
        buf_size += dir_entry->rec_len;
        dir_entry = (struct ext2_dir_entry *) (disk + (EXT2_BLOCK_SIZE * dir_block_num) + buf_size);
      }
      size -= buf_size;
    }
  }
  return 0;
}

/*
Prints the name of the directory entry if the inode != 0
Returns the rec_len of the entry
*/
unsigned int print_dir_entry(struct ext2_dir_entry *dir_entry){
  unsigned int buf_size = 0;
  if(dir_entry->inode != 0){
    char name[dir_entry->name_len + 1]; //need to append null terminator
    strcpy(name, dir_entry->name);
    name[dir_entry->name_len] = '\0';
    printf("%s\n",name);
  }
  buf_size += dir_entry->rec_len;

  return buf_size;

}

/*
Reset the bit at the index in the given bitmap to a 0
*/
void reset_bit(int index, unsigned int *bitmap){
  unsigned int offset = index/32;
  bitmap += offset;
  *bitmap &= (unsigned int)(~(1 << (index % 32)));
}

/*
Set the bit at the index in the given bitmap to a 1
*/
void set_bit(int index, unsigned int *bitmap){
  unsigned int offset = index/32;
  bitmap += offset;
  *bitmap |= (unsigned int)((1 << (index % 32)));
}

/*
Add a directory entry to the given inode
*/
void add_direntry(struct ext2_dir_entry *dir_entry, struct ext2_inode *inode, char * file_name){
  
  unsigned int i;  
  unsigned short entry_size = nearest_multiple_four(dir_entry->name_len + DIR_STRUCT_SIZE);
  unsigned short curr_size = 0;

  struct ext2_dir_entry *curr_entry;
  unsigned char *block_ptr;
  unsigned char *block_end_addr;

  for(i = 0; i < SINGLE_INDIR_BLOCK_INDEX; i++){
    //block is populated
    if(inode->i_block[i] > 0){
      
      block_ptr = disk + inode->i_block[i]*EXT2_BLOCK_SIZE;
      block_end_addr = block_ptr + EXT2_BLOCK_SIZE;

      curr_entry = (struct ext2_dir_entry *)block_ptr;
      curr_size = nearest_multiple_four(curr_entry->name_len + DIR_STRUCT_SIZE);

      //loop until space for dir_entry is found
      //last entry will point to end of block, so we want the curr_entry be the last entry in the block
      while(block_ptr < block_end_addr && ((unsigned short)(curr_entry->rec_len - curr_size)) < entry_size){
        
        block_ptr += curr_entry->rec_len;
        curr_entry = (struct ext2_dir_entry *)block_ptr;
        curr_size = nearest_multiple_four(curr_entry->name_len + DIR_STRUCT_SIZE);        
      }        

      //if there is space to add the entry in the block
      if((curr_entry->rec_len - curr_size) >= entry_size){
        dir_entry->rec_len = curr_entry->rec_len - curr_size;        
        curr_entry->rec_len = curr_size;
        block_ptr += curr_size;
        struct ext2_dir_entry *new_entry  = (struct ext2_dir_entry *)block_ptr;
        new_entry->inode = dir_entry->inode;
        new_entry->rec_len = dir_entry->rec_len;
        new_entry->name_len = dir_entry->name_len;
        new_entry->file_type = dir_entry->file_type;

        memcpy(block_ptr + sizeof(struct ext2_dir_entry), file_name, new_entry->name_len);        
        
        return;
        
      }
      
    }
    //need to find a new free block
    else{
      unsigned int free_block = find_first_free_block();
      if(free_block == 0){
        fprintf(stderr,"No space left on device\n");
        exit(ENOSPC);
      }
      inode->i_block[i] = free_block;
           
      dir_entry->rec_len = EXT2_BLOCK_SIZE;      
      
      struct ext2_dir_entry *new_entry  = (struct ext2_dir_entry *)(disk + inode->i_block[i]*EXT2_BLOCK_SIZE);
      new_entry->inode = dir_entry->inode;
      new_entry->rec_len = dir_entry->rec_len;
      new_entry->name_len = dir_entry->name_len;
      new_entry->file_type = dir_entry->file_type;
      memcpy(disk + inode->i_block[i] * EXT2_BLOCK_SIZE + sizeof(struct ext2_dir_entry), file_name, new_entry->name_len);
           
      
      return;
    }
  }

  return;
}

/*
Return the block number of the first free block in the block bitmap, else return 0
*/
unsigned int find_first_free_block() {
    unsigned int *block_bitmap = (unsigned int *)(disk + (block_group_desc->bg_block_bitmap * EXT2_BLOCK_SIZE));

    //iterate through all blocks to find the first one
    unsigned char *block_bitmap_location = (disk + ((block_group_desc->bg_block_bitmap) * EXT2_BLOCK_SIZE));
    unsigned int mask = 0x40; //particular mask we will be using
    unsigned int starting_bit = 2; //start at 9th bit, the 2nd bit in the 2nd char
    unsigned int block_to_start_at = 17; //blocks before this are served

    
    while (block_to_start_at < 128) {
        //when we find the first free block:
        if ((block_bitmap_location[starting_bit] & mask) == 0) {
          super_block->s_free_blocks_count--;
          block_group_desc->bg_free_blocks_count--;
          block_bitmap_location[starting_bit] |= mask;
          memset(disk + (block_to_start_at * EXT2_BLOCK_SIZE), 0, EXT2_BLOCK_SIZE);      
          set_bit(block_to_start_at - 1, block_bitmap);
          return block_to_start_at;
        }
        
        mask >>= 1;
        if (mask == 0) {
            mask = 0x80;
            starting_bit++;
        }
        block_to_start_at++;
    }
  return 0;
}

/*
Return the inode number of the first inode in the inode bitmap, else return 0
*/
int find_first_free_inode_num() {

  unsigned int *inode_bitmap = (unsigned int *)(disk + (block_group_desc->bg_inode_bitmap * EXT2_BLOCK_SIZE));
  int first_free_inode_num = 0;
  int j = 0;

  for(j = 11; j < super_block->s_inodes_count; j++){
    if(!(inode_bitmap[0] & (1 << j))){     
      super_block->s_free_inodes_count--;
      block_group_desc->bg_free_inodes_count--;
      first_free_inode_num = j + 1;
      set_bit(first_free_inode_num-1, inode_bitmap);
      break;
      }
  }
  return first_free_inode_num;
}

/*
Return the nearest multiple of 4 that is greater than size.
If size is already a multiple of 4, return size
*/
unsigned short nearest_multiple_four(unsigned char size){
  return (unsigned short) ((size + 3) & ~0x3);
}
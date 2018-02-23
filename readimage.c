#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>

unsigned char *disk;


int main(int argc, char **argv) {

  if(argc != 2) {
    fprintf(stderr, "Usage: readimg <image file name>\n");
    exit(1);
  }
  int fd = open(argv[1], O_RDWR);

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  //ex8
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  printf("Inodes: %d\n", sb->s_inodes_count);
  printf("Blocks: %d\n", sb->s_blocks_count);


  struct ext2_group_desc *ab = (struct ext2_group_desc *)(disk + 2048);
  printf("Block group:\n");
  printf("    block bitmap: %d\n", ab->bg_block_bitmap);
  printf("    inode bitmap: %d\n", ab->bg_inode_bitmap);
  printf("    inode table: %d\n", ab->bg_inode_table);
  printf("    free blocks: %d\n", ab->bg_free_blocks_count);
  printf("    free inodes: %d\n", ab->bg_free_inodes_count);
  printf("    used_dirs: %d\n", ab->bg_used_dirs_count);


  //ex9
  unsigned int * bm = (unsigned int *)(disk + 3072);

  int i,j,tabCounter;
  tabCounter = 0;
  printf("Block bitmap: \t");
  for(i = 0; i < 128/32 ; i++){

    for(j = 0; j < 32; j++){
      if(bm[i] & (1 << j)){
        printf("1");
      }
      else{
        printf("0");
      }
      tabCounter++;
      if(tabCounter == 8){
        printf(" ");
        tabCounter = 0;
      }
    }

  }


  printf("\n");
  printf("Inode bitmap: \t");
  unsigned int *inode_bitmap = (unsigned int *)(disk + 4096);
  i = 0;
  j = 0;
  tabCounter = 0;
  for(j = 0; j < 32; j++){
    if(inode_bitmap[0] & (1 << j)){
      printf("1");
    }
    else{
      printf("0");
    }
    tabCounter++;
    if(tabCounter == 8){
      printf(" ");
      tabCounter = 0;
    }
  }
  printf("\n");
  printf("\n");


  printf("Inodes:\n");
  struct ext2_inode *inode_table = (struct ext2_inode *)(disk + 5120);
  char file_type[1];
  for(j = 1; j < 32; j++){

    if( (j > 1) && (j < 11)){
      continue;
    }
    else{
      if(inode_bitmap[0] & (1 << j)){
        if((inode_table[j].i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK){
          *file_type = 's';
        }
        else if((inode_table[j].i_mode & EXT2_S_IFREG) == EXT2_S_IFREG){
          *file_type = 'f';
        }
        else if((inode_table[j].i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR){
          *file_type = 'd';
        }
        else{
          fprintf(stderr, "Incorret file type\n");
          exit(1);
        }
        printf("[%d] type: %c size: %u links: %hu blocks: %u\n",
              j+1,*file_type,inode_table[j].i_size,inode_table[j].i_links_count,
              inode_table[j].i_blocks);
        printf("[%d] Blocks: %d\n", j+1, inode_table[j].i_block[0]);
      }
      else{
        continue;
      }
    }
  }
  printf("\n");

  //ex10
  printf("Directory Blocks:\n");
	unsigned int size;
  int dir_block_num;
  for(j = 1; j < 32; j++){
    if((1 < j && j < 11) || inode_table[j].i_blocks == 0 || !S_ISDIR(inode_table[j].i_mode)){
      continue;
    }
    size = 0;
    dir_block_num = inode_table[j].i_block[0];
    struct ext2_dir_entry *dir_entry = (struct ext2_dir_entry *) (disk + (1024 * dir_block_num));
    printf("\tDIR BLOCK NUM: %d (for inode %d)\n",dir_block_num,j+1);
    //read the dir
    while(size < inode_table[j].i_size){
      if(dir_entry->file_type & EXT2_FT_UNKNOWN){
        *file_type = 'u';
      }
      else if(dir_entry->file_type & EXT2_FT_REG_FILE){
        *file_type = 'f';
      }
      else if(dir_entry->file_type & EXT2_FT_DIR){
        *file_type = 'd';
      }
      else if(dir_entry->file_type & EXT2_FT_SYMLINK){
        *file_type = 's';
      }

      char name[dir_entry->name_len + 1]; //need to append null terminator
      strcpy(name, dir_entry->name);
      name[dir_entry->name_len] = '\0';
      printf("Inode: %u rec_len: %hu name_len: %d type= %c name=%s\n",
              dir_entry->inode, dir_entry->rec_len, dir_entry->name_len, *file_type, name);
      size += dir_entry->rec_len;
      dir_entry = (struct ext2_dir_entry *) (disk + (1024 * dir_block_num) + size);
    }
  }

  return 0;
}

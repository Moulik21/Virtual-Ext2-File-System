#include <stdio.h>

int main() {
  char *path = "home/lru.c";
  int diff;
  int null_space;
  int last_index;
  char *current_pt;
  char *next_pt;
  unsigned int current_inode = 2;
  unsigned int next_inode = 0;
  //path += sizeof(char);//remove the / in the start
  if(strchr(path, '/')!= NULL){
    diff = strchr(path,'/') - path;
    null_space = diff == 0 ? 1 : 0;
    last_index = diff == 0 ? 1 : diff;    
    char current[diff + 1 + null_space];
    if(diff == 0){
      current[0] = '/';
      current_pt = current;
    }
    else{
      strncpy(current,path,diff);
      current_pt = current;
    }
    current[last_index] = '\0';
    printf("%s\n",current_pt);   
  }
  path += diff + 1;
  
  if(strchr(path, '/')!= NULL){
    diff = strchr(path,'/') - path;
    null_space = diff == 0 ? 1 : 0;
    last_index = diff == 0 ? 1 : diff;    
    char next[diff + 1 + null_space];    
    strncpy(next,path,diff);
    next_pt = next;    
    next[last_index] = '\0';
    printf("%s\n",next_pt);   
  }
  
  //get next
  
  
  //will only be used for the last level in path
  // have reached the end of path
  // that means that last next is the current
  // return the 
  else{
    char next[strlen(path) + 1];
    strcpy(next, path);
    next[strlen(path)] = '\0';
    next_pt = next;
    printf("%s\n",next_pt);   
  }
  
  //find inode number for next
  
  //path += diff + 1;
  return 0;
}

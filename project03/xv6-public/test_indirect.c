#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

//#define DEBUG
#define N_STRESS 4
#define FILE_SIZE1 (8*1024*1024)   // 8MB
#define FILE_SIZE2 (16*1024*1024)  // 16MB

#define CHARS_LEN 62
char CHARACTERS[CHARS_LEN+1] = "11111111111111111111111111111111111111111111111111111111111111";

int test_write(const char* file_name, int FILE_SIZE);
int test_read(const char* file_name, int FILE_SIZE);
int test_stress(const char* file_name, int FILE_SIZE);

int
main(int argc, char *argv[])
{
  // int i;
  char file_name1[8] = "test_f1";
  char file_name2[8] = "test_f2";

  printf(1, "test_indirect starting\n");

  printf(1, "1. 8MB file test\n");
  // Create Test
  if(test_write(file_name1, FILE_SIZE1) != 0){
    printf(1, "painc at test_write\n");
    exit();
  }else{
    printf(1, "test_write ok\n");
  }

  // Read Test
  if(test_read(file_name1, FILE_SIZE1) != 0){
    printf(1, "painc at test_read\n");
    exit();
  }else{
    printf(1, "test_read ok\n");
  }
  printf(1, "1. 8MB file test ok\n");

  printf(1, "2. 16MB file test\n");
  // Create Test
  if(test_write(file_name2, FILE_SIZE2) != 0){
    printf(1, "painc at test_write\n");
    exit();
  }else{
    printf(1, "test_write ok\n");
  }

  // Read Test
  if(test_read(file_name2, FILE_SIZE2) != 0){
    printf(1, "painc at test_read\n");
    exit();
  }else{
    printf(1, "test_read ok\n");
  }
  printf(1, "2. 16MB file test ok\n");

  printf(1, "test_indirect ok\n");

  // // Stress Test
  // for(i = 0; i < N_STRESS; i++){
  //   file_name[6] = '0' + i;
  //   if(test_stress(file_name) != 0){
  //     printf(1, "painc at test_stress[%d]\n", i);
  //     exit();
  //   }
  // }
  // printf(1, "test_stress ok\n");

  exit();
}

// Test for write
int test_write(const char *file_name, int FILE_SIZE){
  int fd, offset, size;
  
  // Remove
  unlink(file_name);

  // Open (create) 
  fd = open(file_name, O_CREATE | O_RDWR);
  if(fd < 0){
    printf(1, "open fail\n");
    return -1;
  }
  
  // Write
  offset = 0;
  while(offset < FILE_SIZE){
    size = (offset+CHARS_LEN < FILE_SIZE) ? CHARS_LEN : (FILE_SIZE-offset);

    if(write(fd, CHARACTERS, size) != size){
      printf(1, "write fail at %d\n", offset);
      close(fd);
      return -1;
    }

    offset += size;
  }

  close(fd);
  return 0;
}

// Test for read
int test_read(const char *file_name, int FILE_SIZE){
  int fd, offset, size, i;

  int buf_size = 200;
  char str[buf_size+1];

#ifdef DEBUG
  printf(1, "[test_read] %s: ", file_name);
#endif

  // Open
  fd = open(file_name, O_RDONLY);
  if(fd < 0){
    printf(1, "open fail\n");
    return -1;
  }
  
  // Read
  offset = 0;
  while(offset < FILE_SIZE){
    memset(str, 0, buf_size);

    size = (offset+buf_size < FILE_SIZE) ? buf_size : (FILE_SIZE-offset);

    if(read(fd, &str, size) != size){
      printf(1, "read fail at %d\n", offset);
      close(fd);
      return -1;
    }

    for(i = 0; i < size; i++){
#ifdef DEBUG
      printf(1, "%c", str[i]);
#endif
      // Matching
      if(str[i] != CHARACTERS[(offset+i)%CHARS_LEN]){
        printf(1, "\nmatch fail at %d (expected %c, but %c)\n",
            offset+i, str[i], CHARACTERS[(offset+i)%CHARS_LEN]);
        close(fd);
        return -1;
      }
    }

    offset += size;
  }

#ifdef DEBUG
  printf(1, "\n");
#endif
  
  close(fd);
  return 0;
}

int test_stress(const char *file_name, int FILE_SIZE){
  if(test_write(file_name, FILE_SIZE) != 0){
    printf(1, "test_write fail\n");
    return -1;
  }

  if(test_read(file_name, FILE_SIZE) != 0){
    printf(1, "test_read fail\n");
    return -1;
  }

  return 0;
}
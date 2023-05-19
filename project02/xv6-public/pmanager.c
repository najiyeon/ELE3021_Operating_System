#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define BUFFERSIZE 1024

int
getcmd(char *buf, int nbuf)
{
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[BUFFERSIZE];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    // 'list' command
    if(buf[0] == 'l' && buf[1] == 'i' && buf[2] == 's' && buf[3] == 't' && (buf[4] == ' ' || buf[4] == '\n')){
      // Print the information of the currently runnig processes
      pmanager_list();
      continue;
    }
    // 'kill' command
    else if(buf[0] == 'k' && buf[1] == 'i' && buf[2] == 'l' && buf[3] == 'l' && buf[4] == ' '){
        // Kill the process with the cerresponding pid
        // Get pid
        int pid = 0;
        int i = 0;
        while(buf[5+i] != ' '){
            int tmp = 10;
            for(int j=0; j<i; j++){
                tmp *= 10;
            }
            pid += atoi(buf[5+i]) * tmp;
            i++;
        }

        // Print success
        if(kill(pid) == -1){
            printf(1, "kill failed!\n");
        }
        else{
            printf(1, "kill success!\n");
        }
        continue;
    }
    // 'execute' command
    else if(buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'e' && buf[3] == 'c' && buf[4] == 'u' && buf[5] == 't' && buf[6] == 'e' && buf[7] == ' '){
        char *path;
        int stacksize;

        int i = 0;
        while(buf[8+i] != ' '){
            path[i] = buf[8+i];
            i++;
        }
        path[i] = '\0';
        i++;
        while(buf[8+i] != ' '){
            int tmp = 10;
            for(int j=0; j<i; j++){
                tmp *= 10;
            }
            stacksize += atoi(buf[8+i]) * tmp;
            i++;
        }

        if(exec2(path, path, stacksize) == -1){
            printf(1, "execute failed!\n");
        }
        continue;
    }
    // 'memlim' command
    else if(buf[0] == 'm' && buf[1] == 'e' && buf[2] == 'm' && buf[3] == 'l' && buf[4] == 'i' && buf[5] == 'm' && buf[6] == ' '){
        int pid = 0;
        int limit = 0;

        int i = 0;
        while(buf[7+i] != ' '){
            int tmp = 10;
            for(int j=0; j<i; j++){
                tmp *= 10;
            }
            pid += atoi(buf[7+i]) * tmp;
            i++;
        }
        i++;
        while(buf[7+i] != ' '){
            int tmp = 10;
            for(int j=0; j<i; j++){
                tmp *= 10;
            }
            limit += atoi(buf[7+i]) * tmp;
            i++;
        }

        // Print success
        if(setmemorylimit(pid, limit) == -1){
            printf(1, "memlim failed!\n");
        }
        else{
            printf(1, "memlim success!\n");
        }
        continue;
    }
    // 'exit' command
    else if(buf[0] == 'e' && buf[1] == 'x' && buf[2] == 'i' && buf[3] == 't' && (buf[4] == ' ' || buf[4] == '\n')){
        // Exit pmanager
        break;
    }
  }
  exit();
}
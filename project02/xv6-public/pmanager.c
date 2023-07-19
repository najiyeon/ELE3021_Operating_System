#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "param.h"

#define BUFFERSIZE 1024

int
getcmd(char *buf, int nbuf)
{
  printf(2, "pmanager > ");
  memset(buf, '0', nbuf);
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
      pmanagerList();
      continue;
    }
    // 'kill' command
    else if(buf[0] == 'k' && buf[1] == 'i' && buf[2] == 'l' && buf[3] == 'l' && buf[4] == ' '){
        // Kill the process with the cerresponding pid
        // Get pid
        int pid = 0;

        pid += atoi(&buf[5]);

        // printf(1, "pid: %d\n", pid);

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
        char path[60];
        memset(path, '\0', sizeof(path));
        char *argv[MAXARG];
        int stacksize = 0;
        int i = 0;

        while(buf[8+i] != ' ' && buf[8+i] != '\n'){
            path[i] = buf[8+i];
            i++;
        }
        i++;

        stacksize += atoi(&buf[8+i]);

        // printf(1, "path: %s stacksize: %d\n", path, stacksize);

        argv[0] = path;
        argv[1] = 0;

        int pid = fork();
        if(pid == 0){
            pid = fork();
            if(pid == 0){
                if(exec2(path, argv, stacksize) < 0){
                  printf(1, "execute failed!\n");
                }
            }
            exit();
        }
        else if(pid > 0){
            wait();
        }
        else{
            printf(1, "fork failed!\n");
        }
        continue;
    }
    // 'memlim' command
    else if(buf[0] == 'm' && buf[1] == 'e' && buf[2] == 'm' && buf[3] == 'l' && buf[4] == 'i' && buf[5] == 'm' && buf[6] == ' '){
        int pid = 0;
        int limit = 0;
        int i = 0;

        pid += atoi(&buf[7]);

        while(buf[7+i] != ' ' && buf[7+i] != '\n'){
            i++;
        }
        i++;

        limit += atoi(&buf[7+i]);

        // printf(1, "pid: %d limit: %d\n", pid, limit);

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

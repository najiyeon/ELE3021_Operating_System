#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000

#define NUM_THREAD 4
#define MAX_LEVEL 3

int parent;
int me;

int fork_children()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
    if ((p = fork()) == 0)
    {
      me = i;
      //sleep(5);
      return getpid();
    }
  return parent;
}

void exit_children()
{
  if (getpid() != parent)
    exit();
  while (wait() != -1);
}

int main(int argc, char *argv[])
{
  int i, pid;
  int count[MAX_LEVEL] = {0};
  //int child;

  parent = getpid();

  printf(1, "MLFQ test start\n");

  printf(1, "[Test 1] default\n");
  pid = fork_children();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 2)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
  }
  exit_children();
  printf(1, "[Test 1] finished\n");
  printf(1, "\n");

  printf(1, "[Test 2] priority\n");
  pid = fork_children();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 2)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
      if(me > 3){
        me = 3;
      }
      setPriority(pid, me);
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
  }
  exit_children();
  printf(1, "[Test 2] finished\n");
  printf(1, "\n");

  printf(1, "[Test 3] yield\n");
  pid = fork_children();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 2)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
      yield();
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
  }
  exit_children();
  printf(1, "[Test 3] finished\n");
  printf(1, "\n");

  printf(1, "[Test 4] scheduler lock\n");
  pid = fork_children();

  if (pid != parent)
  {
    if(me == NUM_THREAD - 1)
      schedulerLock(2021038122);
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 2)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }
      count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
    if(me == NUM_THREAD - 1)
      schedulerUnlock(2021038122);
  }
  exit_children();
  printf(1, "[Test 4] finished\n");
  printf(1, "\n");

  printf(1, "done\n");
  exit();
}
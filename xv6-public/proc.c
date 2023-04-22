#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

struct queue {
	struct spinlock lock; // queue lock
  struct proc *head;    // head pointer of the queue
  struct proc *tail;    // tail pointer of the queue
  int time_quantum;     // time quantum value of the queue
};

struct queue L0;   // L0 queue
struct queue L1;   // L1 queue
struct queue L2;   // L3 queue

// Enqueue a process to the tail of the queue
void enqueue(struct queue *q, struct proc *p) {
  acquire(&q->lock);
  p->next = 0; // set the next pointer of the process to null
  if(q->head == 0) { // if the queue is empty
    q->head = q->tail = p; // set both head and tail pointers to the process
  } else { // if the queue is not empty
    q->tail->next = p; // set the next pointer of the tail process to the process
    q->tail = p; // set the tail pointer to the process
  }
  release(&q->lock);
}

// Dequeue a process from the queue
void dequeue(struct queue *q, struct proc *p) {
  acquire(&q->lock);
  if(q->head == p){ // if the process to be dequeued is at the head of the queue
    q->head = p->next; // move the head pointer to the next process
    if(q->head == 0) // if the queue is empty after dequeueing the process
      q->tail = 0; // set the tail pointer to NULL
    p->next = 0; // set the next pointer of the dequeued process to NULL
  }
  else{ // if the process to be dequeued is not at the head of the queue
    struct proc *prev_p = q->head; // start from the head of the queue
    while(prev_p->next != p) // find the process just before the process to be dequeued
      prev_p = prev_p->next;
    prev_p->next = p->next; // set the next pointer of the previous process to the next process
    if(q->tail == p) // if the process to be dequeued is at the tail of the queue
      q->tail = prev_p; // move the tail pointer to the previous process
    p->next = 0; // set the next pointer of the dequeued process to NULL
  }
  release(&q->lock);
}


void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  p->queueLevel = 0; // set the queue level to 0
  p->timeQuantum = 0; // initialize the time quantum to 0
  p->priority = 3; // set the priority to 3

  enqueue(&L0, p);

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct proc *next_p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  // Set the time quantum of the queue
  L0.time_quantum = 4;
  L1.time_quantum = 6;
  L2.time_quantum = 8;


  for(;;){
    // Enable interrupts on this processor.
    sti();

    acquire(&ptable.lock);

    //cprintf("this is start\n");

    // If a process with scheduler_locked ==1 exists
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(p->scheduler_locked == 1 && p->pid > 0) {
        //cprintf("schedulerlocked!\n");
        for(;;) {
          if(p->pid < 1 || p->scheduler_locked != 1) {
            goto mlfq;
          }
          // Switch to chosen process.  It is the process's job
          // to release ptable.lock and then reacquire it
          // before jumping back to us.
          c->proc = p;
          switchuvm(p);
          p->state = RUNNING;
          
          swtch(&(c->scheduler), p->context);
          switchkvm();

          // Process is done running for now.
          // It should have changed its p->state before coming back.
          c->proc = 0;
        }
      } 		
	  }

    mlfq:
    // Check if a process exists in the L0 queue
    if(L0.head != 0){ //scheduling L0 queue
      for(p = L0.head; p != 0; p = next_p){
        if(p->state != RUNNABLE || p->pid <= 0){
          next_p = p->next;
          //cprintf("L0: not runnable\n");
          continue;
        }
        next_p = p->next;
        //cprintf("L0 scheduling!\n");

        // Run the process
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->timeQuantum++; //increase the process's timeQuantum
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        //cprintf("L0 process: scheduling done!\n");

        // If the process finished, remove it from L0
        if (p->state == ZOMBIE){
          //cprintf("remove form queue\n");
          dequeue(&L0, p);
          goto end;
        }
        // If process uses up its time quantum in L0, move it to L1
        else if(p->timeQuantum >= L0.time_quantum){
          //cprintf("use up timequantum\n");
          p->timeQuantum = 0;
          p->queueLevel = 1;

          // Remove from L0
          dequeue(&L0, p);
          // Add process to L1 queue
          enqueue(&L1, p);

          goto end;
        }
        // Move process to end of L0 queue
        else{
          //cprintf("move to end of queue\n");
          dequeue(&L0, p);
          enqueue(&L0, p);
          
          goto end;
        }
      }
    }
    // Check if a process exists in the L1 queue
    if(L1.head != 0){ //scheduling L1 queue
      for(p = L1.head; p != 0; p = next_p){
        if(p->state != RUNNABLE || p->pid <= 0){
          next_p = p->next;
          //cprintf("L1: not runnable\n");
          continue;
        }
        next_p = p->next;
        //cprintf("L1 scheduling!\n");

        // Run the process
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->timeQuantum++; //increase the process's timeQuantum
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        //cprintf("L1 process: scheduling done!\n");

        // Check if priority boosting has occured
        if(p->queueLevel == 0){
          goto end;
        }
        // If the process finished, remove it from L1
        if (p->state == ZOMBIE){
          //cprintf("remove form queue\n");
          dequeue(&L1, p);
          
          goto end;
        }
        // If process uses up its time quantum in L1, move it to L2
        else if(p->timeQuantum >= L1.time_quantum){
          //cprintf("use up timequantum\n");
          p->timeQuantum = 0;
          p->queueLevel = 2;

          // Remove from L1
          dequeue(&L1, p);
          // Add process to L2 queue
          enqueue(&L2, p);

          goto end;
        }
        // Move process to end of L1 queue
        else{
          //cprintf("move to end of queue\n");
          dequeue(&L1, p);
          enqueue(&L1, p);

          goto end;
        }
      }
    }
    // Check if a process exists in the L2 queue
    if(L2.head != 0){ //scheduling L2 queue
      for(p = L2.head; p != 0; p = next_p){
        if(p->state != RUNNABLE || p->pid <= 0){
          next_p = p->next;
          //cprintf("L2: not runnable\n");
          continue;
        }
        next_p = p->next;
        //cprintf("L2 scheduling!\n");

        // Find the process with the highest priority
        struct proc *max_p = p;
        int max_priority = p->priority;
        int fcfs = 0; // flag to indicate if multiple processes with same priority exist

        for(struct proc *temp = p->next; temp != 0; temp = temp->next){
          if(temp->priority < max_priority){
            max_p = temp;
            max_priority = temp->priority;
            fcfs = 0; // reset flag if new maximum priority process is found
          }
          else if(temp->priority == max_priority){
            fcfs = 1; // set flag if process with same priority is found
          }
        }

        // If multiple processes with same priority exist, run the first one that arrived
        if(fcfs){
          for(struct proc *temp = p; temp != 0; temp = temp->next){
            if(temp->priority == max_priority){
              max_p = temp;
              break;
            }
          }
        }

        // Run the chosen process
        p = max_p;
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;
        p->timeQuantum++;
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        //cprintf("L2 process: scheduling done!\n");

        // Check if priority boosting has occured
        if(p->queueLevel == 0){
          goto end;
        }
        // If the process finished, remove it from L2
        if (p->state == ZOMBIE){
          //cprintf("remove form queue\n");
          dequeue(&L2, p);
          
          goto end;
        }
        // If process uses up its time quantum in L2, 
        // reduce priority and reset time quantum
        // and move to end of queue
        else if(p->timeQuantum >= L2.time_quantum){
          //cprintf("use up timequantum\n");
          if(p->priority>0 && p->priority<=3){
            p->priority--;
          }
          p->timeQuantum = 0;

          // Move process to end of L2 queue
          dequeue(&L2, p);
          enqueue(&L2, p);

          goto end;
        }
        else{
          //cprintf("move to end of queue\n");
          // Move process to end of L2 queue
          dequeue(&L2, p);
          enqueue(&L2, p);

          goto end;
        }
      }
    }
    end:
    //cprintf("this is end\n");
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  struct proc *p = myproc();
  if(p == 0){
    cprintf("yield fail : no current running process!\n");
    exit();
  }
  p->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//Returns the level of the queue to which the process belongs
int
getLevel(void)
{
  struct proc *p = myproc(); //declaring a pointer and assigning it to myproc()
  if (p == 0) { //if myproc() returns a null value
    return -1; //error indicating that the system call has failed
  }
  return p->queueLevel; //returns the level of queue
}

//Set the priority of the process of the corresponding pid
void
setPriority(int pid, int priority)
{
  if(priority < 0 || priority > 3){ //checks whether the priority argument is within a valid range (0~3)
    cprintf("Invalid priority value: %d\n", priority);
    return; //the priority argument is invalid
  }

  struct proc *current_proc, *parent_proc; //declaring current and prarent process pointer

  acquire(&ptable.lock); //acquires lock to ensure that it has exclusive access to the process table
  parent_proc = myproc(); ////assigning parent_proc to the current process

  //loop through all the processes in the process table
  for(current_proc = ptable.proc; current_proc < &ptable.proc[NPROC]; current_proc++){
    if(current_proc->pid == pid && current_proc->parent == parent_proc){ //checks if the pid of the current process matches the input pid and if the parent of the current process matches the parent_proc
      current_proc->priority = priority; //the priority of the process is set to the input priority
      release(&ptable.lock); //release lock
      return;
    }
  }

  release(&ptable.lock); //release lock if process with the specified pid was not found
}

//Implement priority boosting to prevent starvation
void
priorityBoosting(void)
{
  struct proc *p; //declaring a pointer

  acquire(&ptable.lock);

  for(p = L0.head; p != 0; p = p->next){
    p->priority = 3; //reset priority of all processes to 3
    p->timeQuantum = 0; //the time quantum of all processes is initialized
  }

  // Rebalance all processes in L1 to L0 queue
  for(p = L1.head; p != 0; p = p->next){
    p->queueLevel = 0; //all processes are rebalanced to the L0 queue
    p->priority = 3; //reset priority of all processes to 3
    p->timeQuantum = 0; //the time quantum of all processes is initialized

    // Remove from L1
    dequeue(&L1, p);
    // Move to L0
    enqueue(&L0, p);
  }

  // Rebalance all processes in L2 to L0 queue
  for(p = L2.head; p != 0; p = p->next){
    p->queueLevel = 0; //all processes are rebalanced to the L0 queue
    p->priority = 3; //reset priority of all processes to 3
    p->timeQuantum = 0; //the time quantum of all processes is initialized

    // Remove from L2
    dequeue(&L2, p);
    // Move to L0
    enqueue(&L0, p);
  }

  release(&ptable.lock);
}

//Ensure that the process is scheduled with priority
void
schedulerLock(int password)
{
  struct proc *p = myproc(); //declaring a pointer and assigning it to the current process

  if(password != 2021038122){
    cprintf("Incorrect password!, pid: %d, time quantum: %d, current queue level: %d\n", p->pid, p->timeQuantum, p->queueLevel);
    kill(p->pid); //kill the process
    return;
  }

  ticks = 0; //reset global tick count to 0

  struct proc *tmp;

  if(p->scheduler_locked != 1){
    acquire(&ptable.lock); //acquire lock
    /*check if a process holding the scheduler lock already exists*/
    for(tmp = ptable.proc; tmp < &ptable.proc[NPROC]; tmp++){
      if(tmp != p && tmp->scheduler_locked == 1 && tmp->pid > 0){
        cprintf("A process holding the scheduler lock already exists!\n");
        exit();
      }
    }
    p->scheduler_locked = 1; //set scheduler lock to true
    release(&ptable.lock); //release lock
  }

  yield();
}

//Stops the process from being scheduled with priority.
void
schedulerUnlock(int password)
{
  struct proc *p = myproc(); //declaring a pointer and assigning it to the current process

  if(password != 2021038122){
    cprintf("Incorrect password!, pid: %d, time quantum: %d, current queue level: %d\n", p->pid, p->timeQuantum, p->queueLevel);
    kill(p->pid); //kill the process
    return;
  }

  acquire(&ptable.lock);
  p->scheduler_locked = 0; //set scheduler lock to false

  p->queueLevel = 0; //move to L0 queue
  p->priority = 3; //set priority to 3
  p->timeQuantum = 0; //reset time quantum
  release(&ptable.lock);
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

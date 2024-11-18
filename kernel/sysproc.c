#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

void
backtrace(){
  uint64 fp = r_fp();
  uint64 pg = PGROUNDDOWN(fp);
  printf("backtrace:\n");
  printf("%p\n", (void*)*(uint64*)(fp - 8));
  uint64 pfp = *(uint64*)(fp -16);
  while(pfp < pg+PGSIZE){
    printf("%p\n", (void*)*(uint64*)(pfp - 8));
    fp = pfp;
    pfp = *(uint64*)(fp - 16);
  }
}


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;
  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace();
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigalarm(void)
{
  int ticks;
  uint64 addr;
  struct proc* p = myproc();

  argint(0, &ticks);
  argaddr(1, &addr);
  p->ticknum = ticks;
  p->handler = addr;
  return 0;
}

uint64
sys_sigreturn(void)
{
  struct proc* p = myproc();

  struct trapframe trapframe = myproc()->save;
  p->trapframe->epc = trapframe.epc;
  p->trapframe->a0 = trapframe.a0;
  p->trapframe->a1 = trapframe.a1;
  p->trapframe->a2 = trapframe.a2;
  p->trapframe->a3 = trapframe.a3;
  p->trapframe->a4 = trapframe.a4;
  p->trapframe->a5 = trapframe.a5;
  p->trapframe->a6 = trapframe.a6;
  p->trapframe->a7 = trapframe.a7;
  p->trapframe->ra = trapframe.ra;
  p->trapframe->sp = trapframe.sp;
  p->trapframe->s0 = trapframe.s0;
  p->trapframe->s1 = trapframe.s1;
  p->trapframe->s2 = trapframe.s2;
  p->trapframe->s3 = trapframe.s3;
  p->trapframe->s4 = trapframe.s4;
  p->trapframe->s5 = trapframe.s5;
  p->trapframe->s6 = trapframe.s6;
  p->trapframe->s7 = trapframe.s7;
  p->trapframe->s8 = trapframe.s8;
  p->trapframe->s9 = trapframe.s9;
  p->trapframe->s10 = trapframe.s10;
  p->trapframe->s11 = trapframe.s11;
  p->trapframe->t0 = trapframe.t0;
  p->trapframe->t1 = trapframe.t1;
  p->trapframe->t2 = trapframe.t2;
  p->trapframe->t3 = trapframe.t3;
  p->trapframe->t4 = trapframe.t4;
  p->trapframe->t5 = trapframe.t5;
  p->trapframe->t6 = trapframe.t6;
  p->returned = 1;
  return trapframe.a0;
}

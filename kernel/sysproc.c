#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

#define VMASTART (MAXVA / 2)

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
sys_mmap(void)
{
  uint32 len;
  int prot, flags, fd;
  uint64  off;
  struct proc *p;
  struct file *f;
  struct vma* v, *pv;

  p = myproc();
  if(p->vmacnt >= NVMA)
    panic("mmap: too many vmas");

  argint(1,(int *)&len);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argaddr(5, &off);

  if(fd < 0)
    panic("mmap: fd < 0");
  f = p->ofile[fd];
  filedup(f);

  v = &p->vmas[p->vmacnt];
  if(p->vmacnt == 0){
    v->addr = PGROUNDUP(VMASTART);
  }else{
    pv = &p->vmas[p->vmacnt - 1];
    v->addr = pv->addr + PGROUNDUP(pv->len);
  }

  p->vmacnt++;
  v->permission = PTE_U;
  if(prot & PROT_READ && f->readable){
    v->permission |= PTE_R;
  }
  if(prot & PROT_WRITE){
    if(flags & MAP_PRIVATE || f->writable){
      v->permission |= PTE_W;
    }else{
      p->vmacnt--;
      return -1;
    }
  }

  v->len = len;
  v->flags = flags;
  v->f = f;
  v->off = off;
  //printf("v->addr is %p\n", (void *)v->addr);
  return v->addr;
}
void m_unmap(int k, struct proc *p, uint64 addr, uint32 len){
  uint64 pa, foff, end;
  int n;
  struct vma* v = &p->vmas[k];
  uint32 fsz = v->f->ip->size;

  end = addr + PGROUNDUP(len);
  for(uint64 a = addr; a < end; a += PGSIZE){
    if((pa = walkaddr(p->pagetable, a)) == 0){
      continue;
    }

    if(v->flags & MAP_SHARED && v->f->writable){
      foff = v->off + (a - v->addr);
      n = PGSIZE;
      if(foff + PGSIZE >= fsz){
        n = fsz % PGSIZE;
      }

      begin_op();
      ilock(v->f->ip);
      writei(v->f->ip, 0, pa, foff, n);
      iunlock(v->f->ip);
      end_op();
    }
    uvmunmap(p->pagetable, a, 1, 1);
  }

}

uint64
sys_munmap(void)
{
  uint32 len;
  uint64 addr;
  struct proc* p;
  struct vma* v = 0;

  p = myproc();
  argaddr(0, &addr);
  argint(1, (int *)&len);
  addr = PGROUNDDOWN(addr);

  uint64 start, bound;
  int k;
  for(k = 0; k < p->vmacnt; k++){
    start = p->vmas[k].addr;
    bound = start + p->vmas[k].len;
    if( addr >= start && addr < bound){
      v = &p->vmas[k];
      break;
    }
  }

  if(!v)
    return -1;

  m_unmap(k, p, addr, len);

  //unmap the whole region
  if(addr == v->addr && len == v->len){
    if(v->f)
      fileclose(v->f);
    v->addr = 0;
    v->len = 0;
    v->off = 0;
    if(p->vmacnt > 1){
      if(k != p->vmacnt - 1){
        for(int i = k; i < p->vmacnt - 1; i++){
          p->vmas[i] = p->vmas[i+1];
        }
      }
    }

    p->vmacnt--;
  }else if(addr == v->addr && len < v->len){
    v->addr += len;
    v->len -= len;
    v->off += len;
  }else if(addr > v->addr && len < v->len){
    v->len -= len;
    v->off += len;
  }

  return 0;
}

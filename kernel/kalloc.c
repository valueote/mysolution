// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct cpumem {
  struct spinlock lock;
  struct run* freelist;
};

struct cpumem per_cpu_mem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    initlock(&per_cpu_mem[i].lock, "kmem_per_cpu");
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int id = cpuid();
  struct cpumem* cpu_mem = &per_cpu_mem[id];
  acquire(&cpu_mem->lock);
  r->next = cpu_mem->freelist;
  cpu_mem->freelist = r;
  release(&cpu_mem->lock);
  pop_off();
}

void*
steal(int myid){
  struct run* r;
  for(int i = 0; i < NCPU; i++){
    if(i == myid)
      continue;
    struct cpumem* cpu_mem = &per_cpu_mem[i];
    acquire(&cpu_mem->lock);
    r = cpu_mem->freelist;
    if(r){
      cpu_mem->freelist = r->next;
      release(&cpu_mem->lock);
      break;
    }
    release(&cpu_mem->lock);
  }
  return r;
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int id = cpuid();
  struct cpumem* cpu_mem = &per_cpu_mem[id];
  acquire(&cpu_mem->lock);
  r = cpu_mem->freelist;

  if(r){
    cpu_mem->freelist = r->next;
    release(&cpu_mem->lock);
  }else{
    release(&cpu_mem->lock);
    r =  steal(id);
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end, int hart);
void kfree_helper(void *pa, int hart);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  int i;
  char *prev = end;
  for (i = 0; i < NCPU; ++i) {
    initlock(&kmem[i].lock, "kmem");

    char *next = prev + ((char*)PHYSTOP - prev) / (NCPU - i);
    next = (char *)PGROUNDUP((uint64)next);
    freerange(prev, next, i);
    prev = next;
  }
}

void
freerange(void *pa_start, void *pa_end, int hart)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_helper(p, hart);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree_helper(void *pa, int hart)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[hart].lock);
  r->next = kmem[hart].freelist;
  kmem[hart].freelist = r;
  release(&kmem[hart].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc_helper(int hart)
{
  struct run *r;

  acquire(&kmem[hart].lock);
  r = kmem[hart].freelist;
  if(r)
    kmem[hart].freelist = r->next;
  release(&kmem[hart].lock);
  
  if(r == 0) {
    for (int i = 0; i < NCPU; ++i) {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
kfree(void *pa)
{
  push_off();
  int hart = cpuid();
  kfree_helper(pa, hart);
  pop_off();
}

void *
kalloc(void) 
{
  push_off();
  int hart = cpuid();
  void *addr = kalloc_helper(hart);
  pop_off();
  return addr;
}
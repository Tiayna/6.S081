// Physical memory allocator, for user processes,    用户进程的物理内存分配
// kernel stacks, page-table pages,  内核栈和页表
// and pipe buffers. Allocates whole 4096-byte pages.   管道缓存
// PGSIZE页表大小4096Bytes

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

//定义了一个链表，每个链表都指向上一个可用空间，这个kmem就是一个保存最后链表的变量
struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

// Free the page of physical memory pointed at by v,
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

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

//统计当前剩余空闲页大小
uint64
FreeMem(void)
{
  struct run* r;
  uint16 Num_FreePage=0;

  acquire(&kmem.lock);

  r=kmem.freelist;   //指向第一个可用页表
  while(r)    //沿着链表遍历，直到为空
  {
    Num_FreePage++;
    r=r->next;
  }

  release(&kmem.lock);

  return Num_FreePage * PGSIZE;   //返回可用页表数*页表大小即为剩余内存空间
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
    //kmem.freelist永远指向第一个可用页
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

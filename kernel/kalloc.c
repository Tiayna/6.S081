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

struct {
  struct spinlock lock;
  struct run *freelist;  
} kmem[NCPU];    //将kalloc的共享freelist改为每个CPU独立的freelist

void
kinit()
{
  char buf[10];
  for(int i=0;i<NCPU;i++)
  {
    snprintf(buf,10,"kmem_CPU%d",i);   //为对应序号的CPU的freelist内存分配锁写入名称
    initlock(&kmem[i].lock,buf);    //为每个CPU的freelist初始化锁
  }
  // initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void   //一段内存区域Free
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

  push_off();   //关中断intr_off()
  int cpu=cpuid();    //获得当前cpu的id，该过程需要关中断
  pop_off();    //开中断intr_on()

  acquire(&kmem[cpu].lock);    //通过为单个CPU分配锁，可减少锁争用tot次数
  r->next = kmem[cpu].freelist;    //当前cpu获得内存分配锁
  kmem[cpu].freelist = r;          //将free掉的空闲内存段插到头部
  release(&kmem[cpu].lock);
}

// Allocate one 4096-byte page of physical memory. 从当前CPU的freelist分配出一块空闲的4KB内存
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 若当前CPU有空闲内存块，则返回；否则，从其他cpu对应的freelist中借用一块空闲内存块
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu=cpuid();
  pop_off();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r)   //当前CPU有空闲块，直接分配后返回
    kmem[cpu].freelist = r->next;
  else    //否则“借用”
  {
    //struct run* temp;   //借来的内存块（放到当前cpu中去）
    for(int i=0;i<NCPU;i++)
    {
      if(cpu==i) continue;  //跳过当前没有空闲块的cpu

      acquire(&kmem[i].lock);  //遍历到的CPU内存分配上锁
      r=kmem[i].freelist;   //分配（窃取）内存块（直接拿来使用）
      if(r)  kmem[i].freelist=r->next;   //若不为空，则偷取到遍历到的CPU的内存块
      release(&kmem[i].lock);  //遍历到的CPU内存分配解锁

      if(r) break;  //分配成功，则结束遍历，否则继续寻找下一个有空闲内存块的CPU
    }
  }
  release(&kmem[cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

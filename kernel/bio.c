// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13   //质数组13

struct {
  struct spinlock lock;    //缓存整体的自旋锁（防止死锁）
  //struct buf buf[NBUF];   //由静态数组组成的双向链表，存储内存缓存块

  struct buf hash[NBUCKET][NBUF];
  struct spinlock hashlock[NBUCKET];   //对每个哈希桶进行加锁

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.   //LRU
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKET];     //每个哈希桶的头部节点
} bcache;

void
binit(void)  //初始化缓存（数据结构（链表、哈希表）建立的过程）
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");   //初始化bcache.lock，缓存块的整体自旋锁

  for(int i=0;i<NBUCKET;i++)
  {
    initlock(&bcache.hashlock[i],"bcache");   //初始化每个哈希桶的锁
  
    // Create linked list of buffers
    //struct buf head -> struct buf head[NBUCKET]
    //struct buf buf[NBUF] -> struct buf hash[NBUCKET][NBUF]
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for(b = bcache.hash[i]; b < bcache.hash[i]+NBUF; b++){    //头插法逐个链接到bcache.head[i]后面
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];
      initsleeplock(&b->lock, "buffer");   //初始化缓存块buf中的睡眠锁
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 检查请求的磁盘块是否在缓存中，返回缓存命令结果
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hashcode=blockno % NBUCKET;  //Hash(blockno)
  acquire(&bcache.hashlock[hashcode]);   //获得对应哈希桶的锁

  // Is the block already cached?   磁盘块是否在缓存中(bcache[hashcode]中的有无对应的buf.blockno)
  for(b = bcache.head[hashcode].next; b != &bcache.head[hashcode]; b = b->next){   //环状结构
    if(b->dev == dev && b->blockno == blockno){   //cache命中
      b->refcnt++;   //引用次数++
      release(&bcache.hashlock[hashcode]);   //释放对应哈希桶的锁
      acquiresleep(&b->lock);
      return b;   //返回对应的缓存块（磁盘块的复制）
    }
  }

  // Not cached.   //缓存未命中
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[hashcode].prev; b != &bcache.head[hashcode]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hashlock[hashcode]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);   //转到底层的virtio_disk_rw()，将磁盘块从磁盘加载进缓存中(LRU)再返回
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);  

uint hashcode=b->blockno % NBUCKET;
  acquire(&bcache.hashlock[hashcode]);
  b->refcnt--;
  if (b->refcnt == 0) {    //没有引用之后将该buf从hash[hashcode]中插到空闲处
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    //头插
    b->next = bcache.head[hashcode].next;
    b->prev = &bcache.head[hashcode];
    bcache.head[hashcode].next->prev = b;
    bcache.head[hashcode].next = b;
  }
  
  release(&bcache.hashlock[hashcode]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}



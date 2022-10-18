struct buf {
  int valid;   // has data been read from disk?   缓存区是否包含了一个块的复制（cache命中）
  int disk;    // does disk "own" buf? 缓存区的内容是否已经被提交到了磁盘
  uint dev;    //设备号
  uint blockno;   //每块缓存区的块号（散列键值）
  struct sleeplock lock;
  uint refcnt;    //该块被引用次数
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};


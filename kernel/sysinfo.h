struct sysinfo {
  uint64 freemem;   // amount of free memory (bytes)     当前剩余的内存字节数
  uint64 nproc;     // number of process    状态为UNUSED的进程个数

  //uint64 freefd;    // number of free fie descriptor   当前进程可用文件描述符的数量
};

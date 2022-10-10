#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
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

  if(argint(0, &n) < 0)   //通过一个额外的函数获取参数（void传入的sys_sleep参数int）
    return -1;
  acquire(&tickslock);   //给时钟加锁，获取当前的时间
  ticks0 = ticks;
  while(ticks - ticks0 < n){    //比较是否到了sleep结束的时间
    if(myproc()->killed){      //myproc()返回指向当前进程的PCB
      release(&tickslock);
      return -1;     //进程结束了就退出，什么也不做
    }
    sleep(&ticks, &tickslock);   //否则继续睡眠
  }
  release(&tickslock);    //到时间了则释放时间的锁，返回程序
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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
sys_trace(void)
{
  int mask;
  if(argint(0,&mask)<0) return -1;
  myproc()->mask=mask;
  return 0;
}

uint64 
sys_sysinfo(void)
{
  uint64 addr;
  if(argaddr(0,&addr)<0) return -1;

  struct proc *p=myproc();
  struct sysinfo info;

  info.freemem=FreeMem();
  info.nproc=UnUsedProc();
  //info.freefd=UnUsedFile();

  if(copyout(p->pagetable,addr,(char*)&info,sizeof(info))<0) return -1;
  return 0;
}
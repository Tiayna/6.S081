#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } 
  else if((which_dev = devintr()) != 0){
    // ok
  } 
  //因为mmap()建立的vma是懒分配设计，只是将mapped的文件记录到vmas中，并没有映射到进程真正的页表
  //第一次访问映射的虚拟地址时，发生读写缺页失效，在trap中进行实际分配处理
  else if(r_scause()==13||r_scause()==15){  
    uint64 va=r_stval();  //获取发生缺页失效的虚拟地址
    int i;
    if(va>=p->sz||va<p->trapframe->sp) p->killed=1;    //虚拟地址越界（超出已分配或低于栈区），杀死进程
    else{
      for(i=0;i<NVMA;i++)   //遍历所有虚拟内存区域，检查缺页失效的地址是否在某个vma中
      {
        if(p->vmas[i].valid==1){   //找到第一个已使用的vma
          //判断是否在vma中，即缺页失效的虚拟地址范围在vma起始虚址addr的长度范围内
          if(p->vmas[i].addr<=va&&(p->vmas[i].addr+p->vmas[i].length)>va) break;   
        }
      }
      if(i==NVMA) p->killed=1;  //不在vma，则缺页失效非法，杀死进程
      else{   //在某个vma中，则进行分配物理页等相应处理
        uint64 ka=(uint64)kalloc();  //申请分配物理页
        if(ka==0) p->killed=1;  //申请失败，杀死进程
        else{  //将要映射的文件内容拷贝到该vma起始地址addr对应的刚分配的页表中
          memset((void*)ka,0,PGSIZE);
          va=PGROUNDDOWN(va);  //下行边界
          ilock(p->vmas[i].mapfile->ip);   //readi之前需要获取锁
          //映射文件的inode获取存储内容，输入ka是内核地址，读取文件到ka中
          readi(p->vmas[i].mapfile->ip,0,ka,va-p->vmas[i].addr,PGSIZE);
          iunlock(p->vmas[i].mapfile->ip);
          uint64 pm=PTE_U;   //默认文件可达
          if(p->vmas[i].prot & PROT_READ)  //判断文件是否可读
            pm |= PTE_R;  //对应设置页表
          if(p->vmas[i].prot & PROT_WRITE) //判断文件是否可写
            pm |= PTE_W;  //对应设置页表
          //建立映射关系：创建进程p从虚拟地址va开始的，页表pagetable中的页表项(pm)，映射到分配到的物理页表ka
          if(mappages(p->pagetable,va,PGSIZE,ka,pm)!=0)     //va->pte->ka
          {
            //建立失败，则释放分配的物理页表，并杀死进程
            kfree((void*)ka);  
            p->killed=1;
          }
        }
      }
    }
  }
  else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

# 6.S081
MIT OS
先说起因：个人操作系统的学习路线，先通过书籍《2023王道操作系统》考研书速成概念学习；
同时在实际中接触到操作系统方面的问题时查阅相关博客阅览，了解到MIT的一门知名公开课程6.S081，于是着手接触配置环境进行实际实践学习
实验尝试学习是在本人大三上半学年中进行，断断续续的一个多月，在对个人能力评估的基础上完成了11个labs中的6个（util,syscall,pgtbl,trap,lock,fs）
该说明文档简要概述了完成了的labs中的主要代码实现以及部分key要点。用于个人未来可能需要的实现思路回忆，其他branchs中的代码在写实验时已经自认为给出足够详细的注释，他人可借助理解

更新，忙完窒息考试周后花了几天把剩下的拓展功能类型的lab补完了，Lazy-Allocation、Copy-On-Write、mmap功能相似的三个
第11个lab:New Driver，类似于计算机网络和操作系统结合的实验，不如直接进行下一步阶段的计算机网络的实验学习

环境配置：实验运行在vscode内置远端拓展wsl(window subsystem of linux)，wsl链接的是在微软商店中下载的ubuntu(20.04)
在ubuntu shell中更新（sudo apt-get update...）后，输入课程官网要求配置安装(Installing via APT)项下的一行代码即可完成实验环境搭建

一：util
第一个实验是用来上手理解代码在底层运行逻辑的，包括父子进程的切换fork、管道的端口输入输出pipe等
1、sleep: 检测输入的同时直接调用系统调用的sleep即可
2、pingpong： 明确管道pipe的两端，父进程从p[1]端写入，子进程从p[0]端读出，注意缓冲区的使用
3、primes： 在数学原理的基础上通过子进程的递归创建new_proc(int p[2])实现，记得对管道进行读写的同时要对另一端close/open
4、find： 底层代码最难读懂的，沿目录树查找的代码仿照实现，通过判断当前stat类型为目录还是文件迭代遍历，直到找到T_FILE；stat.h：类似用于追踪文件类型的数据结构
5、xargs： 用于实现读取多行数据，先记忆标准输入中的第二个参数（系统调用命令，第一个参数为xargs），后再从标准输入中逐个读取输入的参数

二：syscall
第二个实验用于了解系统调用的实现转换以及相应的内核态与用户态的切换，在xv6中添加系统调用的流程以及相关的通过脚本实现的底层转换，还有系统调用通过寄存器传参的argxxx实现
1、system call trace： 实现一个可以追踪想要的系统调用的用户指令，通过位运算记录追踪的mask，在syscall()时，如果是要trace的则将其打印输出
2、sysinfo： 记录系统调用过程中空闲的内存、空闲的进程（学校课程中还要求记录空闲的文件描述符数量），通过宏定义中的NPORC遍历查找空闲进程，通过空闲内存块链表遍历查找空闲内存块实现

三：pgtbl
第三个实验展示了代码实现的内存的分页管理系统，xv6分页管理中的一些数据结构：页表pagetable_t，页表项pte_t；页表项物理地址(44+12)与虚拟地址(44+10)之间的转换，多级页表之间的递归查找
1、print a pagetable： 打印进程使用的页表，xv6的分页管理系统使用的是三级页表结构，即物理地址中的27位用于查找页表项，可通过标志位判断页表项使用情况以及level层级
2、a kernel pagetable per process： 内核中只有一个页表，要实现每个进程都有一份内核页表的拷贝，在进程结构体proc中添加一个新的用户内核页表（全局内核页表的拷贝），
仿照相关的初始化/释放函数，对该拷贝的页表进行对应的初始化和释放，以及相应的切换进程时的全局内核页表切换。（这么实现的目的就是能让内核直接去使用用户指针，而不用全局内核页表进行映射操作）
3、Simplify： 内核的copyin函数在读取用户指针时需要先切换为物理地址，利用2实现的用户内核页表，使copyin可以直接访问用户指针；将用户内核页表指针指向用户页表处即可(kpagetable->pagetable)

四：trap
第四个实验展示了xv6中函数调用的用户栈结构，将进程中的参数存储到寄存器中并用函数栈帧trapframe（基本单位）维护，用户栈结构中的函数调用与被调用关系，以及相应用于记录指示的栈底指针sp、当前栈帧指针fp
1、RISC-V assembly： 一些关于RISC-V指令系统的QA
2、backtrace： 要先了解xv6的用户栈中栈帧的结构关系，知道函数栈帧中返回地址与上一个栈帧的存储相对fp的位置，然后便可递归调用backtrace()回溯至用户栈顶
3、Alarm： xv6周期性的发出警告，可用于限制进程的CPU时间或者周期性调用的操作；通过添加系统调用sigalarm,sigreturn，添加的系统调用在测试程序中经过n个CPU时间片时便会调用对应的handler
作为alarm，同时由于sigalarm系统调用的缘故，寄存器的值必然会出现切换，于是需要提前记录好sigalarm前的栈帧trapframe情况，并在sigreturn中将其恢复

五：lock
第五个实验展示了xv6的锁机制，通过修改xv6的锁机制来提高并行性，减少锁冲突
1、Memory Allocator： 修改内存分配机制，与其让多个CPU共同竞争一个内存空闲块链表，不如为每个CPU单独分配一个kmem，使每个CPU内核使用独立的空闲内存块链表，避免锁冲突的情况，
同时在某个CPU没有空闲内存块时可以偷窃其他CPU的；通过修改kmem为数组（大小为NCPU），以及相应的kalloc/kfree操作来进行对应的空闲内存块分配与释放
2、Buffer cache： xv6中的bcache用于缓存磁盘数据块，仅仅一个bcache很容易在多核的情况下发生锁冲突，通过修改对bcache的访问情况来减少锁冲突；一种方法便是使用哈希表
将数据块号blockno作为key值散列，并为每个哈希桶（质数）bucket分配一个锁，以减少锁冲突（具体实现便是在代码中进行维度上升，并仿照原来的实现）；若不修改NBUF，由于桶的数量，势必需要分配更多的锁，需要修改NLOCK

六：fs
第六个实验展示了xv6的文件管理系统，以及inode链接对应的文件的相应结构，并且实现了大文件的二级数据块表和符号链接（软链接）
1、Large file： 原本的xv6的索引节点inode结构中包含12个直接映射和一个间接映射的数据块索引，最多对应存储12+1024/4个数据块，为了应付大文件，将inode中的addrs[11]指向一级间接数据块表头
addrs[12]指向二级简介数据块表头（doubly indirect），修改bmap，仿照原来的一级间接映射，实现二级间接映射：buf[bn/256][bn%256]
2、Symbolic links： 添加一个系统调用symlink实现软链接，对该链接文件的操作将转换为对被链接文件的操作；symlink()参数中，路径path为该文件所在路径，目标target为被链接文件的路径
可通过创建一个新的inode（类型为T_SYMLINK，即软链接），将target记录到这个inode的数据块中；同时修改sys_open，对T_SYMLINK类型进行处理，通过对inode读取数据到path中，实现符号链接，
迭代（或递归）查找被链接文件，同时定义一个递归深度，判断最终是否查找到文件主（实际拥有者）

七：thread
第七个实验展示了xv6的“多线程”实现，还有对类似于lock中锁冲突避免的哈希桶冲突避免，以及实现一个barrier来同步所有线程
1、switching between threads：实现一个用户态的线程调度，实际上在xv6中的“多线程”并非我们如今所说的那么多线程，本质上依旧是多个线程运行在一个CPU上，同时也没有通过类似时钟中断
之类的实现，而是通过线程自己本身调用yield来调用schedule进行线程调度，xv6具体通过类似进程一样的swich（uthread_swich）来切换上下文，同时需在线程结构体thread中额外添加
context来记录返回地址和栈地址以及相关寄存器上下文，并在线程调度时调用thread_switch()切换上下文
2、在多线程情况下，如果线程0调用insert，但未插入完成时，调度了线程1调用insert，此时线程1认为的哈希桶的链表头是原来的表头（而不是线程0插入的元素），就导致切换回线程0时
发生键值对覆盖，导致数据丢失，于是通过对哈希桶上锁来避免数据丢失
3、通过条件参数来实现barrier，线程到达point时会调用barrier，直到bstate.nthread到达nthread之前（全部线程抵达），都进入睡眠队列中等待

//八九十，三个lab都是只包括一个hard要求，与前面的介绍操作系统功能等不同，是在原来的基础上添加实现了几个操作系统中有用的优化技术
八：Lazy-Allocation
第八个实验向我们介绍了操作系统中的一个trick：“懒分配”；即：在系统调用sbrk()申请堆空间时，不实际分配物理内存，而是记录哪些用户地址被分配（用户进程的虚拟空间映射到分配的物理地址）
通过简单地增加用户进程地内存大小来实现“懒分配”，在实际读写该“懒分配”得到的内存时，在引发的缺页中断处理中，再进行真正的页表分配，然后再重新执行。
为什么要这么设计呢？因为实际上进程通常申请的内存比实际真正需要的多得多，一方面大量请求内存分配费时，另一方面实际未被使用又显得浪费，于是直接干脆不分配，等真正需要的时候再处理。
1、Eliminate allocation from sbrk():修改系统调用sbrk()，去除原有的分配物理地址的机制，直接注释掉growproc()，单纯增加进程内存大小p->sz；同时需要注意输入参数的正负的处理。
2、Lazy allocation:调用sbrk()申请物理地址分配后，进程内存大小增加，但相应的区域是非法的，此时会产生读写页表失效，通过在trap.c中添加修改相应代码来处理，通过r_scause()来获取相应标志寄存器判断失效类型，并通过r_stval()获取寄存器中记录的引发缺页失效的虚拟地址，在判断完引发失效的虚拟地址是否在合法范围内后（即没有超过堆栈的界限：va<p->sz||va>=p->trapframe->sp），进行相应的物理地址分配kalloc()与用户进程虚拟地址与新分配的物理页表的映射添加mappage() （懒分配设计的本质所在）；还有最后在uvmunmap()时发现虚拟地址在页表中无效时会panic，直接continue跳过处理即可。
3、Lazytests and Usertests：为了通过最后的测试，需要注意一些特殊处理，包括：sbrk()输入负参数时调用uvmdealloc()、出现分配失败或失效地址不在合法范围内的情况时，需要杀死进程p->killed=1、同时fork()父子进程需要有相同的懒分配情况，忽略未分配的内存、还有在内核态与用户态数据切换copyin/copyout时访问了申请但未分配的地址空间时，也要做相应的内存分配处理，在完善该鲁棒性时，明确了虚拟地址之间的映射关系的调用：（页表项）pte <= (walk) va (walkaddr) => pa（物理地址），而页表项与物理地址之间之间直接通过宏定义的位运算即可。

九：Copy-On-Write
第九个实验介绍的是另一个trick：“写时复制”；即：子进程不用在fork()时直接完完整整地拷贝父进程的用户空间，而是简单地创建一个页表，页表项记录存储指针(地址)指向父进程的物理页表，同时标记所有父子进程未写过的PTE；COW下的子进程在uvmcopy时，虚拟地址va映射到的不是新分配的ka，而是引用的父进程的pa，在后续访问虚拟地址va引发缺页中断时再进行相应的分配ka与拷贝引用的父进程物理页表处理。（思想本质与懒分配同源，但不可直接照搬）。
xv6操作系统原本fork()时，父进程的用户空间是全部复制到子进程的用户空间，就导致如果父进程地址空间太大的话，拷贝的过程会相当耗时，甚至在经常出现的fork()+exec()调用组合中，会使fork()中的复制操作完全浪费掉，通过Copy-On-Write，推迟分配和物理内存页的拷贝，直到拷贝内容真正需要用的时候（缺页中断引发时）再做处理。
1、Implement copy-on write：实现COW的关键点包括：系统调用fork()改写、缺页中断COW处理、维护物理页引用计数数组
fork()会调用uvmcopy()将父进程的用户空间复制给子进程，改写后不再分配，而是将父进程的物理页表pa映射给子进程的页表，同时将两个进程的对应PTE_W清零（没有写权限），并设置为COW类型的页表PTE_COW，并且该物理页表引用计数增加；由于进程没有写权限，后续引发写缺页中断时(r_scause()判断)，通过引发缺页失效的子进程的虚拟地址walk得到父进程的页表项pte，判断是否为COW类型，来进一步分配物理页表kalloc()，并对引用的父进程的物理页表拷贝memmove()，归还相应的写权限，取消COW类型，该物理页表的引用计数减少；同时在内核调用copyout()写入用户进程的COW页时，也需要进行类似的修改来分配物理页表来拷贝引用的父进程的物理页表（否则内核copyout写入的就是父进程的而非子进程的页表），通过uvmunmap()来减少引用计数。
在COW设计下，可能会出现一个物理页表会被多个子进程引用，此时不能随意释放该页表，通过引入一个引用计数数组来追踪，保证在最后一个引用消失时页表才可以被释放；物理物理页表引用计数的数组是通过物理地址/4096来索引的，全局计数来防止随时可能的被kfree()掉，在kalloc()分配物理页时初始化计数为1、uvmcopy()时计数增加、kfree()时计数减少并判断是否归零并进行相应的回收；同时通过引入spinlock来维护该计数数组在多线程下可能出现的问题，记得初始化锁。

十：mmap
第十个实验是关于优化文件系统进程读写控制、实现进程间内存共享的trick。mmap，即memory map,是一种内存映射文件的实现方式，将一个文件或其他对象映射到用户进程的地址空间，实现文件磁盘物理地址与用户进程空间的虚拟地址的一一对映关系；相较于传统的read/write系统调用：磁盘disk（外存） <==> 内核缓冲区（内存） <==> 用户缓冲（内存），mmap的实现可以不用经过中间的内核缓冲区（页缓冲），内核直接将磁盘上的数据拷贝到进程映射的共享内存中即可，减少了一次数据拷贝；同时mmap的实现也可以让不同进程映射到同一块地址空间，实现进程间信息共享。
mmap的实现主要由一种数据结构虚拟内存区域(vma)支持，包含：vma是否被使用valid、共享内存的起始虚拟地址addr、该内存长度length、映射的文件是否可读/可写prot、进程对文件的修改是否需要写回flags、映射的文件描述符mapfile；然后在进程结构体中添加该数据结构数组vmas[NVMA]来维护每个进程内存映射的文件；mmap设计下的vma，同懒分配思想一样，mmap只是将要映射的文件记录到进程的vmas[i].mapfile中，并没有映射到进程的页表中，在进程通过该虚址访问文件时，引发缺页中断来分配页表并读取映射的文件；而munmap主要借助判断虚址是否在某个vma范围内，然后通过uvmunmap()来解除用户进程内存到文件磁盘地址的映射（涉及较复杂的addr与length范围处理）。
在用户态user.h、usys.pl和内核态sysproc.h中添加系统调用mmap和munmap后，在sysfile.c中实现sys_mmap和sys_munmap、在trap.c中进行缺页失效处理，同时还需要修改fork()使父子进程有相同的内存映射文件（父进程的vmas复制到子进程的vmas），修改exit()使进程退出时unmap所有的vma映射的文件。
在具体实现过程中，还包括许多xv6文件系统读写的调用与inode节点的使用，如readi()、ilock()、filewrite()、filedup()、fileclose()、p->ofile[]等，对映射的文件进行包括读取、写回、计数、锁占用等，该实验中，无需考虑脏位，只要flags为MAP_SHARED则写回；mmap与munmap还有其他围绕内存与文件映射的函数，都是通过结构体vma维护的信息来记录处理的。

至此，6.S081作为我个人操作系统的上手实践学习就告一段落，简单的总结一下实验感受：
MIT教授开源的一个教学操作系统，为初学者很好的展现了一个类UNIX的操作系统架构和指令系统体系结构，从最基本简单的用户态内核态切换，到父子进程的创建及相关应用实现，到系统调用的实现转换和RISC-V的底层维护、到xv6操作系统维护内存分配管理、到xv6的中断处理机制和相应进程切换与栈帧切换、到xv6的锁维护和优化应用还有伪多线程管理和线程调度让出、到引出xv6与Linux内核下文件系统的架构，以及基于xv6内存管理与文件系统管理的相应功能trick拓展，实践了解了一个最基本的操作系统该有的功能调用与运行逻辑。
从刚开始接触配Linux内核环境，写第一个实验时对OS这种底层代码逻辑的陌生，到借助xv6-book慢慢熟悉大概，实验越往后写对xv6的越能有一个进一步的了解；个人感觉下来，xv6的内存管理理解起来较为容易，而文件系统方面的两个lab借助相关的查阅指导理解起来依旧较为晦涩，原因认为是对Linux环境下的文件管理仍了解地不够多，未来如果还有机会的话，需要进一步了解不同操作系统下的管理维护

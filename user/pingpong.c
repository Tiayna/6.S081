#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// int main(int argc,char const* argv[])
// {
//     int pid;  //记录当前进程
//     int p[2];
//     pipe(p);  //创建管道

//     if(fork()==0)   //子进程
//     {
//         char buf[2];
//         pid=getpid();
//         if(read(p[0],buf,1)!=1)   //②子进程读取
//         {
//             printf("child: failed to read\n");   //接收失败
//             exit(-1);
//         }
//         close(p[0]);
//         printf("%d: received ping\n",pid);
//         if(write(p[1],buf,1)!=1)
//         {
//             printf("child: failed to write\n");
//             exit(-1);
//         }
//         close(p[1]);
//         exit(1);
//     }
//     else    //父进程:send a byte
//     {
//         char send[2]="a";    //a byte between two processes over a pair of pipes
//         char buf[2];
//         pid=getpid();
//         if(write(p[1],send,1)!=1)   //①：父进程写入
//         {
//             printf("parent: failed to write\n");
//             exit(-1);
//         }
//         close(p[1]);
//         wait(0);    //③等待子进程结束
//         if(read(p[0],buf,1)!=1)
//         {
//             printf("parent: failed to read\n");
//             exit(-1);
//         }
//         printf("%d: received pong\n",pid);
//         close(p[0]);
//         exit(1);
//     }
// }

int main(int argc,char const* argv[])
{
    int pid;
    int p[2];
    pipe(p);
    //p[1]为写入端，p[0]为读出端，两者（文件标识符）指同一个
    //父子进程的文件描述符一模一样（fork会复制父进程的文件描述符和内存）
    if(fork()==0)   //子进程
    {
        pid=getpid();
        char buf[2];
        if(read(p[0],buf,1)!=1)    //将缓存区buf中的一个字节写入管道
        {
            exit(-1);
        }
        close(p[0]);
        printf("%d: received ping\n",pid);
        if(write(p[1],buf,1)!=1)
        {
            exit(-1);
        }
        close(p[1]);
        exit(0);
    }
    else            //父进程
    {
        pid=getpid();
        char buf[2];
        char send[2]="a";    //send a byte
        if(write(p[1],send,1)!=1)    
        {
            exit(-1);
        }
        close(p[1]);
        wait(0);      //等待子进程结束
        if(read(p[0],buf,1)!=1)
        {
            exit(-1);
        }
        close(p[0]);
        printf("%d: received pong\n",pid);
        exit(0);
    }
}
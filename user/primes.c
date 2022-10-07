#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void New_proc(int p[2])
{
    int prime;   //Output Prime
    int IsOver;  //Whether Pipe is Empty
    int n;       //Number in Pipe without Prime
    close(p[1]);
    if(read(p[0],&prime,4)!=4)   //int:4 bytes
    {
        printf("Child process failed to read");
        exit(1);
    }
    printf("Prime: %d \n",prime);

    IsOver=read(p[0],&n,4);
    if(IsOver)   //管道中有剩余数待检验是否为倍数
    {
        int Next_p[2];   //子管道
        pipe(Next_p);
        if(fork()==0)   //子进程：下一轮筛选
        {
            New_proc(Next_p);
        }
        else
        {
            close(p[1]);
            close(Next_p[0]);
            if(n%prime) write(Next_p[1],&n,4);
            while(read(p[0],&n,4)) 
            {
                if(n%prime) write(Next_p[1],&n,4);
            }
            close(p[0]);
            close(Next_p[1]);
            wait(0);   //等待子进程返回
        }
    }
    exit(0);
}

int main(int argc,char const* argv[])
{
    int p[2];
    pipe(p);
    if(fork()==0)
    {
        New_proc(p);
    }
    else
    {
        close(p[0]);
        for(int i=2;i<=35;i++)
        {
            if(write(p[1],&i,4)!=4)
            {
                printf("Failed to write %d",i);
                exit(1);
            }
        }
        close(p[1]);
        wait(0);    //等待子进程结束
        exit(0);
    }
    exit(0);
}
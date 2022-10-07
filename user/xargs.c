//xargs: Extended Argument (参数扩展)
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"   //MAXARG声明New_argv长度

int Readline(int Exe_argc,char* New_argv[32])
{
    char buf[1024];
    int n=0;    //记录标准输入字符长度
    while(read(0,buf+n,1))   //从标准输入（文件描述符0）读取一个字节到缓存区buf
    {
        if(n==1023)         //限制命令行输入长度
        {
            printf("command arguments input too long\n");
            exit(1);
        }
        if(buf[n]=='\n')    //参数读取完成
        {
            break;
        }
        n++;
    }  
    buf[n]=0;
    if(n==0) return 0;   //命令行输入为空，返回0，随后结束进程

    int offset=0;   //缓存区字符串读取首字符偏移
    while(offset<n)
    {
        New_argv[Exe_argc]=buf+offset;   //指针指向字符串首字符(buf中的)
        while(buf[offset]!=' ' && offset<n) offset++;
        //while(buf[offset]==' ' && offset<n) offset++;     
        while(buf[offset]==' '&&offset<n) buf[offset++]=0;    //把缓冲区的空格设置为0  New_argv[]读取命令行参数是读取buf[]的(指针)
        Exe_argc++;
    }

    return Exe_argc;
}

int main(int argc,char const* argv[])
{
    if(argc<2)   //错误输入
    {
        printf("xargs needs one argument at least(command)\n");
        exit(1);
    }

    char* command=malloc(strlen(argv[1])+1);   //用于exec调用的filename
    strcpy(command,argv[1]);

    char* New_argv[MAXARG];    //char*数组，记录合并后的参数列表
    for(int i=1;i<argc;i++)    //从xargs调用的argv[1](echo)命令开始拷贝
    {
        New_argv[i-1]=malloc(strlen(argv[i])+1);
        strcpy(New_argv[i-1],argv[i]);
    }

    int Exe_argc;   //xargs合并命令行后的参数数目

    while((Exe_argc=Readline(argc-1,New_argv))!=0)   //命令行输入不为空
    {
        New_argv[Exe_argc]=0;   //最后一个参数必须为0
        if(fork()==0)    //使用fork()和exec()系统，子进程对每一行输入调用命令
        {
            exec(command,New_argv);   //xargs执行扩展后的参数  (New_argv[0]必须是该命令command本身)
            printf("command failed to exec");    //exec执行成功后不会返回到原来的调用进程
            exit(1);   
        }
        wait(0);  //父进程等待子进程运行完命令
    }
    exit(0);
}

// int readline(char *new_argv[32], int curr_argc){
// 	char buf[1024];
// 	int n = 0;    //防止参数过长越界
// 	while(read(0, buf+n, 1)){   //从标准输入（文件描述符0）中读取，
// 		if (n == 1023)
// 		{
// 			fprintf(2, "argument is too long\n");
// 			exit(1);
// 		}
// 		if (buf[n] == '\n')
// 		{
// 			break;
// 		}
// 		n++;    //命令行参数长度
// 	}
// 	buf[n] = 0;
// 	if (n == 0)return 0;
// 	int offset = 0;
// 	while(offset < n){      //读取命令行中的argv[]
// 		new_argv[curr_argc++] = buf + offset;     //char*[]，字符指针指向buf[]中对应字符串的首个字符
// 		while(buf[offset] != ' ' && offset < n){   //跳过当前字符串
// 			offset++;
// 		}
// 		while(buf[offset] == ' ' && offset < n){   //跳过单个或多个空格
// 			buf[offset++] = 0;
// 		}
// 	}
// 	return curr_argc;   //返回xargs合并后的实际参数数目
// }

// int main(int argc, char const *argv[])
// {
// 	if (argc <= 1)
// 	{
// 		fprintf(2, "Usage: xargs command (arg ...)\n");   
// 		exit(1);
// 	}
// 	char *command = malloc(strlen(argv[1]) + 1);   //argv[0]为xargs ， +1为字符串结束char:/0
// 	char *new_argv[MAXARG];   //新的字符串数组参数
// 	strcpy(command, argv[1]);    //command = "echo/0"
// 	for (int i = 1; i < argc; ++i)    //拷贝调用参数里除"xargs"外的参数
// 	{
// 		new_argv[i - 1] = malloc(strlen(argv[i]) + 1);    //在指针区域开辟内存存储需要合并的参数字符串
// 		strcpy(new_argv[i - 1], argv[i]);  //string copy
// 	}

// 	int curr_argc;
// 	while((curr_argc = readline(new_argv, argc - 1)) != 0)   //为每行运行一次
// 	{
// 		new_argv[curr_argc] = 0;   //argv[n]=0(最后一个参数为0)
// 		if(fork() == 0){
// 			exec(command, new_argv);    //运行命令command(filename)，以新的参数数组  
// 			fprintf(2, "exec failed\n");     //exec只在error时才return
// 			exit(1);
// 		}
// 		wait(0);   //等待子进程返回（正常情况下要ctrl+D手动结束输入）
// 	}
// 	exit(0);   //命令行输入为空时退出xargs
// }
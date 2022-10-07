#include "kernel/types.h"
#include "kernel/stat.h"    //头文件包含顺序问题
#include "user/user.h"
#include "kernel/fs.h"  //File System
//DIRSIZ:最大文件名长度

void find_helper(char const *path, char const *target)   //参考ls.c实现
{
	char buf[512], *p;
	int fd;    //文件描述符file descriptor
	struct dirent de;
	struct stat st;

	if((fd = open(path, 0)) < 0){  //打开指定查找路径
		fprintf(2, "find: cannot open %s\n", path);   //printf -> fprintf
		exit(1);    //return -> exit()
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		exit(1);
	}

	switch(st.type){
		case T_FILE:
			fprintf(2, "Usage: find dir file\n");
			exit(1);   //break -> exit()
		case T_DIR:
			if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
				printf("find: path too long\n");
				break;
			}
			strcpy(buf, path);
			p = buf + strlen(buf);
			*p++ = '/';    //路径字符数组结束标识
			while(read(fd, &de, sizeof(de)) == sizeof(de)){
				if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)  //不要递归进入.和..
					continue;
                // if(de.inum == 0)
                //     continue;
				memmove(p, de.name, DIRSIZ);
				p[DIRSIZ] = 0;
				if(stat(buf, &st) < 0){
        			printf("find: cannot stat %s\n", buf);
        			continue;
      			}
      			if(st.type == T_DIR){   //当前查找到目录
      				find_helper(buf, target);  //目录递归（递归进入子目录）
      			}else if (st.type == T_FILE){   //当前查找到文件
      				if (strcmp(de.name, target) == 0)    //目录名命中
      				{
      					printf("%s\n", buf);    //buf: fmtname(buf) , st.type , st.ino , st.size
      				}
      			}
                //printf("%s\n", buf);
			}
			break;
	}
	close(fd);
}
int main(int argc, char const *argv[])
{
	if (argc != 3)
	{
		fprintf(2, "Usage: find dir file\n");
		exit(1);
	}

	char const *path = argv[1];    //查找路径
	char const *target = argv[2];  //目标文件名
	find_helper(path, target);     //目录树查找匹配
	exit(0);
}
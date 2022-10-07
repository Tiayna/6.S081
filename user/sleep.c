#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		fprintf(2, "Sleep needs one argument!\n");
		exit(-1);
	}
	int time = atoi(argv[1]);
	sleep(time);
    printf("(nothing happens for a little while)\n");
	exit(0);
}
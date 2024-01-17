#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
int main()
{
	int result;
	if(-1==(result=gethostid()))
	{ 
		printf("gethostid err\n");
		exit(0);
	}
	printf("hosid is : %d\n",result);
	
	return 0;
}
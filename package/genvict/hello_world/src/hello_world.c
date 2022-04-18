#include <stdio.h>
int main(char argc, char *argv[])
{
    int i = 1;
    while(1){
        //1~10 循环
        printf("Hello world!!!%d\n",i); //打印内容
		if (i < 10){
			i++;
		}else{
			i = 1;
		}
			sleep(1);// 一秒钟打印一次
	}
	return 0;
}

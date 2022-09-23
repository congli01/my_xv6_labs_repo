#include "kernel/types.h"
#include "user.h"

int main(int argc, char* argv[])
{
	if(argc != 1){
		printf("pingpong needs no argument!\n");	// 检查参数数量是否正确
		exit(-1);
	}
    int pipe_p2c[2];    // 父进程到子进程
    int pipe_c2p[2];    // 子进程到父进程
    pipe(pipe_p2c);
    pipe(pipe_c2p);
    int pid = fork();
    if (pid == 0)
    {
        /*child*/
        /*从管道pipe_p2c读取；向管道pipe_c2p写入*/
        close(pipe_p2c[1]);    // 关闭写端
	    close(pipe_c2p[0]);
        char rstr[5] = {};
        read(pipe_p2c[0], rstr, 4);
        close(pipe_p2c[0]);    //读取完成，关闭读端 
        printf("%d:received %s\n", getpid(), rstr);
	    char wstr[] = "pong";
	    write(pipe_c2p[1], wstr, 4);
	    close(pipe_c2p[1]);
    }
    else if(pid > 0)
    {
        /*parent*/
        /*向管道pipe_p2c写入；管道pipe_c2p读取*/
        close(pipe_p2c[0]);    
        close(pipe_c2p[1]);    
        char wstr[] = "ping";
        write(pipe_p2c[1], wstr, 4);
        close(pipe_p2c[1]);    // 写入完成，关闭写端
        char rstr[5] = {};
        read(pipe_c2p[0], rstr, 4);
        close(pipe_c2p[0]);    //读取完成，关闭读端 
        printf("%d:received %s\n", getpid(), rstr);
    }
    exit(0);
}

# include "kernel/types.h"
# include "user.h"

// 筛选函数
void filtrate(int p1[])
{
    // 子进程从管道中读取父进程写入的第一个数字
    close(p1[1]);
    int prime;
    // 若管道中的数全部读完，关闭管道，退出进程
    if((read(p1[0], &prime, sizeof(prime))) == 0)    
    {
        close(p1[0]);
        exit(0);
    }
    printf("prime %d\n", prime);
    // 创建另一个管道
    int p2[2];
    pipe(p2);
    int pid = fork();   // 子进程再创建一个子进程
    int num;    // 待筛选数字
    if (pid == 0) 
    {
        filtrate(p2);   // 递归调用筛选函数
    }
    else if (pid > 0)
    {
        close(p2[0]);
        // 从管道1读数并进行筛选
        while ((read(p1[0], &num, sizeof(num))) > 0)
        {
            if (num % prime != 0)
            {
                write(p2[1], &num, sizeof(num));
            }
        }
        close(p2[1]);
        wait(0);    // 等待子进程退出
    }
    exit(0);
}


int main(int argc, char* argv[])
{
    // 创建管道
    int p[2];
    pipe(p);
    int pid = fork();
    if (pid == 0)
    {
        // 调用筛选函数
        filtrate(p);
    }
    else if (pid > 0)
    {
        close(p[0]);
        // 将2-35写入管道
        for (int i = 2; i < 36; i++)
        {
            write(p[1], &i, sizeof(i));
        }
        close(p[1]);
        // 等待子进程结束
        wait(0);
    }
    exit(0);
}

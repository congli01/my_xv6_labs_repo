# include "kernel/types.h"
# include "user.h"
# include "kernel/param.h"

int main(int argc, char *argv[]){
    //检查参数数量是否正确
    if(argc < 2)
    {
        printf("usage: xargs command\n"); 
        exit(-1);
    }
    char* argv2[MAXARG];    // 命令参数
    int index = 0;  // argv2数组的索引
    int i;
    for (i = 1; i < argc; i++)
    {
        argv2[index++] = argv[i];
    }
    // 从管道读取数据作为命令的追加参数
    char buffer[32] = {"\0"};    //缓冲区
    while((read(0, buffer, sizeof(buffer))) > 0)
    {
        char append[32] = {"\0"};    // 命令的追加参数
        argv2[index] = append;
        for(i = 0; i < strlen(buffer); i++)
        {
            // 如果读取到换行符，创建子进程执行命令
            if(buffer[i] == '\n')
            {
                if (fork() == 0)
                {
                    exec(argv[1], argv2);
                }
                wait(0);
            }
            else
            {
                append[i] = buffer[i];
            }
        }
    }
    exit(0);
}

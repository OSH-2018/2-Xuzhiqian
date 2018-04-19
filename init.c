#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include<stdlib.h>
#define RD_NONE 0
#define RD_APPEND 1     //>>
#define RD_NEW 2        //>
#define RD_STDIN 3      //<

typedef struct pipes{
    int num;
    char *args[256];
};

int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128];
    while (1) {
        /* 提示符 */
        char cur_wd[256];
        printf("%s # ",getcwd(cur_wd, 256));
        fflush(stdin);
        fgets(cmd, 256, stdin);
        /* 清理结尾的换行符 */
        int i;
        for (i = 0; cmd[i] != '\n'; i++)
            ;
        cmd[i] = '\0';
        /* 拆解命令行 */
        args[0] = cmd;
        for (i = 0; *args[i]; i++) {
            for (args[i+1] = args[i] + 1; *args[i+1] && *args[i+1]!=' '; args[i+1]++);
            for (; *args[i+1]==' '; args[i+1]++)
                *args[i+1]='\0';
        }
        args[i] = NULL;

        /* 没有输入命令 */
        if (!args[0])
            continue;

        /* 内建命令 */
        if (strcmp(args[0], "cd") == 0) {
            if (args[1])
                chdir(args[1]);
            continue;
        }
        if (strcmp(args[0], "pwd") == 0) {
            char wd[4096];
            puts(getcwd(wd, 4096));
            continue;
        }
        if (strcmp(args[0], "exit") == 0)
            return 0;

        if (strcmp(args[0], "export") == 0) {
            char key[256];
            char value[256];
            int i=0,j=0;
            if (*args[1]=='\0' || args[1]==NULL) {
                printf("Invalid syntax\n");
                continue;
            }

            for (;args[1][i]!='=';i++) key[i]=args[1][i];
            args[1][i]='\0';
            for (i++;args[1][j];i++,j++) value[j]=args[1][i];
            args[1][j]='\0';
            setenv(key,value,0);
            continue;
        }

        if (strcmp(args[0],"unset")==0){
            if (*args[1]=='\0' || args[1]==NULL) {
                printf("Invalid syntax\n");
                continue;
            }
            for (int i=1; args[i]; i++)
                unsetenv(args[i]);
            continue;
        }
        /*
        下面这段编译时提示：
        Undefined symbols for architecture x86_64:
            "_echo", referenced from:
            _main in init-c8a012.o
        到现在还是不懂为什么
        if (strcmp(args[0],"echo")==0){
            if (echo(args)!=0)
                continue;
            else
                exit(0);
        }*/

        struct pipes p;
        p.num = 0;
        int pipe_pointer = 0;
        int arg_pointer = 0;
        int prev_pipe = -1;
        for (i=0; args[i]; i++)
            if (*args[i]=='|')
                p.num++;

        /* 外部命令 */
        for (i=0; i<=p.num; i++) {
            int pipe_path[2];

            for (pipe_pointer=0; args[arg_pointer] && *args[arg_pointer]!='|';
                                        arg_pointer++, pipe_pointer++)
                p.args[pipe_pointer] = args[arg_pointer];
            p.args[pipe_pointer] = NULL;
            arg_pointer++;

            if (pipe(pipe_path)<0) {
                perror("pipe");
                break;
            }

            pid_t pid = fork();
            if (pid == 0) {
                close(pipe_path[0]);

                if ((i>0 && dup2(prev_pipe,STDIN_FILENO)<0) || (i<p.num && dup2(pipe_path[1],STDOUT_FILENO)<0)) {
                    perror("dup2");
                    exit(0);
                }
                if (i>0) close(prev_pipe);
                if (i<p.num) close(pipe_path[1]);


                for (int k=0;p.args[k];k++) {
                    int flag=RD_NONE;
                    if (strcmp(p.args[k],">>")==0)
                        flag=RD_APPEND;
                    else if (strcmp(p.args[k],">")==0)
                        flag=RD_NEW;
                    else if (strcmp(p.args[k],"<")==0)
                        flag=RD_STDIN;
                    if (flag==RD_NONE) continue;
                    int file1,file2;

                    p.args[k]=NULL;
                    if (flag==RD_APPEND || flag==RD_NEW) {
                        file1=open(p.args[k+1],O_CREAT | O_RDWR | (flag==RD_APPEND?O_APPEND:O_TRUNC),0644);
                        dup2(file1,STDOUT_FILENO);
                        close(file1);
                    }
                    else {
                        file2=open(p.args[k+1],O_CREAT | O_RDWR ,0644);
                        dup2(file2,STDIN_FILENO);
                        close(file2);
                    }

                }
                /* 子进程 */
                execvp(p.args[0], p.args);
            }
            /* 父进程 */
            close(pipe_path[1]);
            close(prev_pipe);
            if (i<p.num)
                prev_pipe = pipe_path[0];
            else {      //参考man waitpid示例代码
                int wstatus;
                do {
                    pid_t w = waitpid(pid, &wstatus, WUNTRACED | WCONTINUED);
                    if (w == -1) exit(0);
               } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
            }
        }
    }
}
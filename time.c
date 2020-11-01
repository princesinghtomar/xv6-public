#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int main(int argc,char** argv){
    int pid = fork();
    if(pid < 0){
        printf(2,"Error : failed to fork\n");
        exit();
    }
    if(!pid){
        if(argc == 1){
            printf(2,"Error : Please enter more command\n");
            for(int i=0;i<100;i++){
                int kapa;
                kapa = i++;
                kapa -= i;
                kapa = i;
            }
            exit();
        }
        /* if(argc == 2){
            for(int i=0;i<100;i++){
                int kapa;
                kapa = i++;
                kapa -= i;
                kapa = i;
            }
        } 
        else*/{
            int flag = exec(argv[1], argv + 1); 
            if (flag < 0)
			{
				printf(2, "Error : Failed to execute %s\n", argv[1]);
				exit();
			}
        }
    }
    if(pid > 0){
        int wtime;
        int rtime;
        int status_waitx = waitx(&wtime,&rtime);
        printf(1,"wtime = %d\nrtime = %d\nstatus_waitx = %d\n",wtime,rtime,status_waitx);
        exit();
    }
}
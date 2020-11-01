#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc,char** argv){
    int pid;
    int priority;
    if(argc != 3){
        printf(2,"Error : Please enter correct command\n");
        printf(2,"Usage : setPriority <pid> <priority>");
        exit();
    }
    else{
        pid = atoi(argv[2]);
        priority = atoi(argv[1]);
    }
    int val = chpr(priority,pid);
    if(val == -1){
        printf(1,"The process does not exist\n");
    }
    else if( val == -2){
        printf(1,"Priorities are in the Range [0,100]\n");
    }
    else{
        printf(1,"New priority - %d \t Old priority - %d \n",priority,val);
    }
    exit();
}
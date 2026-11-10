#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
int main(int argc,char **argv) {
 if(argc!=6){fprintf(stderr,"usage: runner EXE DATASET PLUGINS OUTPUT METRICS\n");return 2;}
 int out=open(argv[4],O_WRONLY|O_CREAT|O_TRUNC,0600),err=open(argv[5],O_WRONLY|O_CREAT|O_TRUNC,0600); if(out<0||err<0){perror("open");return 2;}
 struct timespec a,b; if(clock_gettime(CLOCK_MONOTONIC,&a)){perror("clock");return 2;} pid_t p=fork(); if(p==0){if(dup2(out,1)<0||dup2(err,2)<0)_exit(126);close(out);close(err);execl(argv[1],argv[1],argv[2],argv[3],(char*)NULL);_exit(127);} if(p<0){perror("fork");return 2;} close(out); close(err); int status;struct rusage ru; pid_t w; do {w=wait4(p,&status,0,&ru);}while(w<0&&errno==EINTR); if(w<0||clock_gettime(CLOCK_MONOTONIC,&b)){perror("wait/clock");return 2;} if(!WIFEXITED(status)||WEXITSTATUS(status)){fprintf(stderr,"child failed: status %d\n",status);return 1;} printf("%.6f,%ld,%ld\n",(b.tv_sec-a.tv_sec)*1000.0+(b.tv_nsec-a.tv_nsec)/1e6,ru.ru_maxrss,(long)p);return 0;
}

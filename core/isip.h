/*
 * Copyright (C) Chesley Diao
 */


#define LOG_DEBUG 	1
#define LOG_INFO	2
#define LOG_WARNING	3
#define LOG_CRIT	4
#define ISIP_OK		8
#define ISIP_ERR	0
#define MAX_PROCESS 1024
#define MAX_THREADS 2048
#define INIT_PROCESS 5
#define INIT_THREAD  10
#define INIT_IP			 INADDR_ANY
#define INIT_PORT		 6060
#define LISTENQ			 512
#define DEFAULT_PIDFILE "/var/run/isipsrv.pid"
#define DEFAULT_LOGFILE "/var/log/isipsrv.log"
#define DEFAULT_ERRLOG	"/var/log/isipsrv.err"
#define min(x, y) ({                            \
 typeof(x) _min1 = (x);                  \
 typeof(y) _min2 = (y);                  \
 (void) (&_min1 == &_min2);              \
 _min1 < _min2 ? _min1 : _min2; })


#include <netinet/in.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>

char* confpath;
int   procid[MAX_PROCESS];
char** isip_argv;
char* isip_argv_last;
int isip_argc;
int deadprocnum;
int iskill;

char* isip_log_buf(int,char *);
ssize_t isip_wr_file(int,char *,size_t);
void isip_strtok(char *,char *,char *);
char* isip_ltrim(char *);
char* isip_rtrim(char *);
int isip_open_file(char *,int);

struct task_t{
	struct epoll_event* ev;
	struct task_t* next;
};

struct proc_t{
	struct task_t* rhnode;
	struct task_t* whnode;
	pthread_mutex_t rchainlock;
	pthread_mutex_t wchainlock;
	sem_t rchainsem;
	sem_t wchainsem;
	sem_t chainsem;
};

typedef struct{
	char	istyle[4];
	int	len;
}isip_msg_t;

typedef struct{
	int maxprocess;
	int maxthreads;
	char* pidfile;
	char* logfile;
	char* errlog;
	
	int fd_pid;
	int fd_log;
	int fd_err;
	
	struct sockaddr_in serveraddr;
	int	listenfd;
	
	struct proc_t proc_v;

}isip_core_t;

void isip_open_log(isip_core_t*);


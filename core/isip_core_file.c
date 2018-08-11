/*
 * Copyright (C) Chesley Diao
 */


#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include "isip.h"


extern char **environ;

int initproctitle(void);
void setproctitle(char*,char*,int);
void isip_child_process(isip_core_t*);
void* pthread_proc(void*);
struct task_t* isip_chain_init(int);
void isip_chain_ins(struct task_t*,struct task_t*);
struct task_t* isip_chain_malloc(int,struct epoll_event*);
struct task_t* isip_chain_get(struct task_t*);
struct epoll_event* isip_chain_free(struct task_t*);
void isip_setnoblock(int,int);
int data_process(isip_core_t*,struct task_t*);
void dealdead(int);
void dealkill(int);
pid_t createproc(isip_core_t*);

void isip_daemon(isip_core_t* isip_cr_param){
	int i;
	size_t size = 0;
	size_t size1 = 0;
	char* ptr;
	char* p;
	char* p1;
	char	str[128];
	struct sigaction act;
	struct sigaction actkill;
	memset(&act,0,sizeof(act));
	memset(&actkill,0,sizeof(actkill));
	act.sa_handler = dealdead;
	actkill.sa_handler = dealkill;
	sigemptyset(&act.sa_mask);
	sigemptyset(&actkill.sa_mask);
	/*
	sigaddset(&act.sa_mask,SIGCHLD);
	 */
		
	switch(fork()){
		case -1:
			ptr = isip_log_buf(4,strerror(errno));
			if(ptr)
			 isip_wr_file(isip_cr_param->fd_err,ptr,strlen(ptr));
			unlink(DEFAULT_PIDFILE);
			exit(1);
		case 0:
			break;
		default:
			exit(0);
		}
	setsid();
	chdir("/");
	umask(022);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	/*
	 will process identifier writing process file
	 */
	snprintf(str,128,"%d\n",getpid());
	if(write(isip_cr_param->fd_pid,str,strlen(str)) < 0){
		ptr = isip_log_buf(3,strerror(errno));
		if(ptr)
		 isip_wr_file(isip_cr_param->fd_err,ptr,strlen(ptr));
		free(ptr);
		}
	
	if(!initproctitle()){
                ptr = isip_log_buf(4,strerror(errno));
		if(ptr)
                 isip_wr_file(isip_cr_param->fd_err,ptr,strlen(ptr));
                free(ptr);
		}
	for(i = 0;i < isip_argc;i++)
	 size += strlen(isip_argv[i]) +1;
	size++;
	p = p1  = (char*)malloc(size);
	if(!p){
		ptr = isip_log_buf(4,strerror(errno));
		if(ptr)
		 isip_wr_file(isip_cr_param->fd_err,ptr,strlen(ptr));
		free(ptr);
		}	
	for(i = 0;i < isip_argc;i++){
		*p++ = ' ';
		size1 = strlen(isip_argv[i]) + 1;
		strncpy(p,isip_argv[i],size1);
		p += size1 - 1;
		}
	setproctitle("isipsrv: master process",p1,size);
	free(p1);
	/*
	signal(SIGCHLD,dealdead);
	 */
	sigaction(SIGCHLD,&act,NULL);
	sigaction(SIGTERM,&actkill,NULL);
	sigaction(SIGQUIT,&actkill,NULL);
	sigaction(SIGTSTP,&actkill,NULL);
	signal(SIGPIPE,SIG_IGN);
			
	}

void isip_process_cycle(isip_core_t* isip_cr_param){
	int i;
	int j;
	int ischild=0;
	char* ptr;
	pid_t pid;
	
	for(i = 0;i < isip_cr_param->maxprocess;i++){
		switch(pid = fork()){
			case -1:
				ptr = isip_log_buf(3,strerror(errno));
				if(ptr)
					isip_wr_file(isip_cr_param->fd_err,ptr,strlen(ptr));
				free(ptr);
				ischild = 0;
				i--;
				break;
			case 0:
				ischild = 1;
				break;
			default:
				ischild = 0;
				for(j = 0;j < MAX_PROCESS;j++)
					if(procid[j] == 0){
						procid[j] = pid;
						break;
					}
		}
		if(ischild)
			break;
	}
	if(ischild){
		isip_child_process(isip_cr_param);
		exit(1);
	}
	/*
	 checking child process number
	 */
	while(1){
		for(;deadprocnum > 0 && !iskill;deadprocnum--){
			pid = createproc(isip_cr_param);
			if(pid < 0){
				deadprocnum++;
				continue;
			}					
			for(i = 0;i < MAX_PROCESS;i++)
				if(procid[i] == 0){
					procid[i] = pid;
					break;
				}
		}
		sleep(120);
	}
}

void isip_child_process(isip_core_t* isip_cr_param){
	
	int i;
	int terr;
	int epfd;
	int nfds;
	int confd;
	size_t ev_len;
	char*			str;
	pthread_t tid[isip_cr_param->maxthreads];
	
	pthread_attr_t attr;
	struct epoll_event ev,events[256];
	struct task_t* rnode;
	struct task_t* wnode;
	
	ev_len = sizeof(struct epoll_event);

	signal(SIGTERM,SIG_DFL);
	signal(SIGQUIT,SIG_DFL);
	signal(SIGTSTP,SIG_DFL);
	setproctitle("isipsrv: worker process",NULL,0);
	if(pthread_mutex_init(&isip_cr_param->proc_v.rchainlock,NULL) != 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
		}
	if(pthread_mutex_init(&isip_cr_param->proc_v.wchainlock,NULL) != 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
		}

	if(sem_init(&isip_cr_param->proc_v.rchainsem,0,0) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
		}
	if(sem_init(&isip_cr_param->proc_v.wchainsem,0,0) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
		}
	if(sem_init(&isip_cr_param->proc_v.chainsem,0,0) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
	}	
	isip_cr_param->proc_v.rhnode = isip_chain_init(isip_cr_param->fd_err);
	isip_cr_param->proc_v.whnode = isip_chain_init(isip_cr_param->fd_err);
	
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	for(i = 0;i < isip_cr_param->maxthreads;++i){
		terr = pthread_create(&tid[i],&attr,pthread_proc,(void*)isip_cr_param);
		if(terr){
			--i;
			str = isip_log_buf(3,strerror(terr));
			if(str)
			 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
			free(str);
			}
		}
	pthread_attr_destroy(&attr);
	
	epfd = epoll_create(512);
	if(epfd < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);		
		}
	isip_setnoblock(isip_cr_param->fd_err,isip_cr_param->listenfd);
	ev.data.fd = isip_cr_param->listenfd;
	ev.events = EPOLLIN|EPOLLET;
	if(epoll_ctl(epfd,EPOLL_CTL_ADD,isip_cr_param->listenfd,&ev) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		free(str);
		exit(2);
		}
		
	while(1){
		nfds = epoll_wait(epfd,events,256,-1);
		if(nfds < 0 && errno == EINTR){
			continue;			
			}
		if(nfds < 0){
			str = isip_log_buf(4,strerror(errno));
			if(str)
			 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
			free(str);
			exit(2);			
			}
		for(i = 0;i < nfds;++i){
			if(events[i].data.fd == isip_cr_param->listenfd){
				if((confd = accept(isip_cr_param->listenfd,NULL,NULL)) < 0){
					str = isip_log_buf(3,strerror(errno));
					if(str)
					 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
					free(str);
					continue;					
					}
				isip_setnoblock(isip_cr_param->fd_err,confd);
				ev.data.fd = confd;
				ev.events = EPOLLIN|EPOLLET;
				if(epoll_ctl(epfd,EPOLL_CTL_ADD,confd,&ev) < 0){
					str = isip_log_buf(3,strerror(errno));
					if(str)
					 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
					free(str);
					}
				
				}
			else if((events[i].events&EPOLLIN)&&(events[i].data.fd > 0)){
				rnode = isip_chain_malloc(isip_cr_param->fd_err,&events[i]);
				pthread_mutex_lock(&isip_cr_param->proc_v.rchainlock);
				isip_chain_ins(isip_cr_param->proc_v.rhnode,rnode);
				pthread_mutex_unlock(&isip_cr_param->proc_v.rchainlock);
				sem_post(&isip_cr_param->proc_v.rchainsem);
				sem_post(&isip_cr_param->proc_v.chainsem);
				}
			else if((events[i].events&EPOLLOUT)&&(events[i].data.fd > 0)){
				wnode = isip_chain_malloc(isip_cr_param->fd_err,&events[i]);
				pthread_mutex_lock(&isip_cr_param->proc_v.wchainlock);
				isip_chain_ins(isip_cr_param->proc_v.whnode,wnode);
				pthread_mutex_unlock(&isip_cr_param->proc_v.wchainlock);
				sem_post(&isip_cr_param->proc_v.wchainsem);
				sem_post(&isip_cr_param->proc_v.chainsem);
				}
			else{
				str = isip_log_buf(3,"epoll returned unknown event!");
				if(str)
				 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
				free(str);
				}
			}
		}
		
		sem_destroy(&isip_cr_param->proc_v.rchainsem);
		sem_destroy(&isip_cr_param->proc_v.wchainsem);
		sem_destroy(&isip_cr_param->proc_v.chainsem);	
		pthread_mutex_destroy(&isip_cr_param->proc_v.rchainlock);
		pthread_mutex_destroy(&isip_cr_param->proc_v.wchainlock);
		exit(2);
	}
	
void* pthread_proc(void* isip_cr_p){
	char* str;
	struct task_t* node;
	isip_core_t* isip_cr_param = (isip_core_t*)isip_cr_p;
	while(1){
		
		if(sem_wait(&isip_cr_param->proc_v.chainsem) < 0){
			str = isip_log_buf(3,strerror(errno));
			if(str)
			 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
			free(str);
			}
		else{
			if(sem_trywait(&isip_cr_param->proc_v.rchainsem) < 0){
				sem_post(&isip_cr_param->proc_v.chainsem);
				/*
				str = isip_log_buf(3,strerror(errno));
				if(str)
				 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
				free(str);
				 */
				}
			else{
				/*
				 get from reading buffer
				 */
				pthread_mutex_lock(&isip_cr_param->proc_v.rchainlock);
				node = isip_chain_get(isip_cr_param->proc_v.rhnode);
				pthread_mutex_unlock(&isip_cr_param->proc_v.rchainlock);
				if(node){
					data_process(isip_cr_param,node);
					}
				}
			}
		if(sem_wait(&isip_cr_param->proc_v.chainsem) < 0){
			str = isip_log_buf(3,strerror(errno));
			if(str)
			 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
			free(str);
			}
		else{
			if(sem_trywait(&isip_cr_param->proc_v.wchainsem) < 0){
				sem_post(&isip_cr_param->proc_v.chainsem);
				/*
				str = isip_log_buf(3,strerror(errno));
				if(str)
				 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
				free(str);
				 */
				}
			else{
				/*
				 get from writing buffer
				 */
				}
			}		
		
		}
	return((void*)3);
	}

char* isip_log_buf(int level,char* str){
	
	time_t isip_time;
	struct tm isip_tm;
	char str_log[1024];
	char str_level[10];
	char* save_str;
	
	if((isip_time = time(NULL)) < 0){
		//perror("time(NULL): %s\n");
		//exit(1);
		return(NULL);
		}
	localtime_r(&isip_time,&isip_tm);
	switch(level){
		case 1:
			strcpy(str_level,"DEBUG");
			break;
		case 2:
			strcpy(str_level,"INFO");
			break;
		case 3:
			strcpy(str_level,"WARNING");
			break;
		case 4:
			strcpy(str_level,"CRIT");
			break;
		default:
			strcpy(str_level,"UNKNOWN");
			
		}
		
	snprintf(str_log,sizeof(str_log),"%d-%02d-%02d %02d:%02d:%02d [ %s ]: %s\n",1900+isip_tm.tm_year,1+isip_tm.tm_mon,isip_tm.tm_mday,isip_tm.tm_hour,isip_tm.tm_min,isip_tm.tm_sec,str_level,str);
	save_str = (char*)malloc(strlen(str_log)+1);
	if(!save_str){
		//perror("malloc!");
		//exit(2);
		return(NULL);	
		}
	strcpy(save_str,str_log);
	return(save_str);

	}

ssize_t isip_wr_file(int fd,char* str,size_t nbytes){
	if(write(fd,str,nbytes) < 0){
		//printf("ERROR: %s ------> %s",strerror(errno),str);
		return(ISIP_ERR);
		}
		return(ISIP_OK);
	}
	
void isip_listen(isip_core_t* isip_cr_param){
	char* str;
	int on = 1;
	
	isip_cr_param->serveraddr.sin_family = AF_INET;
	
	isip_cr_param->listenfd = socket(PF_INET,SOCK_STREAM,0);
	if(isip_cr_param->listenfd < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		unlink(DEFAULT_PIDFILE);
		exit(1);	
		}
	
	if(setsockopt(isip_cr_param->listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		unlink(DEFAULT_PIDFILE);
		exit(1);		
		}	
		
	if(bind(isip_cr_param->listenfd,(struct sockaddr*)&isip_cr_param->serveraddr,sizeof(struct sockaddr_in)) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		unlink(DEFAULT_PIDFILE);
		exit(1);
		}
	
	if(listen(isip_cr_param->listenfd,LISTENQ) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(isip_cr_param->fd_err,str,strlen(str));
		unlink(DEFAULT_PIDFILE);
		exit(1);	
		}
		
	}
	
struct task_t* isip_chain_init(int logfd){
	struct task_t* ptr;
	char* str;
	size_t i = sizeof(struct task_t);
	ptr = (struct task_t*)malloc(i);
	if(!ptr){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(logfd,str,strlen(str));
		free(str);
		exit(2);
		}
	memset(ptr,0,i);
	return(ptr);
	}
	
void isip_chain_ins(struct task_t* headnode,struct task_t* addnode){
	
	addnode->next = headnode->next;
	headnode->next = addnode;
		
	}
	
struct task_t* isip_chain_malloc(int logfd,struct epoll_event* ev){
	struct task_t* ptr;
	char* str;
	size_t i = sizeof(struct task_t);
	ptr = (struct task_t*)malloc(i);
	if(!ptr){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(logfd,str,strlen(str));
		free(str);
		exit(2);
		}
	memset(ptr,0,i);
	ptr->ev = ev;
	return(ptr);
	}
struct task_t* isip_chain_get(struct task_t* headnode){
	
	struct task_t* ptr;
	if(headnode->next){
		ptr = headnode->next;
		headnode->next = ptr->next;
		return(ptr);
		}
	else
		return(NULL);	
	}
struct epoll_event* isip_chain_free(struct task_t* node){
	struct epoll_event* ev;
	ev = node->ev;
	free(node);
	return(ev);
	}

void isip_setnoblock(int logfd,int fd){
	char* str;
	int opts;
	opts = fcntl(fd,F_GETFL);
	if(opts < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(logfd,str,strlen(str));
		free(str);
		exit(2);
		}
	opts |= O_NONBLOCK;	
	if(fcntl(fd,F_SETFL,opts) < 0){
		str = isip_log_buf(4,strerror(errno));
		if(str)
		 isip_wr_file(logfd,str,strlen(str));
		free(str);
		exit(2);
		}
	}
	
int data_process(isip_core_t* isip_cr_param,struct task_t* node){
	
	ssize_t rlen;
	ssize_t wlen;
	char* savedptr;
	char* tempstr;
	char  msgbody[1024];
	isip_msg_t* msghead;

	struct epoll_event* ev = isip_chain_free(node);
	int	  savedfd = ev->data.fd;
	pthread_t tid = pthread_self();
	pid_t	  pid = getpid();
	size_t 	  len = sizeof(isip_msg_t);
	char	  buf[len];
	char	  logstr[512];
	char*     str = malloc(1536);
	savedptr  = str;
	
	while(1){
		str = savedptr;
		memset(str,0,1536);	
		rlen = read(ev->data.fd,buf,len);
		while(rlen < 0 && errno == EINTR)
		 rlen = read(ev->data.fd,buf,len);
		if(rlen == 0){
			ev->data.fd = -1;
			close(savedfd);
			//tempstr = isip_log_buf(3,"client has closed connection!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"client has closed connection!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			break;
			}
		if(rlen < 0)
		 break;
		if(rlen != len){
			//tempstr = isip_log_buf(3,"reading message header error! length of header error!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"reading message header error! length of header error!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			continue;
			}
		msghead = (isip_msg_t*)buf;
		if(msghead->len >= 1024 || msghead->len <= 0){
			//tempstr = isip_log_buf(3,"reading message header error! len in header error!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"reading message header error! len in header error!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			continue;
			}
			
		rlen = read(ev->data.fd,msgbody,msghead->len);
		while(rlen < 0 && errno == EINTR)
		 rlen = read(ev->data.fd,msgbody,msghead->len);
		if(rlen == 0){
			ev->data.fd = -1;
			close(savedfd);
			//tempstr = isip_log_buf(3,"client has closed connection!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"client has closed connection!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			break;
			}
		if(rlen < 0)
		 break;
		if(rlen != msghead->len){
			//tempstr = isip_log_buf(3,"reading message body not enough!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"reading message body not enough!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			msgbody[rlen] = '\n';
			write(isip_cr_param->fd_log,msgbody,rlen+1);
			continue;
			}
		msgbody[rlen] = '\0';
		snprintf(str,1536,"[pid:%d] [pthread_id:%ld] [fd:%d] recieved right data as follow: %s\n",pid,tid,ev->data.fd,msgbody);
		len = strlen(str);	
		wlen = write(isip_cr_param->fd_log,str,len);
		while(wlen < 0 && (errno == EAGAIN || errno == EINTR))
		 wlen = write(isip_cr_param->fd_log,str,len);
		wlen = write(ev->data.fd,str,len);
		while((wlen < 0 && (errno == EAGAIN || errno == EINTR)) || (wlen >= 0 && wlen < len)){
			if(wlen > 0){
				len -= wlen;
				str += wlen;
				}
			wlen = write(ev->data.fd,str,len);
			}
		if(wlen < 0 && errno == EPIPE){
			ev->data.fd = -1;
			close(savedfd);
			//tempstr = isip_log_buf(3,"client has closed connection!");
			snprintf(logstr,512,"[pid %d] [pthread_id %ld] :%s",pid,tid,"client has closed connection!");
			tempstr = isip_log_buf(3,logstr);
			if(tempstr)
			 isip_wr_file(isip_cr_param->fd_err,tempstr,strlen(tempstr));
			free(tempstr);
			break;		
			}	
		}
	free(savedptr);
	return(ISIP_OK);
	}	

void dealdead(int signo){
	int pid;
	int i;
	while((pid = waitpid(-1,NULL,WNOHANG)) > 0){
		deadprocnum++;
		for(i = 0;i<MAX_PROCESS;i++)
			if(procid[i] == pid){
				procid[i] = 0;
				break;
				}
		}
	}
	
void dealkill(int signo){
	//int i;
	iskill = 1;
	//for(i = 0;i<MAX_PROCESS;i++)
	 //if(procid[i] > 0)
	kill(0,signo);
	unlink(DEFAULT_PIDFILE);
	exit(0);			
	}

pid_t createproc(isip_core_t* isip_cr_param){
	pid_t pid;
	pid = fork();
	if(pid == 0){
		isip_child_process(isip_cr_param);
		exit(1);
	}
	else
		return(pid);	
	}
int initproctitle(){
	int i;
	size_t size = 0;
	char* p;
	for(i = 0;environ[i];i++){
		size += strlen(environ[i]) + 1;	
		}		
	p = (char*)malloc(size);
	if(!p)
	 return(ISIP_ERR);
	isip_argv_last = isip_argv[0];
	for(i = 0;isip_argv[i];i++){
		if(isip_argv_last == isip_argv[i])
			isip_argv_last += strlen(isip_argv[i]) + 1;
		}
	for(i = 0;environ[i];i++){
		if(isip_argv_last == environ[i]){
			size = strlen(environ[i]) + 1;
			isip_argv_last += size;
			strncpy(p,environ[i],size);
			environ[i] = p;
			p += size;
			}
		}
	isip_argv_last--;
	return(ISIP_OK);
	}

void setproctitle(char* title,char* p,int i){
	int size;
	char *ptr;
	/*
	isip_argv[1] = NULL;
	*/
	size = strlen(title) + 1;
	strncpy(isip_argv[0],title,size);
	ptr = isip_argv[0] + size - 1;
	if(p)
	 strncpy(ptr,p,i);
	ptr += i;
	while(isip_argv_last >= ptr)
	 *ptr++ = '\0';
	}






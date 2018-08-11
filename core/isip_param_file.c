/*
 * Copyright (C) Chesley Diao
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "isip.h"




void isip_init_param(isip_core_t* isip_cr_param){
	/*
	memset(isip_cr_param,0,sizeof(isip_core_t));
	memset(&procid[0],0,sizeof(int)*MAX_PROCESS);
	*/
	bzero(isip_cr_param,sizeof(isip_core_t));
	bzero(&procid[0],sizeof(int)*MAX_PROCESS);
	isip_cr_param->maxprocess = INIT_PROCESS;
	isip_cr_param->maxthreads = INIT_THREAD;
	isip_cr_param->pidfile = DEFAULT_PIDFILE;
	isip_cr_param->logfile = DEFAULT_LOGFILE;
	isip_cr_param->errlog = DEFAULT_ERRLOG;
	isip_cr_param->serveraddr.sin_addr.s_addr = htonl(INIT_IP);
	isip_cr_param->serveraddr.sin_port = htons(INIT_PORT);
   
	}

void isip_apply_param(isip_core_t* isip_cr_param){
	
	FILE* fp;
	char str[1024];
	char* str1;
	char left[512];
	char* left1;
	char right[512];
	char* right1;
	
	if((fp = fopen(confpath,"r")) == NULL){
		printf("open %s:	%s\n",confpath,strerror(errno));
		exit(1);
		}
	while(fgets(str,1024,fp) != NULL){
		memset(left,0,512);
		memset(right,0,512);
		str1 = isip_ltrim(str);
		if(*str1 == '#' || *str1 == 0)
			continue;
		isip_strtok(str1,left,right);
		left1 = isip_rtrim(left);
		right1 = isip_ltrim(right);
		right1 = isip_rtrim(right1);
		if(strcmp(left1,"maxprocess") == 0)
			isip_cr_param->maxprocess = min(MAX_PROCESS,atoi(right1));
		else if(strcmp(left1,"maxthreads") == 0)
			isip_cr_param->maxthreads = min(MAX_THREADS,atoi(right1));
		else if(strcmp(left1,"pidfile") == 0){
			isip_cr_param->pidfile = (char*)malloc(strlen(right1)+1);
			strcpy(isip_cr_param->pidfile,right1);
			}
		else if(strcmp(left1,"logfile") == 0){
			isip_cr_param->logfile = (char*)malloc(strlen(right1)+1);
			strcpy(isip_cr_param->logfile,right1);
			}
		else if(strcmp(left1,"errlog") == 0){
			isip_cr_param->errlog = (char*)malloc(strlen(right1)+1);
			strcpy(isip_cr_param->errlog,right1);
			}
		else if(strcmp(left1,"servip") == 0){
			isip_cr_param->serveraddr.sin_addr.s_addr = inet_addr(right1);
			}
		else if(strcmp(left1,"servport") == 0)
			isip_cr_param->serveraddr.sin_port = htons((uint16_t)atoi(right1));
		else
			continue;
		}
		fclose(fp);
	
	}
	
	
void isip_strtok(char* str,char* left,char *right){
	char* strptr[2];
	char* saveptr;
	int i;
	for(i = 0;i < 2;i++){
		if((strptr[i] = strtok_r(str,"=",&saveptr)) == NULL)
			break;
		str = NULL;
		}
	if(strptr[0])
		strcpy(left,strptr[0]);
	if(strptr[1])
		strcpy(right,strptr[1]);
	
	}
	
char* isip_ltrim(char* str){
	while(isspace(*str)){
		str++;
		}
	return(str);
	}
	
char* isip_rtrim(char* str){
	int strl;
	strl = strlen(str);
	while(isspace(*(str+(strl-1))) && strl > 0){
		*(str+(strl-1)) = 0;
		strl--;
		}
	return(str);
	}


void isip_open_log(isip_core_t* isip_cr_param){
	
	isip_cr_param->fd_pid = isip_open_file(isip_cr_param->pidfile,1);
	isip_cr_param->fd_log = isip_open_file(isip_cr_param->logfile,0);	
	isip_cr_param->fd_err = isip_open_file(isip_cr_param->errlog,0);
	//unlink(isip_cr_param->pidfile);
	}

int isip_open_file(char* filepath,int excltag){
	int fd;
	int O_EXCLTAG = 0;
	if(excltag)
		O_EXCLTAG |= O_EXCL;
	 
	if((fd = open(filepath,O_RDWR|O_APPEND|O_CREAT|O_EXCLTAG,0644)) < 0){
		if(excltag){
			puts("\n  isipsrv is running!\n");
			exit(1);
			}
		else{
			printf("open file:%s  %s",filepath,strerror(errno));
			exit(1);
			}
		}
		return fd;	
	}
	
	
	

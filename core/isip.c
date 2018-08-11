/*
 * Copyright (C) Chesley Diao
 */


#include <stdio.h>
#include "isip.h"


void isip_init_param(isip_core_t*);
void isip_apply_param(isip_core_t*);
void isip_daemon(isip_core_t*);
void isip_listen(isip_core_t*);
void isip_process_cycle(isip_core_t*);

int main(int argc,char** argv){

	deadprocnum = 0;
	iskill = 0;
	isip_core_t isip_cr_param;
	int ch;
	while((ch = getopt(argc,argv,"f:")) != -1){
		switch(ch){
			case 'f':
				confpath = optarg;
				break;
			case '?':
				puts("	invalid parameter!\n");
				puts("usage:\n	isipsrv [-f configfile]\n");
				exit(1);
				break;
			}
		}
	if(optind != argc){
		puts("Usage:\n	isipsrv [-f configfile]\n");
		exit(1);
		}

	/*
	 init parameter
	 */
	isip_argv = argv;
	isip_argc = argc;
	isip_init_param(&isip_cr_param);

	/*
	 read and apply parameter file
	 */
	if(confpath != NULL ){
		isip_apply_param(&isip_cr_param);
		}

	/*
	 open log file and pid file
	 */
	isip_open_log(&isip_cr_param);
	isip_daemon(&isip_cr_param);
	isip_listen(&isip_cr_param);

	isip_process_cycle(&isip_cr_param);










unlink(DEFAULT_PIDFILE);
exit(0);
}

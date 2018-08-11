CC = gcc
CFLAGS = -g -O -pthread -Wall
isipsrv: ./objs/isip.o ./objs/isip_core_file.o ./objs/isip_param_file.o
	$(CC) -pthread -o $@ $^
./objs/isip.o: ./core/isip.c ./core/isip.h 
	$(CC) $(CFLAGS) -c -o $@ $<
./objs/isip_core_file.o: ./core/isip_core_file.c ./core/isip.h 
	$(CC) $(CFLAGS) -c -o $@ $<
./objs/isip_param_file.o: ./core/isip_param_file.c ./core/isip.h 
	$(CC) $(CFLAGS) -c -o $@ $<

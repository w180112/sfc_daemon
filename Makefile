CFLAGS=-std=c99

sfc_daemon: sfc_daemon.o
	gcc -o sfc_daemon.o sfc_daemon
main.o: sfc_daemon.c
	gcc -c sfc_daemon.c
clean:rm -rf sfc_daemon*

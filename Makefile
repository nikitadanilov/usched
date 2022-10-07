CC=cc

all:
	$(CC) -O3 -Wall pmain.c -pthread -opmain
	$(CC) -fno-stack-protector -Wall usched.c -c -ousched.o
	$(CC) -O3 -fno-stack-protector rr.c usched.o rmain.c -pthread -ormain
	$(CC) -O3 -fno-stack-protector -DSINGLE_THREAD rr.c usched.o rmain.c -pthread -ormain.1t
	$(CC) -O3 -fno-stack-protector ll.c usched.o lmain.c -pthread -olmain
	$(CC) -O3 -fno-stack-protector -g -Wall usched.c umain.c -oumain
	$(CC) -O3 -fcoroutines -Wall -I/usr/local/include c++main.cpp -L/usr/local/lib -lcppcoro -lstdc++ -pthread -oc++main
	go build gmain.go
	cd cycle; cargo build -r

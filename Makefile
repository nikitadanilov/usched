all:
	cc -O3 -Wall pmain.c -pthread -opmain
	cc -fno-stack-protector -Wall usched.c -c -ousched.o
	cc -O3 -fno-stack-protector rr.c usched.o rmain.c -pthread -ormain
	cc -O3 -fno-stack-protector -g -Wall usched.c umain.c -oumain
	gcc -O3 -fcoroutines -Wall -I/usr/local/include c++main.cpp -L/usr/local/lib -lcppcoro -lstdc++ -pthread -oc++main
	go build gmain.go
	cd cycle; cargo build -r

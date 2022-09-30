all:
	cc -O3 -Wall pmain.c -pthread -opmain
	cc -O0 -fno-stack-protector -g -Wall usched.c rr.c rmain.c -pthread -ormain
	cc -O0 -fno-stack-protector -g -Wall usched.c umain.c -oumain
	go build gmain.go

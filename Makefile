all:
	cc -O3 -Wall pmain.c -pthread -opmain
	cc -fno-stack-protector -Wall usched.c rr.c rmain.c -pthread -ormain
	cc -O3 -fno-stack-protector -g -Wall usched.c umain.c -oumain
	go build gmain.go
	cd cycle; cargo build -r

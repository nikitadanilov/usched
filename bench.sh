#!/bin/bash

PROCS=16
for n in 2 8 ;do
    printf "%7s, %7s, %7s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s, %10s\n" \
	   "N" "R" "M" "POSIX" "GO" "GO1T" "RUST" "C++" "C++1T" "U" "U1K" "U10K" "U1T" "U1TS" "UL"
    for r in 2 4 8 50 100 500 1000 5000 10000 50000 100000 500000 1000000 ;do
	m=$((8000000 / $n / $r + 100))
	P=$(pmain $n $r $m     1 0 || echo 100000.0)
	G=$(gmain $n $r $m     0 0 || echo 100000.0)
	GO1T=$(gmain $n $r $m     0 1 || echo 100000.0)
	R=$(cycle/target/release/cycle $n $r $m || echo 1000000.0)
	U=$(rmain $n $r $m 0 $PROCS || echo 100000.0)
	U1K=$(rmain $n $r $m 1000 $PROCS || echo 100000.0)
	U10K=$(rmain $n $r $m 10000 $PROCS || echo 100000.0)
	U1T=$(rmain $n $r $m 0 1 || echo 100000.0)
	U1TS=$(rmain.1t $n $r $m 0 1 || echo 100000.0)
	UL=$(lmain $n $r $m 0 $PROCS || echo 100000.0)
	CPP=$(c++main $n $r $m 0 $PROCS || echo 1000000.0)
	CPP1T=$(c++main $n $r $m 0 1 || echo 1000000.0)
	printf "%7d, %7d, %7d, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f, %10.3f\n" \
	       $n $r $m $P $G $GO1T $R $CPP $CPP1T $U $U1K $U10K $U1T $U1TS $UL
    done
done

#!/bin/bash

for n in 8 ;do
    printf "%7s, %7s, %7s, %5s, %5s, %5s, %5s, %5s, %5s\n" "N" "R" "M" "POSIX" "GO" "RUST" "U" "U1K" "U10K"
    for r in 2 4 8 50 100 500 1000 5000 10000 50000 100000 500000 1000000 5000000 10000000 ;do
	m=$((8000000 / $n / $r + 100))
	P=$(pmain $n $r $m     1 0 || echo 100000.0)
	G=$(gmain $n $r $m     0 0 || echo 100000.0)
	R=$(cycle/target/release/cycle $n $r $m || echo 1000000.0)
	U=$(rmain $n $r $m 0 16 || echo 100000.0)
	U1k=$(rmain $n $r $m 1000 16 || echo 100000.0)
	U10k=$(rmain $n $r $m 10000 16 || echo 100000.0)
	printf "%7d, %7d, %7d, %5.3f, %5.3f, %5.3f, %5.3f, %5.3f, %5.3f\n" $n $r $m $P $G $R $U $U1k $U10k
    done
done

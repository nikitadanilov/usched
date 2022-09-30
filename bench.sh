#!/bin/bash

for n in 2 4 8 50 100 ;do
    for r in 2 4 8 50 100 1000 ;do
	echo -n "P "; pmain $n $r 100     0 0
	echo -n "G "; gmain $n $r 100     0 0
	echo -n "R "; rmain $n $r 100 10000 1
    done
done

package main

import (
	"os"
	"fmt"
	"strconv"
	"sync"
	"time"
)

func main() {
	var wg sync.WaitGroup
	n, _ := strconv.Atoi(os.Args[1]) /* Cycle length. */
	r, _ := strconv.Atoi(os.Args[2]) /* Number of cycles. */
	m, _ := strconv.Atoi(os.Args[3]) /* Number of rounds. */
	token := struct {}{}
	cycles := make([][]chan struct {}, r)
	for i, _ := range cycles {
		cycles[i] = make([]chan struct {}, n)
		for j, _ := range cycles[i] {
			cycles[i][j] = make(chan struct{})
		}
	}
	wg.Add(n * r)
	start := time.Now()
	for i, c := range cycles {
		for j, q := range c {
			go func(i int, j int, q chan struct{}) {
				left  := q
				right := cycles[i][(j + 1) % n]
				for d := 0; d < m; d++ {
					if j == d % n {
						right <- token
						<- left
					} else {
						<- left
						right <- token
					}
				}
				wg.Done()
			} (i, j, q)
		}
	}
	wg.Wait()
	fmt.Printf("%6d %6d %6d %v\n", n, r, m, time.Since(start).Seconds())
}

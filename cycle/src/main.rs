use futures::executor::block_on;
use futures::future::join_all;
use async_std::channel::{bounded, Sender, Receiver};
use std::env;
use std::time::Instant;

fn main() {
    let argv : Vec<String> = env::args().collect();
    let n    : usize       = argv[1].parse().unwrap(); /* Cycle length. */
    let r    : usize       = argv[2].parse().unwrap(); /* Number of CYCLES. */
    let m    : usize       = argv[3].parse().unwrap(); /* Number of rounds. */
    let tobe               = f(n, r, m);
    let start = Instant::now();
    block_on(tobe);
    println!("{:?}", start.elapsed().as_secs_f64());
}

struct R {
    rx : Receiver<()>,
    tx : Sender<()>
}

static mut CYCLES : Vec::<Vec::<R>> = Vec::<Vec::<R>>::new();

const DEPTH: usize = 1000;
async fn f(n : usize, r : usize, m : usize) {
    let _pad : [u8; DEPTH] = [50; DEPTH];
    for _ in 0 .. r {
        let mut c = Vec::<R>::new();
        for _ in 0 .. n {
            let (tx, rx) = bounded::<()>(10);
            c.push(R { tx : tx.clone(), rx : rx.clone() });
        }
        unsafe { CYCLES.push(c); }
    }
    join_all((0 .. r)
             .flat_map(|i| (0 .. n)
                       .map(|j| (i, j))
                       .collect::<Vec<_>>())
             .map(|(i, j)| lop(n, m, i, j))
             .collect::<Vec<_>>())
        .await;
}

async fn lop(n : usize, m : usize, cnr : usize, idx : usize) {
    unsafe {
	let left  = &CYCLES[cnr][idx];
        let right = &CYCLES[cnr][(idx + 1) % n];
        for round in 0 .. m {
            if idx == round % n {
                right.tx.send(()).await.expect("send failed");
                left.rx.recv().await.expect("recv failed");
            } else {
                left.rx.recv().await.expect("recv failed");
                right.tx.send(()).await.expect("send failed");
            }
        }
    }
}
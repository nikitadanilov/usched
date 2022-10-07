import sys
import matplotlib as mpl
import matplotlib.pyplot as plt
import numpy as np
import csv
from collections import defaultdict

def drill():
    return defaultdict(drill)

def processinput(files):
    d = drill()
    processone(sys.stdin, d)
    draw(d)

def processone(f, d):
    data = csv.reader(f)
    for row in data:
        r = [x.strip() for x in row]
        if r[0] == 'N':
            header = r
        else:
            r = [float(x) for x in r]
            dd = d[r[0]][r[1]][r[2]];
            ops = r[0]*r[1]*r[2]
            for i in range(3, len(r)):
                if len(dd[header[i]]) == 0:
                    dd[header[i]] = []
                dd[header[i]].append(ops/r[i])

def draw(d):
    fig, ax = plt.subplots()
    ax.set_xscale('log')
    f = drill()
    for n in d:
        for r in d[n]:
            for m in d[n][r]:
                for k in d[n][r][m]:
                    v = d[n][r][m][k]
                    avg = sum(v)/len(v)
                    f[k][n*r] = [max(0, avg - min(v)), avg, max(0, max(v) - avg)]
    for k in f:
        F = f[k]
        plt.errorbar(F.keys(), [F[t][1] for t in F],
                     yerr = [[F[t][0] for t in F],
                             [F[t][2] for t in F]], label=k)
    plt.legend(loc='lower right')
    plt.savefig(sys.stdout.buffer, format='svg')
    plt.show()
    
if __name__ == '__main__':
    processinput(sys.argv[1:])

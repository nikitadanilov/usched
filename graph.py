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
    draw(d, files[0])

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

def draw(d, out):
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
    first = True
    for k in f:
        F = f[k]
        if first:
            print("| | {} |".format("|".join(["{:.0f}".format(t) for t in F])))
            print("|---|{}|".format("|".join(["---" for t in F])))
            first=False
        plt.errorbar(F.keys(), [F[t][1] for t in F],
                     yerr = [[F[t][0] for t in F],
                             [F[t][2] for t in F]], label=k)
        print("| {} | {} |".format(k, "|".join(["{:.2f}".format(F[t][1]/1000000) for t in F])))
    plt.legend(loc='lower right')
    plt.savefig(out, format='svg')
    plt.show()
    
if __name__ == '__main__':
    processinput(sys.argv[1:])

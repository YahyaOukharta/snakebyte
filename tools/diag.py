#!/usr/bin/env python3
"""Arena games by phase (more than half of the apples left / down to 7 / the last 6), us and them, top two and rest.

  diag.py
Per side: share of snake-turns without a reachable apple, mean reach distance, head row, length
(mean, longest, shortest), dead snakes per position. Per apple eaten: whether it was in the top
three rows, and who was nearer to it 3 and 8 turns before (races). SUBS lists the submissions read.
"""
import json,subprocess,collections,statistics,sys
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)
T='arena/traces'; rows=[json.loads(l) for l in open(T+'/index.jsonl')]
SUBS={41758237,41760913,41752683,41752283,41761465}
def load(r):
    out=subprocess.check_output(['build/referee','--trace',f"{T}/{r['game_id']}.trace"]).decode().split('\n')
    seq=[]
    for line in out:
        f=line.split('|')
        if len(f)<6: continue
        v=list(map(int,f[4].split())); n=len(v)//7
        sn=[v[7*i:7*i+7] for i in range(n)]
        eat=[(int(e.split(':')[0]),)+tuple(map(int,e.split(':')[1].split(','))) for e in f[5].split() if not e.startswith('!')]
        hit=[e for e in f[5].split() if e.startswith('!')]
        seq.append(dict(t=int(f[0]),len=tuple(map(int,f[1].split())),ap=int(f[3]),sn=sn,eat=eat,hit=hit))
    return seq
if __name__=='__main__':
    A=collections.defaultdict(collections.Counter)
    for r in rows:
        if r['submission'] not in SUBS or r['mismatch']: continue
        grp='top2' if r['opponent'] in ('sZoom','_H3R0_') else 'rest'
        seq=load(r)
        if not seq: continue
        n=len(seq[0]['sn']); A0=seq[0]['ap']
        for k,p in enumerate(seq):
            ph='1early' if p['ap']>A0*0.5 else '2mid' if p['ap']>6 else '3end'
            for side,(lo,hi) in (('us',(0,n//2)),('them',(n//2,n))):
                c=A[(grp,ph,side)]
                for i in range(lo,hi):
                    s=p['sn'][i]
                    if not s[0]: c['dead']+=1; continue
                    c['st']+=1; c['idle']+=s[6]==255; c['y']+=s[3]; c['len']+=s[1]
                    if s[6]<255: c['dist']+=s[6]; c['dn']+=1
                    c['short']+=s[1]<=4; c['long']+=s[1]>=10
                ls=sorted(s[1] for s in p['sn'][lo:hi] if s[0])
                if ls: c['maxlen']+=ls[-1]; c['minlen']+=ls[0]; c['pos']+=1
            # races: apple eaten at next position k+1? eat events listed on the line where they happened
            for (i,x,y) in p['eat']:
                side='us' if i<n//2 else 'them'
                for back in (3,8):
                    if k-back<0: continue
                    q=seq[k-back]
                    du=min([s[6]+ (0 if (s[4],s[5])==(x,y) else 50) for s in q['sn'][:n//2] if s[0]] or [255])
                    dt=min([s[6]+ (0 if (s[4],s[5])==(x,y) else 50) for s in q['sn'][n//2:] if s[0]] or [255])
                    # only when this apple was the nearest target of somebody on both sides
                    c=A[(grp,ph,'race%d'%back)]
                    if du<50 and dt<50:
                        c['both']+=1
                        near='us' if du<dt else 'them' if dt<du else 'tie'
                        c[f'{near}_near_{side}_ate']+=1
                    elif du<50: c[f'onlyus_{side}_ate']+=1
                    elif dt<50: c[f'onlythem_{side}_ate']+=1
                    else: c[f'nobody_{side}_ate']+=1
                A[(grp,ph,'eat')][side+('_top' if y<=2 else '_low')]+=1
    for k in sorted(A):
        c=A[k]
        if k[2] in ('us','them'):
            print(f"{k[0]:5}{k[1]:7}{k[2]:5} idle {100*c['idle']/c['st']:5.1f}%  reach dist {c['dist']/max(1,c['dn']):5.2f}  head y {c['y']/c['st']:5.2f}  len {c['len']/c['st']:5.2f}  max {c['maxlen']/c['pos']:5.2f} min {c['minlen']/c['pos']:5.2f}  short<=4 {100*c['short']/c['st']:4.1f}% long>=10 {100*c['long']/c['st']:4.1f}%  dead/turn {c['dead']/c['pos']:.2f}")
        else: print(k, dict(c))

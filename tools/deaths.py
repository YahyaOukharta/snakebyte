#!/usr/bin/env python3
"""Head-on collisions between our snakes and theirs by length (3 or longer), and whole snakes lost by cause.

  deaths.py        the submissions listed in diag.py, top two and rest
"""
import json,subprocess,collections,sys
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from diag import load,rows,SUBS
C=collections.defaultdict(collections.Counter); G=collections.Counter()
for r in rows:
    if r['submission'] not in SUBS or r['mismatch']: continue
    grp='top2' if r['opponent'] in ('sZoom','_H3R0_') else 'rest'
    seq=load(r)
    if not seq: continue
    n=len(seq[0]['sn']); G[grp]+=1
    for p in seq:
        ev={}
        for e in p['hit']:
            i,c,o,ln=e[1:].split(':'); ev[int(i)]=(c,int(o),int(ln))
        for i,(c,o,ln) in ev.items():
            if c!='H' or i>=n//2 or o<n//2: continue      # our snake i vs enemy o
            lo=ev.get(o,(0,0,0))[2]
            a='our3' if ln<=3 else 'our4+'; b='their3' if lo<=3 else 'their4+'
            C[grp][a+'_vs_'+b]+=1
            C[grp]['our_len_sum']+=ln; C[grp]['their_len_sum']+=lo; C[grp]['n']+=1
        # deaths by cause
        for i,(c,o,ln) in ev.items():
            if ln<=3 or c=='F':
                side='us' if i<n//2 else 'them'
                C[grp][f'death_{side}_{c}']+=1
for g in C:
    print(g,G[g],'games'); 
    for k,v in sorted(C[g].items()): print(f"   {k:22} {v:5}  per game {v/G[g]:.2f}")

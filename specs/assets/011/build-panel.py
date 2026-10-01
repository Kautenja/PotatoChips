#!/usr/bin/env python3
"""Original Voice 2151 panel compositions; lettering uses spec 012's pinned font."""
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
labels=['VOICE 2151','YAMAHA OPM','VOICE','OPERATOR 1','OPERATOR 2','OPERATOR 3','OPERATOR 4',
'TUNE','ALG','FB','LEVEL','LFO','AMD','PMD','AMS','PMS','WAVE','NOISE','RATE','ROUTE',
'AR','D1','SR','SL','RR','VOL','MUL','KS','DT1','DT2','AM','V/OCT','GATE','TRIG','L','R',
'1','2','3','4','OUT','ARHYTHMETIC UNITS']
source=HERE/'lettering.json'
source.write_text(json.dumps([dict(key=s,name=s,sub=s) for s in labels],indent=2)+'\n')
subprocess.run(['swift','-module-cache-path','/tmp/potatochips-swift-cache',str(ROOT/'specs/assets/012/outline-titles.swift'),str(ROOT/'specs/assets/012/fonts/LiberationSans-Bold.ttf'),str(source),str(HERE/'lettering-outlines.json')],check=True)
letters=json.loads((HERE/'lettering-outlines.json').read_text())['modules']
def text(s,x,y,size,color):
    o=letters[s]['title']; scale=size/100
    return f'<path fill="{color}" transform="translate({x-o["width"]*scale/2:.3f},{y-o["height"]*scale:.3f}) scale({scale})" d="{o["d"]}"/>'
for dark in [False,True]:
    bg,ink,accent,group=('#111e28','#e8eef0','#63ddcd','#203640') if dark else ('#eee9df','#172e38','#08766d','#d9e4df')
    svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="900" height="380" viewBox="0 0 900 380">',f'<rect width="900" height="380" fill="{bg}"/>']
    for col in range(5):
        left=col*180
        svg.append(f'<rect x="{left+5}" y="32" width="170" height="325" rx="5" fill="{group}"/>')
        svg.append(text('VOICE' if col==0 else f'OPERATOR {col}',left+90,43,11,accent))
    svg += [text('VOICE 2151',450,23,23,ink),text('YAMAHA OPM',95,22,10,accent),text('ARHYTHMETIC UNITS',450,374,10,ink)]
    for i,s in enumerate(['TUNE','ALG','FB','LEVEL','LFO','AMD','PMD','AMS','PMS','WAVE','NOISE','RATE','ROUTE']):
        svg.append(text(s,32+i%3*55,89+i//3*46,9,ink))
    for i,s in enumerate(['V/OCT','GATE','TRIG','ALG','FB','LFO','AMD','PMD','RATE','LEVEL']):
        svg.append(text(s,24+i%5*33,283+i//5*42,7.5,ink))
    for op in range(4):
        left=180+op*180
        for i,s in enumerate(['AR','D1','SR','SL','RR','VOL','MUL','KS','DT1','DT2','AM']):
            svg.append(text(s,left+32+i%3*55,89+i//3*55,10,ink))
        for i,s in enumerate(['AR','D1','SR','SL','RR','VOL','MUL']):
            svg.append(text(s,left+24+i%4*43,283+i//4*42,9,ink))
    svg += [text('L',839,247,9,ink),text('R',875,247,9,ink),'</svg>']
    (ROOT/'res'/('YM2151-dark.svg' if dark else 'YM2151.svg')).write_text('\n'.join(svg)+'\n')
# Original algorithm graphs in natural operator order. Output carriers are explicit.
links=[[(1,2),(2,3),(3,4)],[(1,3),(2,3),(3,4)],[(2,3),(1,4),(3,4)],[(1,2),(2,4),(3,4)],[(1,2),(3,4)],[(1,2),(1,3),(1,4)],[(1,2)],[]]
carriers=[[4],[4],[4],[4],[2,4],[2,3,4],[2,3,4],[1,2,3,4]]
folder=ROOT/'res/YM2151_algorithms';folder.mkdir(exist_ok=True)
for alg in range(8):
    pos={1:(12,12),2:(36,12),3:(60,12),4:(84,12)}
    svg=['<svg xmlns="http://www.w3.org/2000/svg" width="100" height="40" viewBox="0 0 100 48">']
    for a,b in links[alg]:
        x,y=pos[a];u,v=pos[b]
        yy=25+(b-a)*3
        if b-a==1:
            svg.append(f'<path d="M{x+7},{y} H{u-7} M{u-10},{y-2} L{u-7},{y} L{u-10},{y+2}" fill="none" stroke="#63ddcd"/>')
        else:
            svg.append(f'<path d="M{x},{y+7} V{yy} H{u} V{v+7} M{u-2},{v+10} L{u},{v+7} L{u+2},{v+10}" fill="none" stroke="#63ddcd"/>')
    for op,(x,y) in pos.items():
        svg.append(f'<rect x="{x-7}" y="{y-7}" width="14" height="14" rx="2" fill="#294652"/>')
        svg.append(text(str(op),x,y+3,10,'#ffffff'))
        if op in carriers[alg]:svg.append(f'<path d="M{x+7},{y} H{x+10} V40 H50" fill="none" stroke="#ffcd74"/>')
    svg.append(text('OUT',50,47,7,'#ffcd74'))
    svg.append('</svg>');(folder/f'{alg}.svg').write_text('\n'.join(svg)+'\n')

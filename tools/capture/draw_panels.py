"""Export static TeX symbols from native geometry and reviewed panel regions.

Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import json
from pathlib import Path
from run import ROOT, selected
from export_screenshots import prepare


def escape(value):
    for source, target in [('&', r'\&'), ('%', r'\%'), ('_', r'\_'), ('#', r'\#')]:
        value = value.replace(source, target)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture_dir', type=Path)
    parser.add_argument('--module')
    args = parser.parse_args()
    prepare(args.capture_dir, args.module)
    regions = json.loads(Path(__file__).with_name('regions.json').read_text())
    for row in selected(args.module):
        name = row['manual']
        if not name:
            continue
        controls = json.loads((args.capture_dir / f'{name}.json').read_text())['controls']
        text = ['% Production widget geometry; regenerate with tools/capture/draw_panels.py.',
                '% See LICENSING.md for visual-asset terms.',
                r'\begingroup',
                r'\pgfmathsetlengthmacro{\panelunit}{min(\textwidth/' +
                str(row['width'] + 70) + r',.60*\textheight/420)}',
                r'\begin{tikzpicture}[x=\panelunit,y=-\panelunit]',
                r'\draw[fill=white,draw=manualMuted,line width=.6pt] (0,0) rectangle (' +
                str(row['width']) + ',380);']
        text.append(r'\node[font=\sffamily\bfseries\scriptsize,text=manualInk] at (' +
                    str(row['width'] / 2) + r',10) {\MakeUppercase{\manualname}};')
        for i, region in enumerate(regions[name], 1):
            text.append(r'\panelregion' + ''.join('{' + str(v) + '}' for v in [i] + region['box']))
        for control in controls:
            text.append(('% ' + control['kind'] + ' ' + str(control['id']) + ': ' + control['label']).rstrip())
            text.append('\\panel' + control['kind'] + ''.join(
                '{' + f'{control[key]:.4f}' + '}' for key in ('x', 'y', 'width', 'height')))
        for i, region in enumerate(regions[name], 1):
            text.append(r'\panelmarker' + ''.join('{' + str(v) + '}' for v in [i] + region['marker']))
        text += [r'\end{tikzpicture}', r'\endgroup', r'\par\smallskip',
                 r'\begin{minipage}{.95\textwidth}\small',
                 r'\begin{tabularx}{\textwidth}{@{}rX@{}}']
        for i, region in enumerate(regions[name], 1):
            text.append(r'\panelkey{' + str(i) + '}{' + escape(region['label']) + '}')
        text += [r'\end{tabularx}', r'\end{minipage}', '']
        directory = ROOT / 'manual' / name / 'figures'
        directory.mkdir(exist_ok=True)
        (directory / 'panel-layout.tex').write_text('\n'.join(text))


if __name__ == '__main__':
    main()

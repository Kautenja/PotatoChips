#!/usr/bin/env python3
"""Rebuild spec 012's frozen panel proposals; never modify runtime res/."""
# Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
import copy
import argparse
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)
ET.register_namespace('xlink', 'http://www.w3.org/1999/xlink')
ROWS = json.loads((ROOT / 'directions.json').read_text())
LAYOUT = json.loads((ROOT / 'layout.json').read_text())['modules']
OUTLINES = json.loads((ROOT / 'title-outlines.json').read_text())['modules']
TITLE_IDS = {
    'Blocks': 'Blocks', 'MiniBoss': 'MINIBOSS',
    'NameCorpOctalWaveGenerator': 'NameCorpOctalWaveGenerator',
    'BossFight': 'BOSSFIGHT', '2612_Blank1': 'BOSSFIGHTEnvelope',
    'InfiniteStairs': '\u221eStairs', 'Jairasullator': 'JAIRASULLATOR',
    'Pulses': 'PULSES', 'PalletTownWavesSystem': 'PalletTownWavesSystem',
    'PotKeys': 'PotKeys', 'SuperADSR': 'SUPERADSR', 'SuperEcho': 'SuperEcho',
    'SuperVCA': 'SuperVCA', 'MegaTone': 'MEGATONE', 'StepSaw': 'StepSaw',
}


def blend(a, b, amount):
    values = [round(int(a[i:i + 2], 16) * (1 - amount)
                    + int(b[i:i + 2], 16) * amount) for i in (1, 3, 5)]
    return '#' + ''.join(f'{v:02X}' for v in values)


def recolor(root, row, theme):
    bg, surface, ink, accent = row['palette'][0:4] if theme == 'light' else row['palette'][4:8]
    chip = row['slug'] == 'Sony_S_SMP_Blank1'
    width = next(r['width'] for r in LAYOUT if r['slug'] == row['slug'])

    def walk(e, parents, in_output=False):
        tag = e.tag.split('}')[-1]
        ident = e.get('id', '')
        in_output = in_output or ident in ('Output', 'OUT', 'Outputs', 'output')
        full_background = ((tag == 'rect' and float(e.get('width', 0)) == width
                            and float(e.get('height', 0)) == 380)
                           or ident in ('Background', 'background-color'))
        for attr in ('fill', 'stroke'):
            value = e.get(attr, '').upper()
            if not value or value == 'NONE' or value.startswith('URL('):
                continue
            if chip:
                mapping = {'#E6E6E6': bg, '#244D28': accent if theme == 'light' else surface}
                color = mapping.get(value, e.get(attr))
                if ident == 'ArhythmeticUnits':
                    color = ink
            elif full_background:
                color = bg
            elif in_output and attr == 'fill' and value in ('#FFFFFF', 'WHITE', '#E6E6E6'):
                color = '#F6F3ED'
            elif tag == 'rect' and value in ('#000000', '#1D1D1D'):
                color = '#191B1E'
            elif value in ('#000000', '#1D1D1D', '#343633', '#FFFFFF', 'WHITE'):
                color = ink
            elif value in ('#E6E6E6', '#E2E2E2'):
                color = blend(surface, ink, .48) if ident.startswith('Line') else ink
            elif value in ('#898989', '#A4A4A4', '#9E9E9E'):
                color = surface
            elif value in ('#929292', '#969696', '#696969'):
                color = blend(bg, ink, .50)
            elif value == '#FF5050':
                # Preserve stereo right-channel semantics independently of branding.
                color = '#B73831' if theme == 'light' else '#FF8B7A'
            elif value == '#4D345C':
                color = bg
            elif value == '#31213B':
                color = blend(bg, accent, .22)
            elif value == '#6B4B7D' or ident == 'voice-lanes':
                color = surface
            elif value in ('#EA4F4F', '#0B71E9', '#E4000F', '#52C22F', '#C41C73', '#6B17D5'):
                color = surface
            else:
                color = accent
            if ident == 'ArhythmeticUnits':
                color = ink
            if row['slug'] == '2612' and value == '#0D3B99':
                color = surface
            if row['slug'] == '2612_Blank1' and tag == 'polyline':
                color = accent
            e.set(attr, color)
        if ident == 'ArhythmeticUnits':
            e.set('fill', ink)
        if in_output and tag == 'rect' and not chip:
            e.set('fill', '#191B1E')
        if row['slug'] == '2612_Blank1' and tag == 'polyline':
            e.set('stroke', accent)
        for child in e:
            walk(child, parents + [ident], in_output)
    walk(root, [])
    # The new title always uses the accent; functional labels use ink.
    return ink, accent


def title(root, row, width, accent, ink):
    ident = TITLE_IDS.get(row['key'])
    removed = 0
    if ident:
        for parent in root.iter():
            for child in list(parent):
                if child.get('id') == ident:
                    # Duplicate IDs also name the outer panel group. Only remove
                    # glyph paths or groups containing glyph paths, never the panel.
                    is_path = child.tag.endswith('path')
                    glyph_group = child.tag.endswith('g') and all(c.tag.endswith('path') for c in child)
                    if is_path or glyph_group:
                        parent.remove(child)
                        removed += 1
        assert removed, (row['key'], 'old title not found')
    g = ET.SubElement(root, f'{{{NS}}}g', {'id': 'rebrand-title', 'fill': accent,
                                        'fill-rule': 'nonzero'})
    ET.SubElement(g, f'{{{NS}}}title').text = row['name']
    left, right = (32, width - 32) if width >= 150 else (32, width - 5)
    if row['slug'] == '2612':
        left, right = 35, 125
    # Keep title inside the original header. No controls move to make room.
    max_height = 11 if width >= 150 else 9
    top = 4
    glyph = OUTLINES[row['key']]['title']
    scale = min((right - left) / glyph['width'], max_height / glyph['height'])
    x = (left + right - glyph['width'] * scale) / 2
    path = ET.SubElement(g, f'{{{NS}}}path', {'d': glyph['d'],
                         'transform': f'translate({x:.5f} {top}) scale({scale:.8f})'})
    bounds = [round(x, 5), top, round(glyph['width'] * scale, 5), round(glyph['height'] * scale, 5)]
    subtitle = None
    # Dense headers have no second line; hardware remains in name/metadata.
    if row['slug'] not in ('2612', 'SuperVCA', '106', '2612_Blank1', 'Sony_S_SMP_Blank1'):
        sub = OUTLINES[row['key']]['subtitle']
        sub_scale = min((right - left) / sub['width'], 5 / sub['height'])
        sx = (left + right - sub['width'] * sub_scale) / 2
        ET.SubElement(g, f'{{{NS}}}path', {'d': sub['d'], 'fill': ink,
                      'transform': f'translate({sx:.5f} 18) scale({sub_scale:.8f})'})
        subtitle = [round(sx, 5), 18, round(sub['width'] * sub_scale, 5), 5]
    return {'title_bounds': bounds, 'subtitle_bounds': subtitle, 'old_title_groups_removed': removed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Fail on stale exports without writing')
    parser.add_argument('--installed', action='store_true', help='Also check runtime panels and names')
    args = parser.parse_args()
    assert hashlib.sha256((ROOT / 'fonts/LiberationSans-Bold.ttf').read_bytes()).hexdigest() == \
        '361c61b82d575c5c35fd9157fda8b0194bcfcd0d88ea8521a4fb5dd53d33dddc', 'Wrong title font'
    repo = ROOT.parents[2]
    active = {m['slug']: m for m in json.loads((repo / 'plugin.json').read_text())['modules']
              if not m.get('disabled')}
    assert len(ROWS) == len(active) == len({r['slug'] for r in ROWS})
    assert {r['slug'] for r in ROWS} == set(active), 'Artwork inventory differs from active models'
    report = []
    for row in ROWS:
        assert OUTLINES[row['key']]['title']['text'] == row['name'].upper(), 'Stale title outlines'
        if args.installed:
            assert active[row['slug']]['name'] == row['name'], 'Wrong manifest name'
        layout = next(r for r in LAYOUT if r['slug'] == row['slug'])
        if args.installed and layout['manual']:
            manual = (repo / 'manual' / layout['manual'] / 'manual.tex').read_text()
            for macro in ('manualname', 'manualshortname'):
                assert '\\newcommand{\\' + macro + '}{' + row['name'] + '}' in manual, 'Wrong manual name'
        source_path = ROOT / 'sources' / layout['panel']
        assert hashlib.sha256(source_path.read_bytes()).hexdigest() == layout['source_sha256']
        source = ET.parse(source_path).getroot()
        for theme in ('light', 'dark'):
            root = copy.deepcopy(source)
            ink, accent = recolor(root, row, theme)
            details = title(root, row, layout['width'], accent, ink)
            for bounds in (details['title_bounds'], details['subtitle_bounds']):
                if bounds:
                    x, y, w, h = bounds
                    assert x >= 0 and y >= 0 and x + w <= layout['width'] and y + h <= 24
            root.find(f'{{{NS}}}title').text = row['name'] + ' - ' + theme
            filename = layout['panel'] if theme == 'light' else layout['panel'].replace('.svg', '-dark.svg')
            data = ET.tostring(root, encoding='utf-8', xml_declaration=True)
            data = b'\n'.join(line.rstrip() for line in data.splitlines()) + b'\n'
            if args.installed:
                assert (repo / 'res' / filename).read_bytes() == data, f'Stale runtime panel: {filename}'
            destination = ROOT / 'panels' / filename
            if args.check:
                assert destination.read_bytes() == data, f'Stale artwork: {filename}'
            else:
                destination.write_bytes(data)
            report.append(dict(slug=row['slug'], theme=theme, file='panels/' + filename,
                               width=layout['width'], height=380, **details))
    data = json.dumps(report, indent=2) + '\n'
    if args.check:
        assert (ROOT / 'artwork-index.json').read_text() == data, 'Stale artwork index'
    else:
        (ROOT / 'artwork-index.json').write_text(data)
    print(f'{"Verified" if args.check else "Generated"} {len(report)} exact-size SVG panel proposals.')


if __name__ == '__main__':
    main()

// Copyright 2026 Arhythmetic Units. SPDX-License-Identifier: GPL-3.0-or-later
import Foundation
import CoreText
import CoreGraphics
let args = CommandLine.arguments
let provider = CGDataProvider(filename: args[1])!
let cgfont = CGFont(provider)!
let font = CTFontCreateWithGraphicsFont(cgfont, 100, nil, nil)
let data = try Data(contentsOf: URL(fileURLWithPath: args[2]))
let rows = try JSONSerialization.jsonObject(with: data) as! [[String: Any]]
func outline(_ text: String) -> [String: Any] {
    let attrs: [NSAttributedString.Key: Any] = [NSAttributedString.Key(kCTFontAttributeName as String): font]
    let line = CTLineCreateWithAttributedString(NSAttributedString(string: text, attributes: attrs))
    let path = CGMutablePath()
    for obj in CTLineGetGlyphRuns(line) as! [CTRun] {
        let n = CTRunGetGlyphCount(obj)
        var glyphs = [CGGlyph](repeating: 0, count: n)
        var pos = [CGPoint](repeating: .zero, count: n)
        CTRunGetGlyphs(obj, CFRangeMake(0, 0), &glyphs)
        CTRunGetPositions(obj, CFRangeMake(0, 0), &pos)
        for i in 0..<n {
            if let p = CTFontCreatePathForGlyph(font, glyphs[i], nil) {
                path.addPath(p, transform: CGAffineTransform(translationX: pos[i].x, y: pos[i].y))
            }
        }
    }
    let b = path.boundingBoxOfPath
    var d = ""
    func pt(_ p: CGPoint) -> String { String(format: "%.5f,%.5f", Double(p.x - b.minX), Double(b.maxY - p.y)) }
    path.applyWithBlock { ptr in
        let e = ptr.pointee
        switch e.type {
        case .moveToPoint: d += "M" + pt(e.points[0])
        case .addLineToPoint: d += "L" + pt(e.points[0])
        case .addQuadCurveToPoint: d += "Q" + pt(e.points[0]) + " " + pt(e.points[1])
        case .addCurveToPoint: d += "C" + pt(e.points[0]) + " " + pt(e.points[1]) + " " + pt(e.points[2])
        case .closeSubpath: d += "Z"
        @unknown default: break
        }
    }
    return ["text":text,"d":d,"width":b.width,"height":b.height]
}
var result: [String: Any] = [:]
for row in rows {
    let key = row["key"] as! String
    result[key] = ["title":outline((row["name"] as! String).uppercased()), "subtitle":outline(row["sub"] as! String)]
}
let out: [String: Any] = ["font": CTFontCopyPostScriptName(font) as String, "version": CTFontCopyName(font,kCTFontVersionNameKey) as Any, "font_size":100,"modules":result]
let bytes = try JSONSerialization.data(withJSONObject: out, options:[.prettyPrinted,.sortedKeys])
try bytes.write(to: URL(fileURLWithPath: args[3]))

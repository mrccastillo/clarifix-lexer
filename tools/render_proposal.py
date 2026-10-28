"""Render the revised proposal PDF. Optional dependencies: reportlab, markdown.

Run from any directory: python tools/render_proposal.py
The lexer and its tests do not need these dependencies.
"""
from html import escape
from pathlib import Path
import re
import xml.etree.ElementTree as ET

import markdown
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, Preformatted,
)

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'docs/ClariFix-Revised-Proposal.md'
DESTINATION = SOURCE.with_suffix('.pdf')


def main():
    font_dir = Path('C:/Windows/Fonts')
    if (font_dir / 'calibri.ttf').is_file():
        pdfmetrics.registerFont(TTFont('Body', str(font_dir / 'calibri.ttf')))
        pdfmetrics.registerFont(TTFont('BodyBold', str(font_dir / 'calibrib.ttf')))
        pdfmetrics.registerFont(TTFont('Code', str(font_dir / 'consola.ttf')))
        pdfmetrics.registerFontFamily('Body', normal='Body', bold='BodyBold', italic='Body', boldItalic='BodyBold')
    else:
        pdfmetrics.registerFontFamily('Body', normal='Helvetica', bold='Helvetica-Bold', italic='Helvetica-Oblique', boldItalic='Helvetica-BoldOblique')
    body_font = 'Body' if (font_dir / 'calibri.ttf').is_file() else 'Helvetica'
    bold_font = 'BodyBold' if body_font == 'Body' else 'Helvetica-Bold'
    code_font = 'Code' if body_font == 'Body' else 'Courier'
    styles = getSampleStyleSheet()
    for style in styles.byName.values():
        style.fontName = body_font
    styles['BodyText'].fontSize = 10
    styles['BodyText'].leading = 14
    styles['BodyText'].spaceAfter = 7
    for heading in ('Title', 'Heading1', 'Heading2'):
        styles[heading].fontName = bold_font
        styles[heading].textColor = colors.HexColor('#234d40')
        styles[heading].keepWithNext = True
    styles['Title'].alignment = TA_LEFT
    styles.add(ParagraphStyle('Cell', parent=styles['BodyText'], fontSize=8.5, leading=11, spaceAfter=0))
    styles.add(ParagraphStyle('CodeBlock', fontName=code_font, fontSize=8, leading=11, spaceAfter=10))
    width = A4[0] - 36 * mm

    def inline(element):
        text = escape(element.text or '')
        for child in element:
            value = inline(child)
            if child.tag == 'code':
                value = f'<font name="{code_font}">{value}</font>'
            elif child.tag in ('strong', 'b'):
                value = f'<b>{value}</b>'
            elif child.tag in ('em', 'i'):
                value = f'<i>{value}</i>'
            text += value + escape(child.tail or '')
        return text

    html = markdown.markdown(SOURCE.read_text(encoding='utf-8'), extensions=['tables', 'fenced_code'])
    root = ET.fromstring('<document>' + html + '</document>')
    story = []
    for element in root:
        if element.tag in ('h1', 'h2', 'h3'):
            style = {'h1': 'Title', 'h2': 'Heading1', 'h3': 'Heading2'}[element.tag]
            story.append(Paragraph(inline(element), styles[style]))
        elif element.tag == 'p':
            story.append(Paragraph(inline(element), styles['BodyText']))
        elif element.tag == 'pre':
            text = ''.join(element.itertext()).rstrip('\n')
            for line in text.splitlines():
                if pdfmetrics.stringWidth(line, code_font, 8) > width - 12:
                    raise ValueError('Code line exceeds printable width: ' + line)
            story.append(Preformatted(text, styles['CodeBlock']))
        elif element.tag in ('ul', 'ol'):
            for i, item in enumerate(element.findall('li'), 1):
                prefix = f'{i}.' if element.tag == 'ol' else '-'
                story.append(Paragraph(prefix + ' ' + inline(item), styles['BodyText']))
        elif element.tag == 'table':
            rows = element.findall('./thead/tr') + element.findall('./tbody/tr')
            cells = [[Paragraph(inline(cell), styles['Cell']) for cell in row] for row in rows]
            count = len(cells[0])
            fractions = [0.23, 0.77] if count == 2 else [0.22, 0.13, 0.65]
            if count == 6:
                fractions = [0.06, 0.14, 0.25, 0.15, 0.12, 0.28]
            # Syntax/example and kind/spellings/meaning tables need balanced columns.
            headers = [''.join(cell.itertext()) for cell in rows[0]]
            if count == 3 and headers[1] != 'Count':
                fractions = [0.18, 0.37, 0.45]
            table = Table(cells, colWidths=[width * x for x in fractions], repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e7eee9')),
                ('LINEBELOW', (0, 0), (-1, 0), 0.6, colors.HexColor('#547461')),
                ('LINEBELOW', (0, 1), (-1, -1), 0.3, colors.HexColor('#d6ded9')),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                ('LEFTPADDING', (0, 0), (-1, -1), 6),
                ('RIGHTPADDING', (0, 0), (-1, -1), 6),
                ('TOPPADDING', (0, 0), (-1, -1), 6),
                ('BOTTOMPADDING', (0, 0), (-1, -1), 6),
            ]))
            story.extend([table, Spacer(1, 10)])
        else:
            raise ValueError('Unsupported Markdown block: ' + element.tag)

    def footer(canvas, doc):
        canvas.saveState()
        canvas.setFont(body_font, 8)
        canvas.setFillColor(colors.HexColor('#55675f'))
        canvas.drawString(18 * mm, 12 * mm, 'ClariFix | Group 4 | Updated-paper alignment addendum')
        canvas.drawRightString(A4[0] - 18 * mm, 12 * mm, str(doc.page))
        canvas.restoreState()

    document = SimpleDocTemplate(str(DESTINATION), pagesize=A4, leftMargin=18 * mm,
                                 rightMargin=18 * mm, topMargin=18 * mm, bottomMargin=20 * mm,
                                 title='ClariFix: updated-paper alignment addendum', author='Group 4')
    document.build(story, onFirstPage=footer, onLaterPages=footer)
    print(DESTINATION)


if __name__ == '__main__':
    main()

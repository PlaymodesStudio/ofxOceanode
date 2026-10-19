#!/usr/bin/env python3
"""Build the beginner manual PDF and explanatory SVG figures from its Markdown.
Requires reportlab. Run with the bundled PDF Python runtime, or another Python
installation containing reportlab. Text lives in docs/oceanode-user-manual.md.
"""
from pathlib import Path
import math
import re
from xml.sax.saxutils import escape
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.pagesizes import A4
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph,
    Spacer, PageBreak, Table, TableStyle, KeepTogether)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.graphics.shapes import Drawing, Rect, Line, Circle, String, PolyLine
from reportlab.graphics import renderSVG

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'output/pdf/oceanode-user-manual.pdf'
ASSETS = ROOT / 'docs/assets/oceanode-manual'
INK = colors.HexColor('#163339')
TEAL = colors.HexColor('#087F86')
ORANGE = colors.HexColor('#D86A36')
MUTED = colors.HexColor('#53666B')
PALE = colors.HexColor('#EAF3F3')
WIDTH = A4[0] - 112


def fonts():
    folder = Path('/System/Library/Fonts/Supplemental')
    candidates = [
        ('Body', folder/'Arial.ttf'), ('Bold', folder/'Arial Bold.ttf'),
        ('Italic', folder/'Arial Italic.ttf'),
    ]
    if all(p.exists() for _, p in candidates):
        for name, path in candidates: pdfmetrics.registerFont(TTFont(name, str(path)))
        pdfmetrics.registerFontFamily('Body', normal='Body', bold='Bold', italic='Italic', boldItalic='Bold')
    else:
        for name, standard in [('Body','Helvetica'), ('Bold','Helvetica-Bold'), ('Italic','Helvetica-Oblique')]:
            pdfmetrics.registerFont(pdfmetrics.Font(name, standard, 'WinAnsiEncoding'))
        pdfmetrics.registerFontFamily('Body', normal='Body', bold='Bold', italic='Italic', boldItalic='Bold')


def label(d, x, y, text, size=9, color=INK, anchor='start', bold=False):
    d.add(String(x,y,text,fontName='Bold' if bold else 'Body',fontSize=size,fillColor=color,textAnchor=anchor))


def axes(d,x=38,y=34,w=WIDTH-65,h=118,xtitle='Cycle position (phase)',ytitle='Value'):
    for v in [0,.5,1]:
        yy=y+v*h
        d.add(Line(x,yy,x+w,yy,strokeColor=colors.HexColor('#DCE7E8'),strokeWidth=.5))
        label(d,x-8,yy-3,str(v).replace('0.5','.5'),8,MUTED,'end')
    for v in [0,.25,.5,.75,1]:label(d,x+v*w,y-15,str(v),8,MUTED,'middle')
    label(d,x,y+h+13,ytitle,9,MUTED)
    label(d,x+w/2,y-31,xtitle,9,MUTED,'middle')
    return x,y,w,h


def curve(d,points,color=TEAL,width=2):
    d.add(PolyLine(points,strokeColor=color,strokeWidth=width,fillColor=None))


def diagram(kind):
    if kind=='chain':
        d=Drawing(WIDTH,130)
        boxes=[(8,53,135,'Phasor','Progress through time'),(174,53,135,'Oscillator','Shape of the value'),(340,53,135,'Output','A changing number')]
        for x,y,w,title,subtitle in boxes:
            d.add(Rect(x,y,w,58,rx=7,ry=7,fillColor=PALE,strokeColor=None))
            label(d,x+w/2,y+35,title,12,INK,'middle',True)
            label(d,x+w/2,y+16,subtitle,9,MUTED,'middle')
        for a,b in [(145,172),(311,338)]:
            d.add(Line(a,82,b,82,strokeColor=TEAL,strokeWidth=2))
            curve(d,[b-5,85,b,82,b-5,79],TEAL,1.5)
        label(d,WIDTH/2,18,'One cable carries cycle progress; the next carries the shaped result.',9,MUTED,'middle')
        return d
    if kind=='phase':
        d=Drawing(WIDTH,177);x,y,w,h=axes(d,h=104)
        curve(d,[x,y,x+w,y+h],ORANGE)
        pts=[]
        for i in range(161):
            p=i/160;pts += [x+p*w,y+(1-math.cos(2*math.pi*p))/2*h]
        curve(d,pts)
        label(d,x+w-3,y+h+16,'PHASE RAMP',8,ORANGE,'end',True)
        label(d,x+w-3,y+h+4,'SMOOTH OUTPUT',8,TEAL,'end',True)
        return d
    if kind=='difference':
        d=Drawing(WIDTH,187);x,y,w,h=axes(d,h=110)
        for off,col in [(0,TEAL),(.25,ORANGE)]:
            pts=[]
            for i in range(161):
                p=i/160;pts += [x+p*w,y+(1-math.cos(2*math.pi*(p+off)))/2*h]
            curve(d,pts,col)
        label(d,x,y+h+28,'Same speed, different cycle positions',10,INK,bold=True)
        label(d,x+225,y+h+28,'0 offset',8,TEAL)
        label(d,x+305,y+h+28,'+0.25 offset',8,ORANGE)
        return d
    if kind=='bank':
        d=Drawing(WIDTH,200)
        x,y,w,h=44,36,WIDTH-62,124
        cols,rows=64,8
        for j in range(rows):
            label(d,x-10,y+(rows-j-.5)*h/rows,str(j+1),8,MUTED,'end')
            for i in range(cols):
                phase=i/cols+(j+.5)/rows
                a=(1-math.cos(2*math.pi*phase))/2
                color=colors.Color(.92-a*.89,.96-a*.48,.96-a*.43)
                d.add(Rect(x+i*w/cols,y+(rows-j-1)*h/rows,w/cols+.2,h/rows-.7,fillColor=color,strokeColor=None))
        label(d,x,y+h+14,'Member',9,MUTED)
        label(d,x+w/2,11,'Time across one cycle  >',9,MUTED,'middle')
        label(d,x+w,y+h+14,'Darker = higher output',9,MUTED,'end')
        return d
    if kind=='shapes':
        d=Drawing(WIDTH,100)
        for k,(title,col) in enumerate([('Round 0: triangle',TEAL),('Round 0.5: smooth',TEAL),('Round 1: square',ORANGE)]):
            x=12+k*160;y=19;w=140;h=54
            d.add(Line(x,y,x+w,y,strokeColor=colors.HexColor('#CADADD')))
            pts=[]
            for i in range(161):
                p=i/160
                v=1-abs(1-2*p) if k==0 else (1-math.cos(2*math.pi*p))/2 if k==1 else (1 if .25<=p<.75 else 0)
                pts += [x+p*w,y+v*h]
            curve(d,pts,col)
            label(d,x+w/2,87,title,9,INK,'middle',True)
            label(d,x,y-14,'0',8,MUTED);label(d,x+w,y-14,'1 cycle',8,MUTED,'end')
        return d
    if kind=='wrap':
        d=Drawing(WIDTH,118)
        for k,(val,text) in enumerate([('0.8','clock phase'),('+ 0.4','member offset'),('= 1.2','before wrapping'),('0.2','after wrapping')]):
            x=10+k*120
            d.add(Rect(x,35,105,65,rx=6,ry=6,fillColor=PALE if k<3 else TEAL,strokeColor=None))
            label(d,x+52.5,69,val,18,INK if k<3 else colors.white,'middle',True)
            label(d,x+52.5,49,text,8,MUTED if k<3 else colors.white,'middle')
        label(d,WIDTH/2,12,'Subtract a complete turn: 1.2 and 0.2 land at the same point in the cycle.',9,MUTED,'middle')
        return d
    raise ValueError(kind)


FIGURES={4:('chain','The first patch: progress becomes a shaped value.'),7:('phase','Phase and output differ, even when they use the same 0-1 range.'),8:('difference','A quarter-cycle phase difference, before extra shaping.'),11:('bank','An eight-member bank over time, using centered phase spacing.'),12:('shapes','Basic Oscillator shapes with other shaping controls at neutral settings.'),13:('wrap','Wrapping keeps phase moving around a cycle.')}


def markup(text):
    text=escape(text)
    text=re.sub(r'`([^`]+)`',r'<font name="Courier" size="9">\1</font>',text)
    text=re.sub(r'\*\*([^*]+)\*\*',r'<b>\1</b>',text)
    return text


class ManualDoc(BaseDocTemplate):
    def afterFlowable(self,flowable):
        if isinstance(flowable,Paragraph) and flowable.style.name=='Chapter' and re.match(r'^\d+\.',flowable.getPlainText()):
            title=flowable.getPlainText();key='chapter-'+title.split('.')[0]
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(title,key,level=0,closed=False)
            self.notify('TOCEntry',(0,title,self.page,key))


def footer(canvas,doc):
    canvas.saveState()
    if doc.page>1:
        canvas.setStrokeColor(colors.HexColor('#DCE7E8'));canvas.setLineWidth(.6)
        canvas.line(56,42,A4[0]-56,42)
        canvas.setFont('Body',8);canvas.setFillColor(MUTED)
        canvas.drawString(56,28,'OCEANODE  /  A BEGINNER\'S GUIDE')
        canvas.drawRightString(A4[0]-56,28,str(doc.page))
        canvas.setFillColor(TEAL);canvas.rect(56,A4[1]-43,23,3,fill=1,stroke=0)
    canvas.restoreState()


def main():
    fonts();OUT.parent.mkdir(parents=True,exist_ok=True);ASSETS.mkdir(parents=True,exist_ok=True)
    styles=getSampleStyleSheet()
    styles.add(ParagraphStyle('Text',fontName='Body',fontSize=10.4,leading=15,textColor=INK,spaceAfter=9))
    styles.add(ParagraphStyle('Chapter',fontName='Bold',fontSize=23,leading=28,textColor=INK,spaceAfter=18,keepWithNext=True))
    styles.add(ParagraphStyle('Subhead',fontName='Bold',fontSize=12,leading=16,textColor=TEAL,spaceBefore=8,spaceAfter=7,keepWithNext=True))
    styles.add(ParagraphStyle('Callout',parent=styles['Text'],fontName='Bold',fontSize=10.2,leading=15,backColor=PALE,borderPadding=10,spaceBefore=6,spaceAfter=14))
    styles.add(ParagraphStyle('List',parent=styles['Text'],leftIndent=15,firstLineIndent=-12,spaceAfter=7))
    styles.add(ParagraphStyle('Cell',parent=styles['Text'],fontSize=9.2,leading=12.5,spaceAfter=0))
    styles.add(ParagraphStyle('CellHead',parent=styles['Cell'],fontName='Bold',textColor=colors.white))
    styles.add(ParagraphStyle('Caption',fontName='Italic',fontSize=8.6,leading=12,textColor=MUTED,spaceAfter=9))
    doc=ManualDoc(str(OUT),pagesize=A4,leftMargin=56,rightMargin=56,topMargin=61,bottomMargin=57,
        title="Oceanode: A Beginner's Guide",author='Oceanode',subject='Patterns, phase, oscillator banks and everyday patching')
    frame=Frame(56,57,WIDTH,A4[1]-118,leftPadding=0,rightPadding=0,topPadding=0,bottomPadding=0)
    doc.addPageTemplates(PageTemplate(id='manual',frames=frame,onPage=footer))
    story=[]
    story += [Spacer(1,48),Paragraph('OCEANODE',ParagraphStyle('CoverLabel',fontName='Bold',fontSize=13,leading=20,textColor=TEAL)),Spacer(1,18)]
    story += [Paragraph("A beginner's guide",ParagraphStyle('CoverTitle',fontName='Bold',fontSize=39,leading=45,textColor=INK)),Spacer(1,15)]
    story += [Paragraph('Making patterns<br/>with numbers',ParagraphStyle('CoverSubtitle',fontName='Body',fontSize=26,leading=34,textColor=INK)),Spacer(1,24)]
    story += [Paragraph('From your first connection to travelling waves,<br/>oscillator banks and a patch you can perform.',styles['Text']),Spacer(1,27),diagram('bank'),Spacer(1,18)]
    story += [Paragraph('Edition 1  /  October 2026',styles['Caption']),Paragraph('For artists, musicians and curious beginners.<br/>No programming or advanced mathematics needed.',styles['Text']),PageBreak()]
    story += [Paragraph('Inside this guide',styles['Chapter'])]
    toc=TableOfContents();toc.levelStyles=[ParagraphStyle('TOC',fontName='Body',fontSize=9.8,leading=13,textColor=INK,leftIndent=0,firstLineIndent=0,spaceBefore=3)]
    story += [toc,PageBreak(),Paragraph('Before you begin',styles['Chapter'])]
    source=(ROOT/'docs/oceanode-user-manual.md').read_text()
    preamble,body=source.split('# 1.',1)
    intro=preamble.split('This guide follows',1)[1]
    intro='This guide follows'+intro
    intro=intro.replace('## How to use this guide','')
    for p in re.split(r'\n\s*\n',intro.strip()):story.append(Paragraph(markup(p.replace('\n',' ')),styles['Text']))
    sections=re.split(r'^# ', '# 1.'+body,flags=re.M)[1:]
    for section in sections:
        lines=section.splitlines();title=lines.pop(0);number=int(title.split('.')[0]);story += [PageBreak(),Paragraph(markup(title),styles['Chapter'])]
        i=0
        while i<len(lines):
            line=lines[i].strip()
            if not line:i+=1;continue
            if line.startswith('!['):i+=1;continue
            if line.startswith('## '):story.append(Paragraph(markup(line[3:]),styles['Subhead']));i+=1;continue
            if line.startswith('|'):
                rows=[]
                while i<len(lines) and lines[i].strip().startswith('|'):
                    row=[s.strip() for s in lines[i].strip().strip('|').split('|')]
                    if not all(re.fullmatch(r'[-: ]+',c) for c in row):rows.append(row)
                    i+=1
                cols=len(rows[0]); widths=([WIDTH*.31,WIDTH*.69] if cols==2 else [WIDTH/cols]*cols)
                if number==22:widths=[WIDTH*.24,WIDTH*.76]
                table=Table([[Paragraph(markup(c),styles['CellHead'] if r==0 else styles['Cell']) for c in row] for r,row in enumerate(rows)],colWidths=widths,repeatRows=1,hAlign='LEFT')
                table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),TEAL),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.HexColor('#F0F6F6'),colors.white]),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),9),('RIGHTPADDING',(0,0),(-1,-1),9),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),('LINEBELOW',(0,-1),(-1,-1),.4,colors.HexColor('#DCE7E8'))]))
                story += [table,Spacer(1,12)];continue
            if line.startswith('> '):
                story.append(Paragraph(markup(line[2:]),styles['Callout']));i+=1;continue
            if line.startswith('- ') or re.match(r'^\d+\. ',line):
                story.append(Paragraph(markup(line),styles['List']));i+=1;continue
            para=[line];i+=1
            while i<len(lines) and lines[i].strip() and not re.match(r'^(#|\||>|- |\d+\. |!\[)',lines[i].strip()):
                para.append(lines[i].strip());i+=1
            story.append(Paragraph(markup(' '.join(para)),styles['Text']))
        if number in FIGURES:
            kind,caption=FIGURES[number];draw=diagram(kind)
            renderSVG.drawToFile(draw,str(ASSETS/(kind+'.svg')))
            story.append(KeepTogether([Spacer(1,5),draw,Paragraph(caption,styles['Caption'])]))
    doc.multiBuild(story)
    print(OUT)

if __name__=='__main__':main()

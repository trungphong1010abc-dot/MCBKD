import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent / 'deps'))
from reportlab.pdfgen import canvas
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.pagesizes import A4

root = Path.cwd()
files = [root/'platformio.ini', root/'partitions.csv'] + sorted((root/'src').rglob('*'))
files = [p for p in files if p.is_file()]
pdfmetrics.registerFont(TTFont('Code', 'C:/Windows/Fonts/consola.ttf'))
pdfmetrics.registerFont(TTFont('Title', 'C:/Windows/Fonts/arial.ttf'))
W,H = A4
pages = []
entries = []
total_lines = 0
for p in files:
    name = p.relative_to(root).as_posix()
    lines = p.read_text(encoding='utf-8-sig').splitlines()
    total_lines += len(lines)
    rows = []
    for n,line in enumerate(lines,1):
        line = line.expandtabs(4)
        chunks = [line[i:i+100] for i in range(0,len(line),100)] or ['']
        assert ''.join(chunks) == line
        rows.extend((str(n) if i==0 else '', chunk) for i,chunk in enumerate(chunks))
    entries.append((name,len(pages)+2,len(lines)))
    for i in range(0,max(1,len(rows)),65):
        pages.append((name,rows[i:i+65]))
out = root/'output/pdf/Tong_hop_source_code.pdf'
out.parent.mkdir(parents=True,exist_ok=True)
c = canvas.Canvas(str(out),pagesize=A4)
c.setTitle('Tổng hợp mã nguồn - Code_MCBKD')
c.setAuthor('Code_MCBKD')
def footer(n):
    c.setFont('Title',8)
    c.setFillColorRGB(.4,.45,.5)
    c.drawString(36,25,'Code_MCBKD | Tổng hợp mã nguồn')
    c.drawRightString(W-36,25,f'{n} / {len(pages)+1}')
def title(text):
    c.setFillColorRGB(.08,.18,.28)
    c.setFont('Title',18)
    c.drawString(36,H-48,text)
title('TỔNG HỢP MÃ NGUỒN')
c.setFont('Title',10)
c.drawString(36,H-73,f'{len(files)} tệp | {total_lines} dòng mã nguồn | 10/09/2026')
c.drawString(36,H-95,'Toàn bộ tệp trong src, platformio.ini và partitions.csv.')
c.drawString(36,H-113,'Dòng dài được xuống dòng khi trình bày; số bên trái là số dòng gốc.')
c.setFont('Title',12)
c.drawString(36,H-149,'Mục lục')
for i,(name,page,count) in enumerate(entries):
    y = H-176-i*23
    c.setFont('Code',10)
    c.drawString(36,y,name)
    c.setFont('Title',9)
    c.drawRightString(W-36,y,f'{count} dòng   |   Trang {page}')
    c.linkAbsolute(name,f'f{i}',(36,y-4,W-36,y+12))
footer(1)
c.showPage()
starts = {page:(i,name) for i,(name,page,_) in enumerate(entries)}
for n,(name,rows) in enumerate(pages,2):
    if n in starts:
        i,_ = starts[n]
        c.bookmarkPage(f'f{i}')
        c.addOutlineEntry(name,f'f{i}',0)
    title(name)
    c.setStrokeColorRGB(.78,.82,.86)
    c.line(36,H-61,W-36,H-61)
    for j,(number,line) in enumerate(rows):
        y=H-82-j*10.7
        c.setFont('Code',8)
        c.setFillColorRGB(.5,.55,.6)
        c.drawRightString(58,y,number)
        c.setFillColorRGB(.08,.1,.14)
        c.drawString(68,y,line)
        assert 68+pdfmetrics.stringWidth(line,'Code',8) <= W-30
    footer(n)
    c.showPage()
c.save()
print(f'Created {out}: {len(files)} files, {total_lines} lines, {len(pages)+1} pages')

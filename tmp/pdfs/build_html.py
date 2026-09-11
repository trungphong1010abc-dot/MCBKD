from pathlib import Path
from html import escape
root=Path.cwd()
files=[root/'platformio.ini',root/'partitions.csv']+sorted(p for p in (root/'src').rglob('*') if p.is_file())
pages=[]; entries=[]; count=0
for p in files:
 rows=[]
 lines=p.read_text(encoding='utf-8-sig').splitlines(); count+=len(lines)
 name=p.relative_to(root).as_posix(); entries.append((name,len(pages)+2))
 for n,line in enumerate(lines,1):
  line=line.expandtabs(4); chunks=[line[i:i+100] for i in range(0,len(line),100)] or ['']
  assert ''.join(chunks)==line
  rows += [f'<div><span>{n if i==0 else ""}</span>{escape(chunk)}</div>' for i,chunk in enumerate(chunks)]
 for j in range(0,max(len(rows),1),62): pages.append((name,''.join(rows[j:j+62])))
style='''@page{size:A4;margin:0}*{box-sizing:border-box}body{margin:0;color:#162c40;font-family:Arial}section{width:210mm;height:297mm;padding:16mm 12mm;position:relative;break-after:page}section:last-child{break-after:auto}h1{font-size:20px;margin:0 0 8mm}pre{font-family:Consolas;font-size:10px;line-height:14px;margin:0;color:#17212b}pre span{display:inline-block;width:8mm;text-align:right;margin-right:3mm;color:#81909c}footer{position:absolute;bottom:8mm;left:12mm;right:12mm;font-size:10px;color:#667788;display:flex;justify-content:space-between}li{font-family:Consolas;font-size:12px;margin-bottom:12px}li b{float:right}p{font-size:12px;line-height:1.6}hr{border:0;border-top:1px solid #ccd5dc;margin-bottom:5mm}'''
def foot(n):return f'<footer><span>Code_MCBKD | Tổng hợp mã nguồn</span><span>{n} / {len(pages)+1}</span></footer>'
html='<!doctype html><meta charset="utf-8"><title>Tổng hợp mã nguồn</title><style>'+style+'</style><section><h1>TỔNG HỢP MÃ NGUỒN</h1>'+f'<p>{len(files)} tệp | {count} dòng mã nguồn | 10/09/2026</p><p>Toàn bộ tệp trong src, platformio.ini và partitions.csv.<br>Dòng dài được xuống dòng; số bên trái là số dòng gốc.</p><h2>Mục lục</h2><ol>'
html+=''.join(f'<li>{escape(name)}<b>{page}</b></li>' for name,page in entries)+'</ol>'+foot(1)+'</section>'
for n,(name,rows) in enumerate(pages,2):html+=f'<section><h1>{escape(name)}</h1><hr><pre>{rows}</pre>'+foot(n)+'</section>'
(root/'tmp/pdfs/source.html').write_text(html,encoding='utf-8')
(root/'output/pdf').mkdir(parents=True,exist_ok=True)
print(len(files),count,len(pages)+1)

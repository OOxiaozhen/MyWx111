import runpy, zipfile, os, html
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
ns=runpy.run_path(r'F:\综设\MyWx111\make_deck.py'); data=ns['data']; out=Path(r'F:\综设\MyWx111\MyWx111_EDA答辩.pptx')
fontp='C:/Windows/Fonts/msyh.ttc'; fontb='C:/Windows/Fonts/msyhbd.ttc'
slides=[]
for idx,(title,sub,cards) in enumerate(data,1):
 im=Image.new('RGB',(1600,900),(23,37,56)); d=ImageDraw.Draw(im)
 ft=ImageFont.truetype(fontb,42); fs=ImageFont.truetype(fontp,23); fh=ImageFont.truetype(fontb,28); fb=ImageFont.truetype(fontp,25); ff=ImageFont.truetype(fontp,18)
 d.text((72,38),title,font=ft,fill=(255,255,255)); d.text((76,105),sub,font=fs,fill=(102,217,200))
 n=len(cards); cols=2 if n>=4 else 1; rows=(n+cols-1)//cols; gap=22; left=76; top=175; cw=(1448-gap*(cols-1))//cols; ch=(655-gap*(rows-1))//rows
 for j,(head,body) in enumerate(cards):
  x=left+(j%cols)*(cw+gap); y=top+(j//cols)*(ch+gap); d.rounded_rectangle((x,y,x+cw,y+ch),radius=22,fill=(38,59,82),outline=(53,83,107),width=2)
  d.text((x+28,y+22),head,font=fh,fill=(102,217,200)); yy=y+66
  words=body; lines=[]; cur=''
  for part in words.split(' '):
   test=(cur+' '+part).strip()
   if d.textbbox((0,0),test,font=fb)[2]>cw-60 and cur: lines.append(cur); cur=part
   else: cur=test
  if cur: lines.append(cur)
  # fallback break overly long Chinese strings by pixels
  for line in lines:
   if d.textbbox((0,0),line,font=fb)[2] < cw-60: d.text((x+28,yy),line,font=fb,fill=(234,242,247)); yy+=39
   else:
    seg=''
    for chx in line:
     if d.textbbox((0,0),seg+chx,font=fb)[2]>cw-60 and seg: d.text((x+28,yy),seg,font=fb,fill=(234,242,247));yy+=38;seg=chx
     else:seg+=chx
    if seg:d.text((x+28,yy),seg,font=fb,fill=(234,242,247));yy+=38
 # Insert source-grounded diagrams in the lower margin of the relevant slides.
 teal=(102,217,200); white=(234,242,247); orange=(255,190,92); muted=(130,150,170)
 if idx==5:
  # Orthogonal routing example with coordinates and 20-unit stubs.
  y0=770; x0=850; x1=1510; mid=(x0+x1)//2
  d.line([(x0,y0-5),(x0+95,y0-5),(mid,y0-5),(mid,y0-48),(x1-95,y0-48),(x1,y0-48)],fill=teal,width=7,joint='curve')
  d.ellipse((x0-10,y0-15,x0+10,y0+5),fill=orange); d.ellipse((x1-10,y0-58,x1+10,y0-38),fill=orange)
  d.text((x0,y0-54),'起点 PinSide=Right',font=ff,fill=white);d.text((x1-260,y0-95),'终点 PinSide=Left',font=ff,fill=white)
  d.text((mid-130,y0-42),'midX=(stubStart.x+stubEnd.x)/2',font=ff,fill=orange)
 if idx==8:
  # AND gate truth table as a compact chart.
  bx,by,bw,bh=850,675,650,34
  headers=['A','B','AND 输出 Y']; rows=[['0','0','0'],['0','1','0'],['1','0','0'],['1','1','1']]
  widths=[bw//3]*3
  for r,row in enumerate([headers]+rows):
   xx=bx
   for c,val in enumerate(row):
    fill=(53,83,107) if r==0 else ((38,59,82) if r%2 else (30,48,68))
    d.rectangle((xx,by+r*bh,xx+widths[c],by+(r+1)*bh),fill=fill,outline=teal,width=2)
    d.text((xx+widths[c]//2,by+r*bh+8),val,font=fh if r==0 else fb,fill=teal if r==0 else white,anchor='ma')
    xx+=widths[c]
 if idx==3:
  # Layered data-flow illustration.
  labels=['UI / Canvas','Circuit Model','Simulator','Netlist / PCB IO']
  xs=[120,475,830,1185]
  for k,label in enumerate(labels):
   d.rounded_rectangle((xs[k],790,xs[k]+280,845),radius=14,fill=(38,59,82),outline=teal,width=2)
   d.text((xs[k]+140,817),label,font=ff,fill=white,anchor='mm')
   if k<3:
    d.line((xs[k]+285,817,xs[k+1]-12,817),fill=orange,width=4)
    d.polygon([(xs[k+1]-12,809),(xs[k+1],817),(xs[k+1]-12,825)],fill=orange)
 if idx==7:
  # Physical mapping from two gates to one chip.
  d.rounded_rectangle((160,788,510,846),radius=12,fill=(38,59,82),outline=teal,width=2);d.text((335,817),'XOR 门 A · slot 1',font=ff,fill=white,anchor='mm')
  d.rounded_rectangle((570,788,920,846),radius=12,fill=(38,59,82),outline=teal,width=2);d.text((745,817),'XOR 门 B · slot 2',font=ff,fill=white,anchor='mm')
  d.line((515,817,565,817),fill=orange,width=4)
  d.rounded_rectangle((980,780,1430,852),radius=12,fill=(53,83,107),outline=orange,width=3);d.text((1205,795),'U1 · 7486 · DIP14',font=ff,fill=white,anchor='mm');d.text((1205,829),'共享 refDes → 同一物理芯片',font=ff,fill=teal,anchor='mm')
 d.text((76,858),f'MyWx111  ·  EDA 项目答辩  ·  {idx:02d}',font=ff,fill=(130,150,170))
 p=Path(r'F:\综设\MyWx111')/f'_slide{idx}.png'; im.save(p);slides.append(p)
EMU=914400
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 z.writestr('[Content_Types].xml','<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/><Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/><Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/><Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>'+''.join(f'<Override PartName="/ppt/slides/slide{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>' for i in range(1,len(slides)+1))+'</Types>')
 z.writestr('_rels/.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/></Relationships>')
 z.writestr('ppt/presentation.xml','<?xml version="1.0" encoding="UTF-8"?><p:presentation xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst><p:sldIdLst>'+''.join(f'<p:sldId id="{256+i}" r:id="rId{i+1}"/>' for i in range(1,len(slides)+1))+'</p:sldIdLst><p:sldSz cx="12192000" cy="6858000" type="screen16x9"/><p:notesSz cx="6858000" cy="9144000"/></p:presentation>')
 z.writestr('ppt/_rels/presentation.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>'+''.join(f'<Relationship Id="rId{i+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide{i}.xml"/>' for i in range(1,len(slides)+1))+'</Relationships>')
 z.writestr('ppt/slideMasters/slideMaster1.xml','<?xml version="1.0" encoding="UTF-8"?><p:sldMaster xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld><p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folhlink"/><p:sldLayoutIdLst><p:sldLayoutId id="1" r:id="rId1"/></p:sldLayoutIdLst></p:sldMaster>')
 z.writestr('ppt/slideMasters/_rels/slideMaster1.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/></Relationships>')
 z.writestr('ppt/slideLayouts/slideLayout1.xml','<?xml version="1.0" encoding="UTF-8"?><p:sldLayout xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" type="blank" preserve="1"><p:cSld name="Blank"><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>')
 z.writestr('ppt/slideLayouts/_rels/slideLayout1.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="../slideMasters/slideMaster1.xml"/></Relationships>')
 for i,p in enumerate(slides,1):
  z.write(p,f'ppt/media/image{i}.png')
  xml=f'''<?xml version="1.0" encoding="UTF-8"?><p:sld xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr><p:pic><p:nvPicPr><p:cNvPr id="2" name="SlideImage{i}"/><p:cNvPicPr><a:picLocks noChangeAspect="1"/></p:cNvPicPr><p:nvPr/></p:nvPicPr><p:blipFill><a:blip r:embed="rId2"/><a:stretch><a:fillRect/></a:stretch></p:blipFill><p:spPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="12192000" cy="6858000"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic></p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>'''
  z.writestr(f'ppt/slides/slide{i}.xml',xml)
  z.writestr(f'ppt/slides/_rels/slide{i}.xml.rels',f'<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/image{i}.png"/></Relationships>')
for p in slides:p.unlink(missing_ok=True)
print(out, out.stat().st_size, len(slides))


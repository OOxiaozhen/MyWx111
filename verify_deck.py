from pptx import Presentation
from zipfile import ZipFile
from pathlib import Path
p=Path(r'F:\综设\MyWx111\MyWx111_EDA答辩_具体案例版.pptx')
prs=Presentation(p)
print('slide_count',len(prs.slides),'shape_counts',[len(s.shapes) for s in prs.slides])
print('first_title',[sh.text for sh in prs.slides[0].shapes if getattr(sh,'has_text_frame',False) and sh.text][:1])
with ZipFile(p) as z: print('zip_test',z.testzip(),'bytes',p.stat().st_size)

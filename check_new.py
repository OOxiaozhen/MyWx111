from pptx import Presentation
p=r'F:\综设\MyWx111\MyWx111_EDA答辩_新版.pptx'
prs=Presentation(p)
print('slides',len(prs.slides))
for i,sl in enumerate(prs.slides,1):
 texts=[sh.text.replace('\n','/') for sh in sl.shapes if getattr(sh,'has_text_frame',False) and sh.text.strip()]
 print(i,' | '.join(texts[:6]))

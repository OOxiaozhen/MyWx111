from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pathlib import Path


OUT=Path(r'F:\综设\MyWx111\MyWx111_EDA答辩_具体案例版.pptx')
prs=Presentation(); prs.slide_width=Inches(13.333); prs.slide_height=Inches(7.5)
BLANK=prs.slide_layouts[6]
NAVY=RGBColor(23,37,56); BLUE=RGBColor(38,76,120); TEAL=RGBColor(0,150,136); CYAN=RGBColor(102,217,200); GOLD=RGBColor(238,164,62); INK=RGBColor(35,49,65); MUTED=RGBColor(90,108,128); WHITE=RGBColor(255,255,255); PALE=RGBColor(235,242,248); PALETEAL=RGBColor(222,244,240); BORDER=RGBColor(197,211,224)

def textbox(slide,x,y,w,h,text,size=18,color=INK,bold=False,align=PP_ALIGN.LEFT,font='Microsoft YaHei',margin=.08):
 sh=slide.shapes.add_textbox(Inches(x),Inches(y),Inches(w),Inches(h)); tf=sh.text_frame; tf.clear();tf.word_wrap=True;tf.margin_left=Inches(margin);tf.margin_right=Inches(margin);tf.margin_top=Inches(margin);tf.margin_bottom=Inches(margin)
 for i,line in enumerate(str(text).split('\n')):
  p=tf.paragraphs[0] if i==0 else tf.add_paragraph();p.text=line;p.alignment=align;p.font.name=font;p.font.size=Pt(size);p.font.bold=bold;p.font.color.rgb=color
 return sh

def rect(slide,x,y,w,h,fill=WHITE,line=BORDER,radius=True):
 sh=slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE if radius else MSO_SHAPE.RECTANGLE, Inches(x),Inches(y),Inches(w),Inches(h)); sh.fill.solid();sh.fill.fore_color.rgb=fill;sh.line.color.rgb=line;sh.line.width=Pt(1);return sh

def line(slide,x1,y1,x2,y2,color=TEAL,width=2,begin=None,end=None):
 sh=slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT, Inches(x1), Inches(y1), Inches(x2), Inches(y2)); sh.line.color.rgb=color;sh.line.width=Pt(width)
 if end: sh.line.end_arrowhead=True
 if begin: sh.line.begin_arrowhead=True
 return sh

def base(title,sub='',dark=False):
 s=prs.slides.add_slide(BLANK); bg=s.background.fill;bg.solid();bg.fore_color.rgb=NAVY if dark else PALE
 if dark:
  textbox(s,.65,.4,12, .55,title,30,WHITE,True); textbox(s,.68,1.03,11.9,.35,sub,14,CYAN)
 else:
  textbox(s,.65,.32,12,.52,title,26,NAVY,True);textbox(s,.68,.9,12,.34,sub,13,TEAL)
 textbox(s,.68,7.12,12,.18,'MyWx111 · EDA 原理图编辑与仿真平台',9,MUTED)
 return s

def card(slide,x,y,w,h,title,body,accent=TEAL,body_size=15):
 rect(slide,x,y,w,h,WHITE,BORDER);rect(slide,x,y,.09,h,accent,accent,False)
 textbox(slide,x+.2,y+.12,w-.35,.35,title,16,NAVY,True)
 textbox(slide,x+.2,y+.53,w-.36,h-.62,body,body_size,INK)

def table(slide,x,y,w,h,headers,rows,widths=None,font=13):
 t=slide.shapes.add_table(len(rows)+1,len(headers),Inches(x),Inches(y),Inches(w),Inches(h)).table
 if widths:
  for i,v in enumerate(widths):t.columns[i].width=Inches(w*v)
 for r,row in enumerate([headers]+rows):
  for c,val in enumerate(row):
   cell=t.cell(r,c);cell.text=str(val);cell.vertical_anchor=MSO_ANCHOR.MIDDLE;cell.margin_left=Inches(.08);cell.margin_right=Inches(.08);cell.margin_top=Inches(.03);cell.margin_bottom=Inches(.03);cell.fill.solid();cell.fill.fore_color.rgb=BLUE if r==0 else (WHITE if r%2 else PALE)
   for p in cell.text_frame.paragraphs:
    p.font.name='Microsoft YaHei';p.font.size=Pt(font);p.font.bold=(r==0);p.font.color.rgb=WHITE if r==0 else INK

 return t

# 1 Cover
s=base('从引脚到网络：MyWx111 EDA 项目实现','把原理图编辑、逻辑仿真与 PCB 数据导出串成一条工程链路',True)
textbox(s,.9,2.05,6.6,1.1,'进阶式挑战性综合项目 I',23,CYAN,True)
textbox(s,.9,3.0,7.2,1.45,'一款以 C++ / wxWidgets 实现的\n数字逻辑原理图编辑与仿真工具',27,WHITE,True)
for i,(h,b) in enumerate([('编辑','元件 / 引脚 / 正交连线'),('验证','网络构建 / 电平传播'),('交付','工程 JSON / gEDA / PCB')]):
 x=.9+i*3.95;rect(s,x,5.25,3.45,1.02,BLUE,BLUE);textbox(s,x+.18,5.38,3.05,.3,h,16,CYAN,True);textbox(s,x+.18,5.75,3.08,.34,b,13,WHITE)

# 2 feature map
s=base('本次答辩讲清 5 个已实现的工程问题','每个模块都能在代码结构中找到对应实现')
for i,(h,b) in enumerate([('画布交互','放置、选择、拖动、旋转、缩放与鼠标事件'),('电路模型','Component / Pin / Wire / Circuit 与 ComponentFactory'),('正交布线','基于 PinSide 方向生成 L/Z 形路径，支持手动折点'),('逻辑仿真','并查集构网，Evaluate 迭代传播，稳定后结束'),('工程互操作','JSON v3 保存加载，通用网表、gEDA 网表与 PCB 文件')]):
 x=.72+(i%3)*4.15;y=1.6+(i//3)*2.15;card(s,x,y,3.85,1.72,h,b,[TEAL,BLUE,GOLD,TEAL,BLUE][i],14)

# 3 architecture
s=base('系统架构：Circuit 是编辑、仿真、导出的共享数据源','视图层负责显示；业务逻辑围绕统一电路模型流转')
layers=[('UI / 事件','MainFrame · ToolPalette · PropertiesPanel'),('View','CircuitCanvas · ComponentRenderer · GridTransform'),('Model','Circuit · Component · Pin · Wire · UndoStack'),('Sim','Simulator：net 拓扑 + LogicLevel 传播'),('IO','ProjectSerializer · NetlistExporter · Geda* · PcbFileExporter')]
for i,(h,b) in enumerate(layers):
 y=1.45+i*1.0;rect(s,1.0,y,2.1,.68,BLUE,BLUE);textbox(s,1.08,y+.13,1.95,.38,h,15,WHITE,True,PP_ALIGN.CENTER);rect(s,3.4,y,8.75,.68,WHITE,BORDER);textbox(s,3.62,y+.14,8.35,.36,b,15,INK)
 if i<4:line(s,2.05,y+.69,2.05,y+.96,GOLD,2,end=True)
textbox(s,1,6.55,11,.35,'数据主线：用户操作 → Circuit 状态变更 → 同一状态供画布刷新、Simulator 求值和 IO 导出',14,TEAL,True)

# 4 model
s=base('对象模型：连线端点引用“元件 ID + 引脚序号”','Wire 不以像素线段代表电气连接；几何路径与电气拓扑各自表达')
rect(s,.9,2.0,3.05,2.15,WHITE,BLUE);textbox(s,1.12,2.18,2.6,.34,'U1：XOR 门',18,NAVY,True);textbox(s,1.12,2.68,2.55,1.1,'id = 12\nrefDes = U1\nslot = 1',16,INK)
rect(s,9.35,2,3.05,2.15,WHITE,BLUE);textbox(s,9.58,2.18,2.6,.34,'U2：AND 门',18,NAVY,True);textbox(s,9.58,2.68,2.55,1.1,'id = 18\n输入 pin = 0\n输出 pin = 2',16,INK)
# routed connection
line(s,3.95,3.0,5.0,3.0,TEAL,4);line(s,5.0,3.0,7.6,3.0,TEAL,4);line(s,7.6,3.0,7.6,3.75,TEAL,4);line(s,7.6,3.75,9.35,3.75,TEAL,4)
textbox(s,4.0,4.0,5.6,.55,'Wire: from=(12, outPin) → to=(18, 0)',16,BLUE,True,PP_ALIGN.CENTER)
card(s,1,5.05,5.3,1.25,'电气引用','PinRef(componentId, pinIndex)：供构网、仿真与网表使用',TEAL,14)
card(s,6.95,5.05,5.3,1.25,'几何路径','自动路径或 bends 折点：供画布渲染与编辑使用',GOLD,14)

# 5 routing detailed
s=base('布线算法案例：两端水平出线 → 中点 Z 形','源码 BuildOrthogonalPath：先计算引出点，再按引脚方向选择拓扑')
# coordinate example
textbox(s,.9,1.45,5.1,.42,'具体坐标（世界坐标，单位：画布坐标）',16,NAVY,True)
textbox(s,.95,1.95,5.0,1.5,'起点 S=(100,100)，侧向 Right\n终点 E=(300,180)，侧向 Left\n引出段长度 = 20',15,INK)
table(s,.9,3.62,5.4,2.05,['变量','计算','结果'],[['stubStart','S + (20,0)','(120,100)'],['stubEnd','E + (-20,0)','(280,180)'],['midX','(120+280)/2','200']], [0.28,.40,.32],13)
# draw path coordinates in schematic panel
rect(s,6.75,1.55,5.55,4.75,WHITE,BORDER)
textbox(s,7.0,1.75,5.0,.38,'生成路径点（Z 形）',16,NAVY,True,PP_ALIGN.CENTER)
pts=[(7.15,3.15),(8.0,3.15),(9.45,3.15),(9.45,4.45),(11.2,4.45),(12.0,4.45)]
for a,b in zip(pts,pts[1:]):line(s,*a,*b,TEAL,4)
for px,py,label in [(pts[0][0],pts[0][1],'S'),(pts[1][0],pts[1][1],'stubStart'),(pts[2][0],pts[2][1],'midX=200'),(pts[3][0],pts[3][1],'midY'),(pts[4][0],pts[4][1],'stubEnd'),(pts[5][0],pts[5][1],'E')]:
 sh=s.shapes.add_shape(MSO_SHAPE.OVAL,Inches(px-.07),Inches(py-.07),Inches(.14),Inches(.14));sh.fill.solid();sh.fill.fore_color.rgb=GOLD;sh.line.fill.background();textbox(s,px-.48,py-.42,1.05,.26,label,10,MUTED,True,PP_ALIGN.CENTER)
textbox(s,.95,6.1,11.6,.48,'AppendOrthogonalPoint 会跳过重复点，并把三点共线的中间点替换掉；因此最终折点数不会被退化段污染。',13,TEAL,True)

# 6 all route cases
s=base('布线分支：朝向决定 L / Z，手动折点仍保持正交','PinSide 表示引脚所在几何侧；它与 Input/Output 电气方向是两个概念')
# three mini charts
for x,title in [(0.8,'水平—水平：Z 形'),(4.95,'垂直—垂直：横版 Z'),(9.1,'一横一竖：L 形')]:
 rect(s,x,1.6,3.45,3.55,WHITE,BORDER);textbox(s,x+.16,1.78,3.1,.34,title,15,NAVY,True,PP_ALIGN.CENTER)
# paths
for a,b in [((1.15,3.0),(1.8,3.0)),((1.8,3.0),(2.8,3.0)),((2.8,3.0),(2.8,4.05)),((2.8,4.05),(3.9,4.05))]:line(s,*a,*b,TEAL,3)
for a,b in [((5.45,2.55),(5.45,3.0)),((5.45,3.0),(6.25,3.0)),((6.25,3.0),(6.25,4.2)),((6.25,4.2),(7.0,4.2)),((7.0,4.2),(7.0,4.65))]:line(s,*a,*b,BLUE,3)
for a,b in [((9.65,3.0),(10.3,3.0)),((10.3,3.0),(10.3,4.1)),((10.3,4.1),(11.9,4.1))]:line(s,*a,*b,GOLD,3)
card(s,.9,5.52,5.55,1.0,'手动折点','AppendOrthogonalSegment：斜向两点之间插入 (to.x, from.y)，拆成 L 段',TEAL,13)
card(s,6.8,5.52,5.55,1.0,'路径不变量','每对相邻点满足 x 相同或 y 相同；不会绘制斜线段',BLUE,13)

# 7 component attributes
s=base('元件属性案例：74x86 门实例如何映射到实体芯片','Component 保存逻辑对象属性，也保存 PCB 导出所需的物理器件属性')
table(s,.8,1.55,7.0,4.75,['字段','实例值','用途'],[['name','异或门 1','画布 / 属性面板显示'],['orientation','90°','旋转后变换引脚坐标'],['bitWidth','1','SetBitWidth 限制在 1–32'],['refDes','U1','物理器件参考编号'],['value','7486','器件型号'],['footprint','DIP14','封装映射'],['slot','1','芯片内门序号']], [0.23,.25,.52],13)
rect(s,8.2,1.55,4.25,4.75,WHITE,BORDER);textbox(s,8.48,1.82,3.65,.4,'同一物理芯片的 4 个门',17,NAVY,True,PP_ALIGN.CENTER)
for i in range(4):
 x=8.6+(i%2)*1.75;y=2.55+(i//2)*1.2;rect(s,x,y,1.45,.78,PALETEAL,TEAL);textbox(s,x+.05,y+.15,1.35,.4,f'U1 / slot {i+1}',13,NAVY,True,PP_ALIGN.CENTER)
textbox(s,8.52,5.2,3.6,.65,'显式 refDes 相同 → Circuit 按 U1 聚合门实例；slot 用于区分门位。',13,INK)
textbox(s,.9,6.52,11.4,.3,'空 refDes：按类型自动编号；自动分组规则：同类型每 4 个门一片，生成连续 slot。',13,TEAL,True)

# 8 chip grouping actual algorithm
s=base('芯片分组：从逻辑门集合生成 PCB 器件组','Circuit::GetChipGroups 对显式编号和自动分配分别处理')
card(s,.8,1.55,3.75,1.4,'① 显式 refDes','先按 refDes 聚合；同 U1 的门放入同一 ChipGroup。',TEAL,14)
card(s,4.8,1.55,3.75,1.4,'② 无编号分桶','剩余门按 ComponentType 分桶，并按每片 4 门切组。',BLUE,14)
card(s,8.8,1.55,3.75,1.4,'③ 生成编号和 slot','自动组得到 U1/U2…；slot 从 1 连续递增。',GOLD,14)
# table
rows=[]
for i in range(1,7):rows.append([f'XOR 门实例 {i}', '显式 U1' if i<3 else '自动分配', f'U1 / slot {i}' if i<3 else f'U{(i-3)//4+2} / slot {(i-3)%4+1}'])
table(s,1.2,3.45,10.9,2.35,['模型中的门','分组来源','PCB 器件视图'],rows,[.38,.28,.34],13)
textbox(s,1.2,6.15,10.8,.45,'校验点：同一 refDes 下 slot 重复会被检测并报告冲突。',14,TEAL,True)

# 9 simulation topology
s=base('仿真构网：并查集把引脚连接归并为网络','Simulator::RebuildTopology 先登记引脚，再 Union 连线两端')
# graph pins and wire
nodes=[(1.5,3.15,'A.out'),(4.0,3.15,'AND.in0'),(7.1,3.15,'AND.out'),(10.5,3.15,'LED.in')]
for x,y,l in nodes:
 rect(s,x-.7,y-.48,1.4,.8,PALETEAL,TEAL);textbox(s,x-.63,y-.27,1.26,.35,l,13,NAVY,True,PP_ALIGN.CENTER)
line(s,2.2,3.15,3.3,3.15,BLUE,3);line(s,7.8,3.15,9.8,3.15,BLUE,3)
textbox(s,.9,1.55,11.5,.4,'两个 Wire 各自 Union 端点；若连到 Splitter，其全部引脚也被合并为同一网络。',16,NAVY,True)
card(s,1.0,4.45,3.4,1.3,'映射 1','pin key = (componentId << 20) | pinIndex',TEAL,13)
card(s,4.95,4.45,3.4,1.3,'映射 2','pinToNet：查询任一引脚所属 net',BLUE,13)
card(s,8.9,4.45,3.4,1.3,'映射 3','wireToNet：读取该连线当前逻辑电平',GOLD,13)
textbox(s,1.0,6.15,11.4,.4,'分线器被当作透明导通节点，而不是“会改变信号”的逻辑元件。',14,TEAL,True)

# 10 truth table
s=base('仿真具体例：AND 门逐轮传播到 LED','InputPin 提供 0/1 驱动；AndGate::Evaluate 输出逻辑电平')
table(s,.8,1.55,5.3,3.2,['A','B','Y=A∧B'],[['0','0','0'],['0','1','0'],['1','0','0'],['1','1','1']],[.28,.28,.44],16)
# flow boxes
steps=[('输入网络','A=1\nB=1'),('门求值','Evaluate([1,1])'),('输出驱动','Y=1'),('显示端','LED 高电平')]
for i,(h,b) in enumerate(steps):
 x=6.5+i*1.58;rect(s,x,2.2,1.35,1.48,WHITE,TEAL);textbox(s,x+.08,2.4,1.19,.32,h,13,NAVY,True,PP_ALIGN.CENTER);textbox(s,x+.08,2.92,1.19,.55,b,12,INK,False,PP_ALIGN.CENTER)
 if i<3:line(s,x+1.36,2.92,x+1.55,2.92,GOLD,2,end=True)
card(s,6.5,4.35,5.95,1.0,'一次 Evaluate 的语义','输入向量按输入引脚 index 升序传入；输出再作为该 net 的驱动值参与合并。',BLUE,13)
textbox(s,.9,5.75,11.5,.52,'示例电路：Input A ─┐\n                 AND ── LED；Input B ─┘',16,TEAL,True)

# 11 evaluate loop
s=base('仿真迭代：从初值到稳定状态','Simulator::Evaluate 每轮重算组合逻辑输出，检查网络值是否变化')
flow=[('1','读取输入 net'),('2','调用各门 Evaluate'),('3','合并输出驱动'),('4','比较网络状态'),('5','稳定则结束')]
for i,(n,t) in enumerate(flow):
 x=.85+i*2.48;rect(s,x,2.15,2.02,1.25,BLUE,BLUE);textbox(s,x+.1,2.3,.38,.5,n,22,CYAN,True,PP_ALIGN.CENTER);textbox(s,x+.48,2.42,1.4,.55,t,13,WHITE,True,PP_ALIGN.CENTER)
 if i<4:line(s,x+2.04,2.77,x+2.39,2.77,GOLD,2,end=True)
card(s,1.0,4.15,5.3,1.28,'网络驱动合并','CombineDrivers 汇总 High / Low 驱动；没有可用驱动时网络回到 Unknown。',TEAL,14)
card(s,6.95,4.15,5.3,1.28,'终止条件','本轮网络状态不再变化即收敛；达到最大轮次仍变化则标记 oscillation。',GOLD,14)
textbox(s,1.0,5.85,11,.55,'UI 触发输入电平或元件状态更新后重新 Evaluate，并按 net / pin / wire 查询结果。',14,TEAL,True)

# 12 project serialization
s=base('工程 JSON v3：保存可编辑状态，而非画布截图','读取时先暂存和校验，随后映射到新的 Circuit 对象')
rect(s,.9,1.6,3.25,3.8,WHITE,BORDER);textbox(s,1.12,1.82,2.8,.38,'JSON 顶层',17,NAVY,True);textbox(s,1.12,2.38,2.75,2.7,'format: MyWx111-Circuit\nversion: 3\ncomponents: [...]\nwires: [...]\nbends: [...]',15,INK)
line(s,4.18,3.45,5.2,3.45,GOLD,3,end=True)
rect(s,5.25,1.6,3.15,3.8,PALETEAL,TEAL);textbox(s,5.5,1.85,2.65,.38,'Temp 数据 + 校验',17,NAVY,True);textbox(s,5.5,2.42,2.6,2.45,'检查 type / id\n读取 pin refs\n恢复折点 bends\n应用默认字段\n全部通过再提交',15,INK)
line(s,8.42,3.45,9.35,3.45,GOLD,3,end=True)
rect(s,9.4,1.6,2.95,3.8,WHITE,BLUE);textbox(s,9.62,1.85,2.5,.38,'Circuit',17,NAVY,True);textbox(s,9.62,2.42,2.5,2.45,'创建元件\n重建连线\n恢复物理属性\n恢复输入/时钟字段\n刷新仿真拓扑',15,INK)
textbox(s,1,5.88,11.4,.55,'版本演进：v2 加入 refdes / footprint / value / slot；v3 增加输入电平、时钟频率与占空比字段。',14,TEAL,True)

# 13 property persistence
s=base('属性如何穿过 UI、模型、JSON 与 PCB 导出','以 U1 / 7486 / DIP14 / slot 2 为例追踪同一字段')
cols=[('属性面板','输入 value=7486\nfootprint=DIP14\nrefDes=U1\nslot=2'),('Circuit / Component','SetComponentValue\nSetComponentFootprint\nSetComponentRefDes\nSetComponentSlot'),('ProjectSerializer','JSON 保存字段\n加载回 Component\n版本默认值兼容'),('gEDA / PCB 导出','ChipGroup 合并 U1\n门号保留 slot=2\n映射 DIP14 封装')]
for i,(h,b) in enumerate(cols):
 x=.75+i*3.18;rect(s,x,2.1,2.78,2.2,WHITE,TEAL);textbox(s,x+.12,2.34,2.54,.34,h,15,NAVY,True,PP_ALIGN.CENTER);textbox(s,x+.16,2.92,2.45,1.13,b,13,INK,False,PP_ALIGN.CENTER)
 if i<3:line(s,x+2.8,3.2,x+3.12,3.2,GOLD,2,end=True)
card(s,1.0,4.9,5.2,1.15,'逻辑属性','name / orientation / bitWidth：控制编辑与模型语义',BLUE,14)
card(s,6.95,4.9,5.2,1.15,'物理属性','refDes / value / footprint / slot：连接芯片封装和 PCB 交付',GOLD,14)

# 14 IO exports
s=base('导出不是“另存为”：每个目标格式有独立适配器','统一读取 Circuit；各导出器负责不同下游语法')
for i,(h,b) in enumerate([('ProjectSerializer','可继续编辑的工程 JSON\n保存组件、wire、bends 与属性'),('NetlistExporter','通用连接关系\n供检查逻辑网络与器件引脚'),('GedaNetlistExporter','gEDA pcb 行式 .net\n输出器件 / 网络项'),('PcbFileExporter','生成 .pcb 初始布局\n写盘前 ValidatePcbText 自检')]):
 x=.78+i*3.15;card(s,x,1.8,2.8,2.2,h,b,[TEAL,BLUE,GOLD,TEAL][i],14)
 if i<3:line(s,x+2.82,2.9,x+3.1,2.9,BLUE,2,end=True)
textbox(s,1.05,4.55,11.2,.7,'典型链路：编辑电路 → 保存工程 JSON → 导出 gEDA 网表与 pcb 文件 → 在 PCB 工具中载入布局数据。',17,NAVY,True,PP_ALIGN.CENTER)
card(s,1.25,5.55,10.8,.8,'格式校验','PCB 文本检查括号配对和必需标记 FileVersion / PCB / Mark，避免明显结构错误写盘。',GOLD,14)

# 15 engineering design
s=base('软件工程落点：把变化限制在稳定边界内','从源码实现抽取出的三条工程设计决策')
card(s,.85,1.65,3.75,3.2,'扩展元件','Component 抽象接口\n• BuildPins：引脚布局\n• GetLocalBounds：命中边界\n• DescribeSymbol：几何描述\n• Evaluate：组合逻辑求值\nComponentFactory 集中创建类型。',TEAL,15)
card(s,4.8,1.65,3.75,3.2,'分离电气与图形','PinDirection：Input / Output / Bidi\nPinSide：Left / Right / Top / Bottom\n前者用于连线和仿真；后者用于计算引出方向。\nSymbolGeometry 与 Renderer 分层。',BLUE,15)
card(s,8.75,1.65,3.75,3.2,'集中变更路径','Circuit 提供属性修改入口；UndoStack 保存编辑历史。\n序列化只处理数据格式；导出器只处理目标格式。\n避免画布、模型和导出之间出现多份状态。',GOLD,15)
textbox(s,1,5.55,11,.55,'结果：增加元件类型时可复用画布、属性、仿真和导出主干；数据变化由 Circuit 统一管理。',15,TEAL,True)

# 16 five people
s=base('五人协作：按可交付模块拆分工作','建议按源码模块认领责任，再通过 Circuit 接口集成')
roles=[('成员 1','模型与数据','Component / Pin / Wire / Circuit / ComponentFactory'),('成员 2','画布交互','CircuitCanvas / GridTransform / Renderer / 编辑事件'),('成员 3','仿真','Simulator / 网络构建 / 逻辑传播 / 输出查询'),('成员 4','文件互操作','ProjectSerializer / Netlist / gEDA / PCB 导出'),('成员 5','集成与质量','PropertiesPanel / UndoStack / 场景联调 / 文档答辩')]
for i,(m,h,b) in enumerate(roles):
 y=1.4+i*1.03;rect(s,.95,y,1.55,.73,BLUE,BLUE);textbox(s,1.03,y+.18,1.4,.32,m,15,WHITE,True,PP_ALIGN.CENTER);rect(s,2.8,y,2.3,.73,PALETEAL,TEAL);textbox(s,2.92,y+.18,2.05,.32,h,15,NAVY,True,PP_ALIGN.CENTER);rect(s,5.45,y,6.9,.73,WHITE,BORDER);textbox(s,5.66,y+.17,6.48,.38,b,14,INK)

# 17 closing
s=base('总结：一个电路模型，连接编辑、验证与制造数据','MyWx111 当前代码的核心价值')
for i,(h,b) in enumerate([('可编辑','引脚引用、变换后几何、自动 / 手动正交布线'),('可验证','Union-Find 构网、Evaluate 迭代、稳定状态查询'),('可保存','JSON v3 保留拓扑、折点和芯片属性'),('可互操作','通用网表、gEDA 网表、PCB 初始布局')]):
 x=.82+i*3.15;card(s,x,2.0,2.8,2.25,h,b,[TEAL,BLUE,GOLD,TEAL][i],15)
textbox(s,1.0,5.25,11.2,.85,'感谢聆听\n欢迎提问',26,NAVY,True,PP_ALIGN.CENTER)

prs.save(OUT)
print(str(OUT), 'slides=',len(prs.slides))


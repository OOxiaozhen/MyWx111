import zipfile, os, html, re
from pathlib import Path
out=Path(r'F:\综设\MyWx111\MyWx111_EDA答辩.pptx')
slides=[]
# Slide content each: title, subtitle, cards[(heading,body)]
data=[
('从逻辑表达走向可制造数据','MyWx111 电路原理图编辑与仿真平台｜进阶式挑战性综合项目 I',[('课题方向','工业软件 / EDA 原理图编辑与逻辑仿真'),('项目目标','以同一电路模型贯通交互编辑、逻辑验证、工程存储与下游导出'),('小组答辩','五人团队｜软件工程 × 数字电路 × 图形交互')]),
('课题任务拆解：五模块闭环','任务书要求映射到已实现的工程模块',[('用户界面','电路画布、工具栏、属性面板与事件交互'),('元件模块','门电路元件、引脚定义、工厂创建与符号几何'),('编辑模块','放置、选择、拖动、旋转、布线、撤销重做'),('文件模块','工程序列化；通用网表、gEDA 网表与 PCB 文件导出'),('仿真模块','逻辑电平传播、迭代收敛与振荡识别')]),
('系统架构：模型是单一事实源','按职责分层，数据从交互流向验证与制造',[('View｜视图','CircuitCanvas · ComponentRenderer · GridTransform'),('UI｜交互','MainFrame · ToolPalette · PropertiesPanel'),('Model｜领域','Circuit · Component · Pin · Wire · UndoStack'),('Sim｜验证','Simulator：网络构建、逻辑传播、收敛判断'),('IO｜互操作','ProjectSerializer · NetlistExporter · GedaNetlistExporter · PcbFileExporter')]),
('对象模型：从元件到网络','稳定 ID 与引脚引用表达电气连接，不依赖屏幕坐标',[('Component','id / type / name / position / orientation / bitWidth'),('Pin','index + 局部 offset + Input / Output / Bidi 电气方向'),('Wire','from(componentId,pinIndex) → to(componentId,pinIndex)；可保存 bends'),('Circuit','集中维护元件、连线与修改接口'),('关键取舍','视图几何与电气拓扑分离，导出复用同一语义模型')]),
('布线逻辑：严格正交自动路由','实例：两个水平引脚错位，自动得到 Z 形路径',[('① 引出','依据 PinSide 求方向，两端延伸 20 世界单位，避免贴着符号拐弯'),('② 选型','两端水平→中点竖线构成 Z；两端垂直→中点横线；一横一竖→L 形'),('③ 清理','AppendOrthogonalPoint 合并重合点与三点共线段'),('④ 手动折点','AppendOrthogonalSegment 将斜向端段拆成 L 形，保持路径横平竖直'),('性质','路径相邻点必有 x 相等或 y 相等，可读、可编辑、可序列化')]),
('编辑交互：事件变更模型','一次拖动如何从鼠标动作变为可保存的工程状态',[('命中与选择','世界坐标命中元件包围盒（含引脚外扩），更新当前选择'),('拖动与网格','GridTransform 在屏幕坐标与世界坐标之间转换，位置吸附网格'),('旋转','orientation 归一到 0 / 90 / 180 / 270°，引脚同步变换'),('连线','以元件 ID 和引脚 index 绑定端点，移动后仍可追踪'),('撤销重做','UndoStack 管理可逆操作，形成可恢复的编辑历史')]),
('元件属性：逻辑与物理映射','示例：74x86 四个 XOR 门组成一颗 DIP14 芯片',[('通用属性','名称、位置、朝向、位宽（SetBitWidth 钳制 1–32）'),('芯片属性','refDes=U1；value=7486；footprint=DIP14；slot=1..4'),('建模规则','同芯片多个门共享 refDes，以 slot 区分门号'),('导出意义','导出器据 refDes 合并物理器件，按 slot / 引脚映射生成 PCB 数据'),('自动默认','空编号、型号或封装允许导出层自动分配 / 按类型映射')]),
('仿真器：构网后迭代传播','案例：输入 A、B → AND → LED；A=B=1 时输出为 1',[('构网','Wire 端点关系建立 pin→net 与 wire→net 索引'),('求值','按元件输入引脚收集 LogicLevel，调用 Evaluate(inputs)'),('传播','迭代更新网络值，直至稳定或达到最大轮数'),('真值例','AND：0&0=0；0&1=0；1&0=0；1&1=1'),('异常识别','迭代耗尽标记 oscillation，暴露组合环路或自反馈')]),
('工程文件：版本演进与加载安全','MyWx111-Circuit 格式当前版本 v3',[('v1 → v2','增加 refdes / footprint / value / slot 芯片物理属性'),('v2 → v3','增加输入引脚电平、时钟频率与占空比'),('安全加载','先解析到 TempComponent / TempWire，校验通过后才写入 Circuit'),('前向兼容','未知 JSON 键由 SkipValue 跳过；缺失字段使用默认值'),('工程收益','加载失败不污染当前电路；版本边界清楚，利于演进')]),
('互操作：原理图连到下游工具','IO 层隔离格式语法，Model 保持电路语义',[('通用网表','NetlistExporter 输出基础连接描述'),('gEDA','GedaNetlistExporter 与 GedaFootprints 提供兼容网表和封装映射'),('PCB 文件','PcbFileExporter 将器件与网络转为 PCB 侧可消费数据'),('具体例子','两个 7486 门 → 共享 U1、slot1/slot2 → 映射同一芯片封装'),('边界价值','网表 / 封装 / PCB 转换集中在 IO，避免污染交互与仿真模块')]),
('工程设计：扩展性与边界意识','用明确抽象承载复杂度，用验证计划补齐风险',[('可扩展元件','Component 抽象钩子：BuildPins / GetLocalBounds / DescribeSymbol / Evaluate'),('绘制解耦','模型给 SymbolGeometry，Renderer 专职抗锯齿绘制'),('语义清晰','PinDirection（电气）与 PinSide（几何）分离'),('已知边界','当前多输出元件不参与仿真；复杂布局与时序仿真未覆盖'),('质量路线','增加路径性质、真值表、格式兼容、导出一致性自动化用例')]),
('多方案比较：选择来自约束','对应任务书的分析论证与方案优选要求',[('布线','直线 / 固定 L / 引脚方向正交路由 → 选正交：易读、兼容多朝向、支持手动折点'),('文件','画布快照 / 结构化语义文件 → 选语义数据：能继续编辑、仿真、转换'),('绘制','模型直接绘图 / 几何描述 + Renderer → 选分层：后端可替换，模型易测'),('仿真','单遍求值 / 网络迭代收敛 → 选迭代：传播稳定值并识别不收敛'),('结论','对照需求、复杂度、可扩展性，再用场景验证方案')]),
('五人小组：模块边界并行协作','请将“成员 1–5”替换为真实姓名',[('成员 1｜模型架构','Circuit / Component / Pin / Wire / 工厂与接口'),('成员 2｜画布交互','绘制、坐标变换、选择、拖动、旋转与连线'),('成员 3｜仿真逻辑','网络构建、逻辑真值、未知态、收敛与振荡'),('成员 4｜文件互操作','工程读写、版本兼容、网表 / gEDA / PCB 导出'),('成员 5｜集成质量','属性面板、撤销重做、集成验证、文档与答辩')]),
('验证矩阵：用案例检查闭环','建议答辩演示：创建 → 连接 → 仿真 → 保存 → 导出',[('几何用例','水平 / 垂直 / 混合朝向；验证所有线段正交与折点去冗余'),('属性用例','位宽边界、朝向归一化、同 refDes 多 slot 器件合并'),('仿真用例','AND 四组输入、未知值传播、反馈回路不收敛'),('文件用例','保存→清空→加载，核对折点、属性与时钟配置'),('导出用例','对照网表、gEDA 与 PCB 文件中的器件和网络映射')]),
('总结：从教学项目走向 EDA 工具链','一套一致的数据模型连接编辑、验证与物理实现',[('任务覆盖','图编辑、元件属性、工程管理、网表 / PCB 导出、动态逻辑仿真'),('实现亮点','正交布线算法、芯片 refDes/slot 属性、网络级逻辑传播'),('软件工程','分层解耦、显式数据结构、版本化文件、格式适配边界'),('下一步','补齐自动化验证、复杂布线规则、更多元件与时序能力'),('感谢聆听','欢迎提问')])]
# OOXML helpers, 16:9 13.333 x 7.5in, 914400 EMU/in
EMU=914400
def esc(s): return html.escape(str(s),quote=False)
def shape(id,name,x,y,w,h,fill,text='',size=1800,color='F2F6FA',bold=False,align='l'):
    x=int(x*EMU);y=int(y*EMU);w=int(w*EMU);h=int(h*EMU)
    tx=''
    if text:
        paras=[]
        for i,line in enumerate(str(text).split('\n')):
            paras.append(f'<a:p><a:pPr algn="{align}"/><a:r><a:rPr lang="zh-CN" sz="{size}" b="{1 if bold else 0}"><a:solidFill><a:srgbClr val="{color}"/></a:solidFill><a:latin typeface="Microsoft YaHei"/><a:ea typeface="Microsoft YaHei"/></a:rPr><a:t xml:space="preserve">{esc(line)}</a:t></a:r><a:endParaRPr lang="zh-CN" sz="{size}"/></a:p>')
        tx='<p:txBody><a:bodyPr wrap="square" lIns="140000" tIns="100000" rIns="140000" bIns="70000" anchor="ctr"/><a:lstStyle/>'+''.join(paras)+'</p:txBody>'
    return f'<p:sp><p:nvSpPr><p:cNvPr id="{id}" name="{esc(name)}"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr><p:spPr><a:xfrm><a:off x="{x}" y="{y}"/><a:ext cx="{w}" cy="{h}"/></a:xfrm><a:prstGeom prst="roundRect"><a:avLst/></a:prstGeom><a:solidFill><a:srgbClr val="{fill}"/></a:solidFill><a:ln w="12000"><a:solidFill><a:srgbClr val="35536B"/></a:solidFill></a:ln></p:spPr>{tx}</p:sp>'
def mk_slide(title,sub,cards,n):
    shapes=[]; i=2
    shapes.append(shape(i,'Title',.58,.28,12.1,.58,'172538',title,2500,'FFFFFF',True));i+=1
    shapes.append(shape(i,'Subtitle',.62,.91,12.0,.36,'172538',sub,1150,'66D9C8'));i+=1
    count=len(cards); gap=.16; left=.62; usable=12.08
    if count>=4:
        cols=2; rows=(count+1)//2; cw=(usable-gap)/2; ch= (5.55-(rows-1)*gap)/rows
    else:
        cols=1; rows=count; cw=usable; ch=(5.55-(rows-1)*gap)/rows
    for j,(head,body) in enumerate(cards):
        col=j%cols; row=j//cols; x=left+col*(cw+gap); y=1.47+row*(ch+gap)
        bodytext=head+'\n'+body
        shapes.append(shape(i,'Card'+str(j),x,y,cw,ch,'263B52',bodytext,1450,'EAF2F7',False));i+=1
    shapes.append(shape(i,'Footer',.62,7.16,12.1,.18,'172538',f'MyWx111  ·  EDA 项目答辩  ·  {n:02d}',850,'8296AA'))
    bg='<p:bg><p:bgPr><a:solidFill><a:srgbClr val="172538"/></a:solidFill><a:effectLst/></p:bgPr></p:bg>'
    tree='<p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>'+''.join(shapes)+'</p:spTree>'
    return f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?><p:sld xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main">{bg}{tree}<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>'
N=len(data)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml','<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/><Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/><Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>'+''.join(f'<Override PartName="/ppt/slides/slide{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>' for i in range(1,N+1))+'</Types>')
    z.writestr('_rels/.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="ppt/presentation.xml"/></Relationships>')
    z.writestr('ppt/presentation.xml','<?xml version="1.0" encoding="UTF-8"?><p:presentation xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst><p:sldIdLst>'+''.join(f'<p:sldId id="{256+i}" r:id="rId{i+1}"/>' for i in range(1,N+1))+'</p:sldIdLst><p:sldSz cx="12192000" cy="6858000" type="screen16x9"/><p:notesSz cx="6858000" cy="9144000"/></p:presentation>')
    z.writestr('ppt/_rels/presentation.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>'+''.join(f'<Relationship Id="rId{i+1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide{i}.xml"/>' for i in range(1,N+1))+'</Relationships>')
    z.writestr('ppt/slideMasters/slideMaster1.xml','<?xml version="1.0" encoding="UTF-8"?><p:sldMaster xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld><p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/><p:sldLayoutIdLst><p:sldLayoutId id="1" r:id="rId1"/></p:sldLayoutIdLst></p:sldMaster>')
    z.writestr('ppt/slideMasters/_rels/slideMaster1.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/></Relationships>')
    z.writestr('ppt/slideLayouts/slideLayout1.xml','<?xml version="1.0" encoding="UTF-8"?><p:sldLayout xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" type="blank" preserve="1"><p:cSld name="Blank"><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr></p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>')
    z.writestr('ppt/slideLayouts/_rels/slideLayout1.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="../slideMasters/slideMaster1.xml"/></Relationships>')
    for i,(t,s,c) in enumerate(data,1):
        z.writestr(f'ppt/slides/slide{i}.xml',mk_slide(t,s,c,i))
        z.writestr(f'ppt/slides/_rels/slide{i}.xml.rels','<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/></Relationships>')
print(out, out.stat().st_size, 'slides',N)

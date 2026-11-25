import json
from pathlib import Path
root=Path(r'C:\Users\liyizhen\.codex\skills\GordenPPTSkill')
template=root/'templates'/'architecture-deck'
d=json.load(open(template/'detail.json',encoding='utf-8'))
selected=[2,4,5,7,11,12,15]
content={
2:['需求分析','模型设计','元件库','原理图编辑','仿真验证','工程保存','网表导出','PCB 数据','需求拆解','数据结构','引脚定义','正交布线','逻辑传播','版本化文件','格式适配','需求→模型','模型→画布','连线→网络','网络→输出','保存→重载','原理图→网表','输入','处理','输出','创建','编辑','验证','交付','对象模型','交互编辑','仿真求值','文件读写','成果汇总','阶段闭环','工程闭环'],
4:['用户界面','工具栏','属性面板','电路画布','鼠标事件','元件模型','Component','Pin','Wire','Circuit','元件库','输入输出','逻辑门','分线器','门电路','输入脚','输出脚','引脚序号','连线端点','网络关系','编辑功能','放置','选择','拖动','旋转','布线','撤销','重做','保存','加载','仿真器','网表导出','gEDA 导出','PCB 文件','模块职责清晰','数据语义统一','id + pinIndex','模型驱动视图','输入驱动','网络求值','导出映射','交互 → 模型','模型 → 仿真','模型 → IO','扩展元件类型'],
5:['数据模型','引脚布局','正交路由','路径简化','逻辑仿真','工程序列化','格式导出','端点引用','世界坐标','PinSide','引出段 20','L / Z 形','共线合并','手动折点','BuildPins','Evaluate','LogicLevel','Unknown','pin→net','Union-Find','JSON v3','暂存校验','netlist','gEDA .net','pcb .pcb','位置','朝向 0/90/180/270','位宽 1–32','芯片编号','封装','型号','slot 门号','网络状态','驱动合并','迭代收敛','输入电平','时钟参数','引用 ID','类型工厂'],
7:['电路对象','创建元件','生成引脚','属性写入','画布渲染','位置变换','命中选择','拖动更新','旋转变换','正交走线','保存 JSON','加载校验','构建网络','合并节点','调用 Evaluate','传播电平','稳定判断','输出探针','设置输入','InputPin','AndGate','XorGate','Splitter','LedIndicator','LogicProbe','TextLabel','Vcc / Gnd','refDes=U1','value=7486','DIP14','slot=1..4','网表 T 行','封装映射','PCB 初始布局','UndoStack'],
11:['逻辑输入','Circuit','元件引脚','连线端点','网络构建','并查集 Union','输入网络','门电路求值','输出驱动','驱动合并','网络状态','未知态','稳定收敛','反馈检测','LogicLevel','InputPin','AndGate','OrGate','NotGate','NandGate','NorGate','XorGate','XNOR','Buffer','TriState','LED','LogicProbe','Splitter 透明导通','pin→net','wire→net'],
12:['工程编辑闭环','放置元件','连接引脚','生成折线路径','设置输入电平','仿真传播','观察输出','修改属性','记录撤销栈','保存项目','解析暂存','校验数据','恢复电路','导出网表','封装映射','PCB 文件','文件标记','MyWx111-Circuit','组件数组','连线数组','bends 折点','refDes / slot'],
15:['项目生命周期','创建工程','添加元件','构建连接','调整属性','仿真验证','保存版本','导出交换','工程创建','对象建模','界面编辑','网络分析','结果观察','工程持久化','制造数据','UI 层','Model 层','View 层','Sim 层','IO 层','元件类型','引脚定义','电气连接','几何路径','逻辑状态','JSON v3','通用网表','gEDA 网表','PCB 初始布局','工程闭环']}
edits=[]
for sn in selected:
 page=d['pages'][sn-1]; vals=content[sn]; k=0
 for slot in page['text_slots']:
  if not slot.get('editable',True): continue
  role=slot.get('role','')
  if '装饰' in role or '序号' in role or role in ['英文标签','日期']:
   # Keep template numbers and fixed labels, but avoid original business-specific labels where editable.
   if '装饰' in role or '序号' in role: continue
  value=vals[k%len(vals)]; k+=1
  # Ensure explicit main title / body slots carry slide-specific concepts where detected.
  if slot['slot_id']==page.get('title_slot_id'):
   value={2:'从需求到工程交付',4:'MyWx111 模块架构',5:'核心数据模型与能力',7:'编辑仿真工作流',11:'逻辑网络求值流程',12:'电路工程闭环',15:'项目能力全景'}[sn]
  edits.append({'slide':sn,'slot_id':slot['slot_id'],'new_text':value})
obj={'template_slug':'architecture-deck','selected_slides':selected,'edits':edits}
Path(r'F:\综设\MyWx111\eda_defense_edits.json').write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8')
print('edits',len(edits),'slides',selected)

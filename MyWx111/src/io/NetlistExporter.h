#pragma once

#include <utility>
#include <vector>

#include <wx/string.h>

class Circuit; // 前置声明：网表层只需引用模型，不在头文件引入 Circuit.h，减少编译耦合

/// 网表导出层。
/// 当前仅支持 KiCad 6+ 的 S-expression 网表（.net）格式，
/// 与 model / view / ui 平级，只依赖模型层的只读接口。
namespace netlist
{
/// 引脚唯一键：元件 id + 元件内引脚索引。
struct PinKey
{
    int componentId = -1; ///< 引脚所属元件 id
    int pinIndex = -1;    ///< 引脚在元件内的索引
};

/// @param a 第一个引脚键
/// @param b 第二个引脚键
/// @return a 是否按字典序小于 b
bool PinKeyLess(const PinKey& a, const PinKey& b);

/// 通过并查集把连线划分成若干网络。
/// 以 Circuit::GetWires() 为唯一连通性真相源；Splitter 的所有引脚在此阶段被合并，
/// 从而使网络穿透该元件；孤立引脚输出为单节点网络。
/// @param circuit 只读电路模型
/// @return 每个元素是一个网络的引脚列表；网络之间按「最小引脚键」稳定排序
std::vector<std::vector<PinKey>> BuildNets(const Circuit& circuit);

/// 将电路导出为 KiCad 6+ S-expression 网表文件。
/// @param circuit 待导出的电路，仅读取，不修改其任何状态
/// @param path    目标文件的完整路径（Unicode 路径由 wxFileOutputStream 处理）
/// @param error   失败时写入中文错误描述；另有「已导出但存在告警」时写入告警文本；
///                允许传 nullptr 表示调用方不关心文本
/// @return true 表示文件已成功写出（即使 error 非空也可能为 true，此时 error 内容为告警）；
///         false 表示未产生可用文件（I/O 失败、路径不可写等）
/// @throws 不抛异常，内部所有错误均转换为 false + error
bool ExportKiCadNetlist(const Circuit& circuit, const wxString& path, wxString* error);
} // namespace netlist

#pragma once

#include <wx/string.h>

class Circuit; // 前置声明：序列化层只依赖模型，不在头文件引入 Circuit.h

/// 工程文件（JSON）序列化层。
/// 与网表导出并列：网表用于交给 PCB 工具消费，工程文件用于保存后可重新打开继续编辑。
/// 只依赖模型层的公开接口，不感知视图与界面。
namespace project
{
/// 将电路序列化为内存 JSON 文本，供撤销快照等内存场景使用，不触碰磁盘。
/// @param circuit 待序列化的电路，只读
/// @param out 成功时写入 JSON 文本；调用方无需预先清空
/// @param error 预留的失败描述出口；当前构造过程不会失败，可为 nullptr
/// @return 成功返回 true
/// @throws 不抛异常
bool SerializeCircuitToJson(const Circuit& circuit, wxString& out, wxString* error = nullptr);

/// 从内存 JSON 文本重建电路内容，解析全部通过后才改动 circuit。
/// @note 内部会先 Clear 再重建，因此调用方若关心变更通知，
///       应在外层用 Circuit::SuppressScope 抑制，避免回放过程反向触发各类回调。
/// @note 重建时元件 id 会重新分配，函数内部通过「旧 id → 新 id」映射保证连线指向正确，
///       因此同一份快照内部始终自洽。
/// @param circuit 目标电路
/// @param json JSON 文本
/// @param error 失败时写入中文错误描述；成功但有告警（如个别连线被跳过）时写入告警文本；可为 nullptr
/// @return 成功返回 true；JSON 语法错误或格式标识/版本不匹配时返回 false
/// @throws 不抛异常
bool DeserializeCircuitFromJson(Circuit& circuit, const wxString& json,
                                wxString* error = nullptr);

/// 将电路保存为工程文件（JSON）。
/// 不保存画布视图状态（缩放、平移），仅保存可重建电路所需的元件与连线数据。
/// @param circuit 待保存的电路，仅读取，不修改其状态
/// @param path 目标文件完整路径
/// @param error 失败时写入中文错误描述；可为 nullptr
/// @return 成功写出返回 true
/// @throws 不抛异常，内部错误均转换为 false + error
bool SaveProjectJson(const Circuit& circuit, const wxString& path, wxString* error);

/// 从工程文件加载电路，成功时覆盖 circuit 的现有内容。
/// 解析阶段全部通过后才改动 circuit，因此解析失败时原电路保持完好。
/// @param circuit 目标电路
/// @param path 工程文件完整路径
/// @param error 失败时写入中文错误描述；加载成功但存在告警（如连线端点丢失）时写入告警文本；可为 nullptr
/// @return 成功加载返回 true
/// @throws 不抛异常，内部错误均转换为 false + error
bool LoadProjectJson(Circuit& circuit, const wxString& path, wxString* error);
} // namespace project

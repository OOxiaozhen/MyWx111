#pragma once

#include <wx/string.h>

class Circuit;

/// pcb（gEDA 布局软件）初始布局文件（.pcb）导出。
namespace geda
{
/// 把电路导出为 pcb 可加载的初始布局文件。
/// 文件可经 pcb 的 File → Load layout data to buffer 载入。
/// @param circuit 只读电路模型
/// @param path 目标文件完整路径
/// @param error 失败时写入中文错误描述；可为 nullptr
/// @return true 表示已成功写盘
/// @throws 不抛异常，内部错误均转为 false + error
bool ExportPcbFile(const Circuit& circuit, const wxString& path, wxString* error);

/// 生成 .pcb 文本。
/// 生成后会先经 ValidatePcbText 自检，不通过时返回空字符串。
/// @param circuit 只读电路模型
/// @param error 失败时写入中文错误描述；可为 nullptr
/// @return 完整的 .pcb 文本；失败时返回空字符串
wxString BuildPcbText(const Circuit& circuit, wxString* error);

/// 对生成的 .pcb 文本做结构自检。
/// 检查方括号/圆括号是否配平、必需记号（FileVersion / PCB / Mark）是否存在。
/// pcb 的解析器对括号不配对会直接失败，因此在写盘前拦下比其他什么都不做更可靠。
/// @param text 待校验文本
/// @param error 失败时写入具体原因的中文描述；可为 nullptr
/// @return true 表示通过自检
bool ValidatePcbText(const wxString& text, wxString* error);
} // namespace geda

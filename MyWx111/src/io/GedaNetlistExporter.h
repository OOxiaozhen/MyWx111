#pragma once

#include <wx/string.h>

class Circuit;

/// gEDA pcb 行式网表（.net）导出。
namespace geda
{
/// 导出行为选项。
struct NetlistOptions
{
    /// T 行是否写出 value 字段。
    /// 真值为「gnetlist pcb 后端是否输出第四列」；不同后端版本存在差异，
    /// 故做成开关，实测不符时只需翻转默认值，不必改结构。
    bool includeValueOnTLine = true;

    /// 头部 release 注释的版本标记
    wxString releaseTag = L"pcb 20140316";

    /// 是否在末尾追加 "# End of netlist" 收尾注释
    bool appendEndMarker = true;
};

/// 把电路导出为 gEDA pcb 网表文件。
/// @param circuit 只读电路模型
/// @param path 目标文件完整路径
/// @param error 失败时写入中文错误描述；可为 nullptr
/// @param options 导出选项
/// @return true 表示已成功写盘
/// @throws 不抛异常，内部错误均转为 false + error
bool ExportGedaNetlist(const Circuit& circuit, const wxString& path, wxString* error,
                       const NetlistOptions& options = {});

/// 生成网表文本（不落盘，便于自检与预览）。
/// @param circuit 只读电路模型
/// @param error 失败时写入中文错误描述；可为 nullptr
/// @param options 导出选项
/// @return 完整的 .net 文本；失败时返回空字符串且 error 非空
wxString BuildGedaNetlistText(const Circuit& circuit, wxString* error,
                              const NetlistOptions& options = {});
} // namespace geda

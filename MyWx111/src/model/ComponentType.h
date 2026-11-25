#pragma once

#include <vector>

#include <wx/string.h>

/// 电路支持的元件类型。
/// 枚举顺序即序列化契约：工程文件按整数值存储 type，只允许在末尾追加新值，
/// 禁止插入或删除中间项，否则旧文件中的类型会整体错位。
enum class ComponentType
{
    InputPin,  ///< 输入引脚（电路输入端）
    OutputPin, ///< 输出引脚（电路输出端）
    AndGate,   ///< 与门
    OrGate,    ///< 或门
    NotGate,   ///< 非门
    NandGate,  ///< 与非门
    NorGate,   ///< 或非门
    XorGate,   ///< 异或门
    Splitter,  ///< 分线器

    // ── 以下为仿真扩展新增。仅可继续在末尾追加：序列化按整数值存储，
    //    一旦插入中间位置，旧工程文件中的 type 会整体错位 ──
    TextLabel,    ///< 文本标注（纯注释，无引脚，不参与仿真）
    VccSource,    ///< 已废弃：由 LevelSource 取代；保留原值仅供旧文件兼容，不可创建
    GndSource,    ///< 已废弃：由 LevelSource 取代；保留原值仅供旧文件兼容，不可创建
    ClockSource,  ///< 时钟信号源（本次不实现翻转行为）
    XnorGate,     ///< 同或门
    BufferGate,   ///< 缓冲器
    TriStateGate, ///< 三态门
    DFlipFlop,    ///< 已废弃：时序分组已裁剪，不可创建
    RsLatch,      ///< 已废弃：时序分组已裁剪，不可创建
    LedIndicator, ///< LED 指示灯
    LogicProbe,   ///< 逻辑探针
    LevelSource   ///< 统一电平源：高/低由属性面板切换，取代 VccSource/GndSource
};

/// 取元件类型的中文显示名，用于左侧调色板与属性面板。
/// @param type 元件类型
/// @return 显示名字符串
wxString GetComponentDisplayName(ComponentType type);

/// 取元件类型的默认位宽。
/// @param type 元件类型
/// @return 新建元件时的初始位宽
int GetComponentDefaultBitWidth(ComponentType type);

/// 取元件类型所属的调色板分组名。
/// @param type 元件类型
/// @return 分组名（「输入/输出」「逻辑门」「接线」之一）
wxString GetComponentGroupName(ComponentType type);

/// 取全部调色板分组名，按界面展示顺序排列。
/// @return 分组名列表
std::vector<wxString> GetAllComponentGroups();

/// 取指定分组下的全部元件类型，按界面展示顺序排列。
/// @param groupName 分组名
/// @return 该分组下的元件类型列表；分组名不存在时返回空列表
std::vector<ComponentType> GetComponentTypesInGroup(const wxString& groupName);

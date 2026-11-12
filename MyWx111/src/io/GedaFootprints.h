#pragma once

#include <wx/string.h>

#include "../model/ComponentType.h"

/// gEDA / pcb 侧的封装名、器件型号与 DIP14 引脚号映射。
/// 本命名空间全部为无状态纯函数，便于在导出层与属性面板间共享。
namespace geda
{
/// 默认 DIP14 封装名（gEDA m4 风格，不带库前缀）
inline constexpr const wchar_t* kDip14Footprint = L"DIP14";
/// 单引脚连接器封装名
inline constexpr const wchar_t* kSinglePinFootprint = L"1X01";
/// DIP14 的引脚总数
inline constexpr int kDip14PinCount = 14;

/// 逻辑门在 DIP14 上的三端引脚号。
struct GatePinMap
{
    int inputA = -1; ///< 第一个输入脚（A）
    int inputB = -1; ///< 第二个输入脚（B）
    int output = -1; ///< 输出脚（Y）
};

/// 取某 slot 对应的 DIP14 引脚号。
/// 74xx 四门器件的标准排布：门1(1,2→3) 门2(4,5→6) 门3(9,10→8) 门4(12,13→11)；
/// 注意门 3、门 4 的输出脚号小于输入脚号，不能按线性公式推算。
/// @param slot 门序号，超出 1..4 时按 4 取模收敛
/// @return 该门的输入/输出引脚号
GatePinMap Dip14GatePins(int slot);

/// 把元件的逻辑引脚索引映射为实际封装上的引脚号。
/// 逻辑引脚索引由各元件的 BuildPins 决定：
/// 两输入门 0/1 为输入、2 为输出；非门 0 入 1 出；输入/输出引脚仅 0 号一脚。
/// @param type 元件类型
/// @param slot 门序号（1..4）
/// @param logicalPinIndex 元件内引脚索引
/// @return 物理引脚号；无法映射时返回 -1（调用方须视为导出错误）
int MapLogicalPinToDip14(ComponentType type, int slot, int logicalPinIndex);

/// 取元件类型的 gEDA 默认封装名。
/// @param type 元件类型
/// @return 封装名；Splitter 返回空字符串（不作为物理元件导出）
wxString DefaultFootprint(ComponentType type);

/// 取元件类型的 gEDA 默认器件型号（如 XorGate → "7486"）。
/// @param type 元件类型
/// @return 型号字符串；无对应型号时返回空字符串
wxString DefaultValue(ComponentType type);
} // namespace geda

#pragma once

#include <wx/string.h>

/// 单比特逻辑电平。
/// 本次仿真只做单比特：Component::bitWidth 字段保留，但仿真层恒按 1 位处理；
/// 多比特支持留待后续（届时改为 std::vector<LogicLevel> 或位掩码）。
enum class LogicLevel
{
    Low,     ///< 逻辑低
    High,    ///< 逻辑高
    Unknown  ///< 未知 / 高阻 / 振荡 / 未驱动
};

/// @param level 待转换的电平
/// @return 界面显示用单字符："1" / "0" / "?"
inline const wchar_t* LogicLevelToDisplayChar(LogicLevel level)
{
    switch (level)
    {
    case LogicLevel::High:
        return L"1";
    case LogicLevel::Low:
        return L"0";
    default:
        return L"?";
    }
}

/// @param level 待转换的电平
/// @return 界面显示用中文文本："高" / "低" / "未知"
inline wxString LogicLevelToText(LogicLevel level)
{
    switch (level)
    {
    case LogicLevel::High:
        return wxString(L"高");
    case LogicLevel::Low:
        return wxString(L"低");
    default:
        return wxString(L"未知");
    }
}

/// 逻辑非。Unknown 取反仍为 Unknown：
/// 三态门或未驱动网络不得被取反成一个看起来有效的值。
/// @param level 输入电平
/// @return 取反后的电平
inline LogicLevel LogicNot(LogicLevel level)
{
    switch (level)
    {
    case LogicLevel::High:
        return LogicLevel::Low;
    case LogicLevel::Low:
        return LogicLevel::High;
    default:
        return LogicLevel::Unknown;
    }
}

/// 三值逻辑与。任一为 Low 则 Low；无 Low 且全 High 才 High；否则 Unknown。
/// @param a 左操作数
/// @param b 右操作数
/// @return 与结果
inline LogicLevel LogicAnd(LogicLevel a, LogicLevel b)
{
    if (a == LogicLevel::Low || b == LogicLevel::Low)
    {
        return LogicLevel::Low;
    }
    if (a == LogicLevel::High && b == LogicLevel::High)
    {
        return LogicLevel::High;
    }
    return LogicLevel::Unknown;
}

/// 三值逻辑或。任一为 High 则 High；无 High 且全 Low 才 Low；否则 Unknown。
/// @param a 左操作数
/// @param b 右操作数
/// @return 或结果
inline LogicLevel LogicOr(LogicLevel a, LogicLevel b)
{
    if (a == LogicLevel::High || b == LogicLevel::High)
    {
        return LogicLevel::High;
    }
    if (a == LogicLevel::Low && b == LogicLevel::Low)
    {
        return LogicLevel::Low;
    }
    return LogicLevel::Unknown;
}

/// 三值逻辑异或。任一为 Unknown 则 Unknown，否则按布尔异或。
/// @param a 左操作数
/// @param b 右操作数
/// @return 异或结果
inline LogicLevel LogicXor(LogicLevel a, LogicLevel b)
{
    if (a == LogicLevel::Unknown || b == LogicLevel::Unknown)
    {
        return LogicLevel::Unknown;
    }
    const bool result = (a == LogicLevel::High) != (b == LogicLevel::High);
    return result ? LogicLevel::High : LogicLevel::Low;
}

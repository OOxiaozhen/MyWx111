#pragma once

#include <wx/gdicmn.h>

/// 引脚电气方向。
enum class PinDirection
{
    Input,  ///< 只能作为连线终点
    Output, ///< 只能作为连线起点
    Bidi    ///< 双向（当前版本暂未使用）
};

/// 元件引脚的纯数据描述。
/// 引脚不持有任何图形对象，坐标以「相对元件原点的本地偏移」保存，
/// 由 Component::GetTransformedPins 按元件朝向换算后再加元件世界坐标得到实际位置。
struct Pin
{
    int index = 0;                                ///< 引脚在所属元件内的序号，从 0 开始
    wxPoint offset;                               ///< 相对元件原点的本地坐标偏移（未应用朝向变换）
    PinDirection direction = PinDirection::Input;  ///< 引脚电气方向
    bool connected = false;                       ///< 是否已被某条连线占用
    int wireId = -1;                              ///< 占用的连线 id；-1 表示未连接

    /// 把引脚偏移换算到世界坐标。
    /// @param compPos 元件在世界坐标中的原点位置
    /// @return 引脚端点的世界坐标
    wxPoint GetWorldPos(const wxPoint& compPos) const { return compPos + offset; }
};

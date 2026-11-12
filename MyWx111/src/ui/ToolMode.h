#pragma once

#include "../model/ComponentType.h"

/// 画布交互模式。
/// 当前用轻量枚举做状态机；当工具数量继续增长时，
/// 可平滑替换为 ITool 策略类（每个工具封装自己的按下 / 移动 / 抬起处理）。
enum class ToolMode
{
    Select, ///< 选择：单击选中、拖拽移动、空白处框选
    Place,  ///< 放置：单击在吸附点创建 pendingType 元件
    Wire,   ///< 连线：从输出引脚拖到输入引脚
    Pan     ///< 平移：拖拽平移画布
};

/// 当前工具的完整上下文。
struct ToolContext
{
    ToolMode mode = ToolMode::Select;                   ///< 当前交互模式
    ComponentType pendingType = ComponentType::AndGate; ///< Place 模式下待放置的元件类型
};

#pragma once

#include <wx/gdicmn.h>
#include <wx/graphics.h>

#include "../model/Component.h"
#include "../model/LogicLevel.h"
#include "../model/Wire.h"
#include "GridTransform.h"

class Circuit;

/// 元件 / 引脚 / 连线 / 选中框 / 放置预览的绘制策略类。
/// 所有绘制都在屏幕坐标系下完成（先把世界坐标转屏幕坐标再绘制），
/// 不使用 gc->Scale()，以保证线宽与文字大小在任意缩放下都保持可读。
class ComponentRenderer
{
public:
    /// 绘制单个元件：符号主体 + 反相圆 + 引脚 + 名称，选中时叠加高亮外框。
    /// @param gc 图形上下文
    /// @param comp 待绘制元件
    /// @param transform 坐标变换
    /// @param selected 是否处于选中态
    /// @param inputLevel 该元件输入引脚的电平；仅 highlightLevel 为 true 时用于 LED/探针的着色与文字
    /// @param highlightLevel 是否按电平高亮（仿真开启时为 true）
    void DrawComponent(wxGraphicsContext& gc, const Component& comp,
                       const GridTransform& transform, bool selected,
                       LogicLevel inputLevel = LogicLevel::Unknown,
                       bool highlightLevel = false) const;

    /// 绘制单个引脚：本体边界到引脚端点的短线 + 端点小方块。
    /// @param gc 图形上下文
    /// @param pinWorld 引脚端点的世界坐标
    /// @param compBounds 所属元件的世界坐标包围盒（用于计算短线起点）
    /// @param transform 坐标变换
    /// @param highlight 是否高亮（连线工具悬停时使用）
    void DrawPin(wxGraphicsContext& gc, const wxPoint& pinWorld, const wxRect& compBounds,
                 const GridTransform& transform, bool highlight) const;

    /// 绘制一条连线。
    /// @param gc 图形上下文
    /// @param wire 待绘制连线
    /// @param circuit 用于解析两端引脚的世界坐标
    /// @param transform 坐标变换
    /// @param selected 是否选中（选中时加粗并变蓝）
    /// @param level 该连线所在网络的电平；仅 highlightLevel 为 true 时生效
    /// @param highlightLevel 是否按电平着色（仿真开启时为 true）
    void DrawWire(wxGraphicsContext& gc, const Wire& wire, const Circuit& circuit,
                  const GridTransform& transform, bool selected,
                  LogicLevel level = LogicLevel::Unknown, bool highlightLevel = false) const;

    /// 绘制框选矩形（虚线 + 半透明填充）。
    /// @param gc 图形上下文
    /// @param worldRect 世界坐标下的矩形
    /// @param transform 坐标变换
    void DrawSelectionRect(wxGraphicsContext& gc, const wxRect& worldRect,
                           const GridTransform& transform) const;

    /// 绘制全部画布注释（半透明底 + 多行文字），应绘制在元件与连线之上。
    /// @param gc        图形上下文
    /// @param circuit   电路模型（提供注释列表与选中态）
    /// @param transform 坐标变换（同时用于取当前缩放，按比例放大字号）
    void DrawNotes(wxGraphicsContext& gc, const Circuit& circuit,
                   const GridTransform& transform) const;

    /// 绘制连线工具进行中的临时线。
    /// 已确认的折点段画实线、仍在跟随鼠标的末段画虚线，便于用户区分。
    /// @param gc 图形上下文
    /// @param fromWorld 起点引脚世界坐标
    /// @param startSide 起点引脚所在侧（决定首段出线方向）
    /// @param confirmedPath 已由点击确认的折点（世界坐标），可为空
    /// @param toWorld 当前鼠标世界坐标
    /// @param transform 坐标变换
    void DrawPendingWire(wxGraphicsContext& gc, const wxPoint& fromWorld, PinSide startSide,
                         const std::vector<wxPoint>& confirmedPath, const wxPoint& toWorld,
                         const GridTransform& transform) const;

    /// 绘制放置预览（淡色符号 + 引脚），让用户确认落点。
    /// @param gc 图形上下文
    /// @param type 待放置的元件类型
    /// @param worldPos 预览位置（世界坐标）
    /// @param transform 坐标变换
    void DrawPlacementPreview(wxGraphicsContext& gc, ComponentType type,
                              const wxPoint& worldPos, const GridTransform& transform) const;

private:
    /// 在元件符号上绘制「电平徽标」：白底圆圈 + 圈内 1/0/? 文字。
    /// 输入引脚与电平源用它表示可点击切换的自身电平，
    /// 输出引脚用同一套样式只读显示上游网络电平，因此抽成公共辅助避免重复。
    /// @param gc 图形上下文
    /// @param comp 元件（提供位置与朝向）
    /// @param localCenter 徽标圆心（元件的本地未变换坐标）
    /// @param level 要显示的电平
    /// @param transform 坐标变换（同时用于按缩放调整半径）
    void DrawLevelBadge(wxGraphicsContext& gc, const Component& comp, const wxPoint& localCenter,
                        LogicLevel level, const GridTransform& transform) const;

    /// 绘制符号主体多边形与附加折线。
    /// @param gc 图形上下文
    /// @param comp 元件（提供位置与朝向）
    /// @param transform 坐标变换
    /// @param penColour 描边颜色
    /// @param fillColour 填充颜色
    void DrawBody(wxGraphicsContext& gc, const Component& comp,
                  const GridTransform& transform, const wxColour& penColour,
                  const wxColour& fillColour) const;

    /// 绘制反相小圆（与非门 / 或非门 / 非门的输出端标记）。
    /// @param gc 图形上下文
    /// @param geo 符号几何描述
    /// @param comp 元件（提供位置与朝向）
    /// @param transform 坐标变换
    /// @param fillColour 填充颜色
    void DrawInversionBubble(wxGraphicsContext& gc, const SymbolGeometry& geo,
                             const Component& comp, const GridTransform& transform,
                             const wxColour& fillColour) const;

    /// 把元件的本地坐标点换算成屏幕坐标。
    /// 与 Component::GetTransformedPins 使用同一套朝向旋转规则，
    /// 保证符号主体、反相圆与引脚的朝向始终一致。
    /// @param comp 元件
    /// @param localPt 本地坐标点
    /// @param transform 坐标变换
    /// @return 屏幕坐标点
    wxPoint LocalToScreen(const Component& comp, const wxPoint& localPt,
                          const GridTransform& transform) const;
};

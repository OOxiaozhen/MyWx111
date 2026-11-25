#include "ComponentRenderer.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "../model/Circuit.h"
#include "../model/GateComponents.h"

namespace
{
/// 圆弧角度转弧度。
/// @param degrees 角度值
/// @return 弧度值
double ToRadians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

/// 把矩形规范化为「左上角 + 正宽高」。
/// 框选时用户可能从右下往左上拖，wxRect 的宽高会变成负数。
/// @param a 第一个角点
/// @param b 第二个角点
/// @return 规范化后的矩形
wxRect NormalizeRect(const wxPoint& a, const wxPoint& b)
{
    wxRect rect(a, b);
    if (rect.width < 0)
    {
        rect.x += rect.width;
        rect.width = -rect.width;
    }
    if (rect.height < 0)
    {
        rect.y += rect.height;
        rect.height = -rect.height;
    }
    return rect;
}
} // namespace

wxPoint ComponentRenderer::LocalToScreen(const Component& comp, const wxPoint& localPt,
                                         const GridTransform& transform) const
{
    wxPoint rotated = localPt;

    // 此处复刻模型层 Component::GetTransformedPins 的旋转规则：
    // 绕本地包围盒中心按 90 度倍数旋转，屏幕坐标 Y 向下即视觉顺时针。
    // 必须保持一致，否则符号主体、反相圆与引脚会出现朝向错位
    const int orientation = comp.GetOrientation();
    if (orientation != 0)
    {
        const wxRect local = comp.GetLocalBounds();
        const double cx = local.x + local.width / 2.0;
        const double cy = local.y + local.height / 2.0;
        const double dx = localPt.x - cx;
        const double dy = localPt.y - cy;

        switch (orientation)
        {
        case 90:
            rotated = wxPoint(static_cast<int>(std::lround(cx - dy)),
                              static_cast<int>(std::lround(cy + dx)));
            break;
        case 180:
            rotated = wxPoint(static_cast<int>(std::lround(cx - dx)),
                              static_cast<int>(std::lround(cy - dy)));
            break;
        case 270:
            rotated = wxPoint(static_cast<int>(std::lround(cx + dy)),
                              static_cast<int>(std::lround(cy - dx)));
            break;
        default:
            break;
        }
    }

    return transform.WorldToScreen(comp.GetPosition() + rotated);
}

void ComponentRenderer::DrawComponent(wxGraphicsContext& gc, const Component& comp,
                                      const GridTransform& transform, bool selected,
                                      LogicLevel inputLevel, bool highlightLevel) const
{
    SymbolGeometry geo;
    comp.DescribeSymbol(geo);

    wxColour penColour(70, 70, 80);
    wxColour fillColour(255, 248, 220);

    // LED 指示灯：仿真开启时按输入电平点亮/熄灭，未开启时保持常态外观
    if (highlightLevel && comp.GetType() == ComponentType::LedIndicator)
    {
        switch (inputLevel)
        {
        case LogicLevel::High:
            penColour = wxColour(200, 60, 60);
            fillColour = wxColour(255, 180, 120); // 点亮：暖橙填充
            break;
        case LogicLevel::Low:
            penColour = wxColour(120, 120, 120);
            fillColour = wxColour(200, 200, 200); // 熄灭：灰
            break;
        default:
            penColour = wxColour(150, 150, 160);
            fillColour = wxColour(235, 235, 240); // 未知：中性淡灰
            break;
        }
    }

    DrawBody(gc, comp, transform, penColour, fillColour);
    DrawInversionBubble(gc, geo, comp, transform, fillColour);

    // 带电平的元件（输入引脚 / 电平源）在符号上画一个「可点击圆圈」，
    // 圈内显示 1/0：点圈内切电平，点圈外仍是正常选中。
    // 它与仿真开关无关——开关只控制连线是否按电平着色。
    if (comp.HasLevelToggle())
    {
        LogicLevel toggleLevel = LogicLevel::Unknown;
        if (comp.GetType() == ComponentType::LevelSource)
        {
            toggleLevel = static_cast<const LevelSource&>(comp).GetLevel();
        }
        else if (comp.GetType() == ComponentType::InputPin)
        {
            toggleLevel = static_cast<const InputPin&>(comp).GetLevel();
        }

        wxColour levelColour(120, 120, 120);
        switch (toggleLevel)
        {
        case LogicLevel::High:
            levelColour = wxColour(200, 60, 60);
            break;
        case LogicLevel::Low:
            levelColour = wxColour(60, 110, 200);
            break;
        default:
            break;
        }

        const wxPoint center = LocalToScreen(comp, comp.GetLevelToggleCenter(), transform);
        // 半径随缩放变化，与符号本体保持同一比例
        const double radius = (kComponentBodySize / 4) * transform.GetZoom();

        // 白色底 + 电平色描边：无论本体填充深浅，圆圈与其中的数字都能看清
        gc.SetPen(wxPen(levelColour, 2));
        gc.SetBrush(wxBrush(wxColour(255, 255, 255)));
        gc.DrawEllipse(center.x - radius, center.y - radius, radius * 2.0, radius * 2.0);

        const wxString text(LogicLevelToDisplayChar(toggleLevel));
        gc.SetFont(wxFont(wxFontInfo(10).Bold()), levelColour);
        wxDouble textWidth = 0.0;
        wxDouble textHeight = 0.0;
        gc.GetTextExtent(text, &textWidth, &textHeight);
        gc.DrawText(text, center.x - textWidth / 2.0, center.y - textHeight / 2.0);
    }

    // 引脚：连线状态由 Circuit 侧维护，渲染器拿不到，因此这里不作为高亮依据
    const wxRect worldBounds = comp.GetWorldBounds();
    for (const Pin& pin : comp.GetTransformedPins())
    {
        DrawPin(gc, comp.GetPosition() + pin.offset, worldBounds, transform, false);
    }

    // 元件下方的名称不再绘制：用户在属性面板维护的名称只作标识用途。
    // 例外是 TextLabel —— 它的 DescribeSymbol 只填 labelAnchor、没有主体与引脚，
    // 画布呈现完全依赖这段文字，若一并删除该元件将彻底不可见。
    if (comp.GetType() == ComponentType::TextLabel)
    {
        const wxPoint labelPos = LocalToScreen(comp, geo.labelAnchor, transform);
        gc.SetFont(wxFont(wxFontInfo(8)), penColour);
        double textWidth = 0.0;
        double textHeight = 0.0;
        gc.GetTextExtent(comp.GetName(), &textWidth, &textHeight);
        gc.DrawText(comp.GetName(), labelPos.x - textWidth / 2.0, labelPos.y);
    }

    // 逻辑探针：在符号右上角直接写出当前电平，避免用户只能靠连线颜色反推
    if (highlightLevel && comp.GetType() == ComponentType::LogicProbe)
    {
        wxColour levelColour(120, 120, 120);
        switch (inputLevel)
        {
        case LogicLevel::High:
            levelColour = wxColour(200, 60, 60);
            break;
        case LogicLevel::Low:
            levelColour = wxColour(60, 110, 200);
            break;
        default:
            break;
        }

        const wxPoint textPos = LocalToScreen(comp, wxPoint(24, 12), transform);
        gc.SetFont(wxFont(wxFontInfo(9).Bold()), levelColour);
        gc.DrawText(LogicLevelToDisplayChar(inputLevel), textPos.x, textPos.y);
    }

    if (selected)
    {
        const wxPoint topLeft = transform.WorldToScreen(worldBounds.GetTopLeft());
        const wxPoint bottomRight = transform.WorldToScreen(worldBounds.GetBottomRight());
        gc.SetPen(wxPen(wxColour(0, 120, 215), 1, wxPENSTYLE_SHORT_DASH));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawRectangle(topLeft.x - 3.0, topLeft.y - 3.0,
                         bottomRight.x - topLeft.x + 6.0,
                         bottomRight.y - topLeft.y + 6.0);
    }
}

void ComponentRenderer::DrawBody(wxGraphicsContext& gc, const Component& comp,
                                 const GridTransform& transform, const wxColour& penColour,
                                 const wxColour& fillColour) const
{
    SymbolGeometry geo;
    comp.DescribeSymbol(geo);

    // 线宽固定为屏幕像素而非世界单位：缩放后线条粗细保持稳定，不会糊成一团或细到看不见
    const wxPen bodyPen(penColour, 2);

    if (geo.body.size() >= 3)
    {
        wxGraphicsPath path = gc.CreatePath();
        const wxPoint first = LocalToScreen(comp, geo.body.front(), transform);
        path.MoveToPoint(first.x, first.y);
        for (size_t i = 1; i < geo.body.size(); ++i)
        {
            const wxPoint pt = LocalToScreen(comp, geo.body[i], transform);
            path.AddLineToPoint(pt.x, pt.y);
        }
        path.CloseSubpath();

        gc.SetPen(bodyPen);
        gc.SetBrush(wxBrush(fillColour));
        gc.DrawPath(path);
    }

    for (const SymbolPolyline& polyline : geo.polylines)
    {
        if (polyline.points.size() < 2)
        {
            continue;
        }

        wxGraphicsPath path = gc.CreatePath();
        const wxPoint first = LocalToScreen(comp, polyline.points.front(), transform);
        path.MoveToPoint(first.x, first.y);
        for (size_t i = 1; i < polyline.points.size(); ++i)
        {
            const wxPoint pt = LocalToScreen(comp, polyline.points[i], transform);
            path.AddLineToPoint(pt.x, pt.y);
        }
        if (polyline.closed)
        {
            path.CloseSubpath();
        }

        gc.SetPen(bodyPen);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    for (const SymbolArc& arc : geo.arcs)
    {
        const wxPoint center = LocalToScreen(comp, arc.center, transform);
        const double radius = arc.radius * transform.GetZoom();
        if (radius <= 0.0)
        {
            continue;
        }

        // wxGraphicsPath::AddArc 的角度以弧度计、逆时针为正，
        // 而 SymbolArc 以屏幕角度（顺时针为正）描述，故取负号换算
        wxGraphicsPath path = gc.CreatePath();
        path.AddArc(center.x, center.y, radius, -ToRadians(arc.startAngle),
                    -ToRadians(arc.endAngle), true);

        gc.SetPen(bodyPen);
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }
}

void ComponentRenderer::DrawInversionBubble(wxGraphicsContext& gc, const SymbolGeometry& geo,
                                            const Component& comp, const GridTransform& transform,
                                            const wxColour& fillColour) const
{
    if (geo.inversionRadius <= 0)
    {
        return;
    }

    const wxPoint center = LocalToScreen(comp, geo.inversionCenter, transform);
    // 半径下限 2 像素：符号缩到很小时反相圆仍可见，否则会被误认为普通门
    const double radius = std::max(2.0, geo.inversionRadius * transform.GetZoom());

    gc.SetPen(wxPen(wxColour(70, 70, 80), 2));
    gc.SetBrush(wxBrush(fillColour));
    gc.DrawEllipse(center.x - radius, center.y - radius, radius * 2.0, radius * 2.0);
}

void ComponentRenderer::DrawPin(wxGraphicsContext& gc, const wxPoint& pinWorld,
                                const wxRect& compBounds, const GridTransform& transform,
                                bool highlight) const
{
    const wxPoint end = transform.WorldToScreen(pinWorld);

    // 引脚短线起点取本体边界上的最近点：按引脚落在本体的哪一侧决定
    wxPoint startWorld;
    if (pinWorld.x <= compBounds.GetLeft())
    {
        startWorld = wxPoint(compBounds.GetLeft(), pinWorld.y);
    }
    else if (pinWorld.x >= compBounds.GetRight())
    {
        startWorld = wxPoint(compBounds.GetRight(), pinWorld.y);
    }
    else if (pinWorld.y <= compBounds.GetTop())
    {
        startWorld = wxPoint(pinWorld.x, compBounds.GetTop());
    }
    else
    {
        startWorld = wxPoint(pinWorld.x, compBounds.GetBottom());
    }

    const wxPoint start = transform.WorldToScreen(startWorld);
    const wxColour colour = highlight ? wxColour(0, 160, 255) : wxColour(70, 70, 80);

    gc.SetPen(wxPen(colour, 1));
    gc.StrokeLine(start.x, start.y, end.x, end.y);

    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(colour));
    const double dotRadius = 2.5;
    gc.DrawEllipse(end.x - dotRadius, end.y - dotRadius, dotRadius * 2.0, dotRadius * 2.0);
}

void ComponentRenderer::DrawWire(wxGraphicsContext& gc, const Wire& wire, const Circuit& circuit,
                                 const GridTransform& transform, bool selected,
                                 LogicLevel level, bool highlightLevel) const
{
    const std::vector<wxPoint> path = circuit.GetWirePath(wire);
    if (path.size() < 2)
    {
        return;
    }

    wxGraphicsPath graphicsPath = gc.CreatePath();
    const wxPoint first = transform.WorldToScreen(path.front());
    graphicsPath.MoveToPoint(first.x, first.y);
    for (size_t i = 1; i < path.size(); ++i)
    {
        const wxPoint pt = transform.WorldToScreen(path[i]);
        graphicsPath.AddLineToPoint(pt.x, pt.y);
    }

    // 选中态优先（蓝色加粗）；仿真开启时按电平着色；否则保持常态灰
    wxColour colour = wxColour(90, 90, 100);
    if (selected)
    {
        colour = wxColour(0, 120, 215);
    }
    else if (highlightLevel)
    {
        switch (level)
        {
        case LogicLevel::High:
            colour = wxColour(200, 60, 60);
            break;
        case LogicLevel::Low:
            colour = wxColour(60, 110, 200);
            break;
        default:
            colour = wxColour(90, 90, 100);
            break;
        }
    }

    gc.SetPen(wxPen(colour, selected ? 3 : 2));
    gc.SetBrush(*wxTRANSPARENT_BRUSH);
    gc.StrokePath(graphicsPath);

    // 两端补小圆点，让连线与引脚端点的接合处更自然
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(colour));
    for (const wxPoint& worldPt : { path.front(), path.back() })
    {
        const wxPoint screenPt = transform.WorldToScreen(worldPt);
        gc.DrawEllipse(screenPt.x - 2.0, screenPt.y - 2.0, 4.0, 4.0);
    }
}

void ComponentRenderer::DrawSelectionRect(wxGraphicsContext& gc, const wxRect& worldRect,
                                          const GridTransform& transform) const
{
    const wxRect normalized = NormalizeRect(worldRect.GetTopLeft(), worldRect.GetBottomRight());
    const wxPoint topLeft = transform.WorldToScreen(normalized.GetTopLeft());
    const wxPoint bottomRight = transform.WorldToScreen(normalized.GetBottomRight());

    gc.SetPen(wxPen(wxColour(0, 120, 215), 1, wxPENSTYLE_SHORT_DASH));
    gc.SetBrush(wxBrush(wxColour(0, 120, 215, 40)));
    gc.DrawRectangle(topLeft.x, topLeft.y,
                     bottomRight.x - topLeft.x, bottomRight.y - topLeft.y);
}

void ComponentRenderer::DrawNotes(wxGraphicsContext& gc, const Circuit& circuit,
                                  const GridTransform& transform) const
{
    const auto& notes = circuit.GetNotes();
    if (notes.empty())
    {
        return;
    }

    // 字号随缩放同步变化，保证文字与注释框一起放大缩小
    const double zoom = transform.GetZoom();
    const int fontSize = std::max(6, static_cast<int>(std::lround(10.0 * zoom)));

    for (const Note& note : notes)
    {
        const wxPoint topLeft = transform.WorldToScreen(note.position);
        const wxPoint bottomRight =
            transform.WorldToScreen(wxPoint(note.position.x + note.size.GetWidth(),
                                            note.position.y + note.size.GetHeight()));

        const bool selected = circuit.IsNoteSelected(note.id);

        gc.SetBrush(wxBrush(wxColour(255, 250, 205, 200)));
        if (selected)
        {
            gc.SetPen(wxPen(wxColour(0, 120, 215), 1, wxPENSTYLE_SHORT_DASH));
        }
        else
        {
            gc.SetPen(wxPen(wxColour(180, 170, 130), 1));
        }
        gc.DrawRectangle(topLeft.x, topLeft.y, bottomRight.x - topLeft.x,
                         bottomRight.y - topLeft.y);

        if (selected)
        {
            // 四角小方块作为「可拖动」的视觉提示
            gc.SetPen(*wxTRANSPARENT_PEN);
            gc.SetBrush(wxBrush(wxColour(0, 120, 215)));
            const wxPoint corners[] = { topLeft, wxPoint(bottomRight.x, topLeft.y), bottomRight,
                                        wxPoint(topLeft.x, bottomRight.y) };
            for (const wxPoint& corner : corners)
            {
                gc.DrawRectangle(corner.x - 2.0, corner.y - 2.0, 4.0, 4.0);
            }
        }

        // 正文按 '\n' 拆行逐行绘制；空注释画浅灰占位文字
        const wxString content = note.text.empty() ? wxString(L"(空注释)") : note.text;
        const wxColour textColour =
            note.text.empty() ? wxColour(160, 160, 160) : wxColour(70, 70, 80);
        gc.SetFont(wxFont(wxFontInfo(fontSize)), textColour);

        // wxGraphicsContext 的文本度量出参是 wxDouble，不能用 int
        wxDouble lineHeight = 0.0;
        wxDouble dummyWidth = 0.0;
        gc.GetTextExtent(wxString(L"Hg"), &dummyWidth, &lineHeight);

        int y = topLeft.y + 4;
        wxString remaining = content;
        while (true)
        {
            const int newlinePos = remaining.Find(L'\n');
            const wxString line =
                (newlinePos == wxNOT_FOUND) ? remaining : remaining.Left(newlinePos);
            gc.DrawText(line, topLeft.x + 4, y);
            y += static_cast<int>(lineHeight) + 2;

            if (newlinePos == wxNOT_FOUND)
            {
                break;
            }
            remaining = remaining.Mid(static_cast<size_t>(newlinePos) + 1);
        }
    }
}

void ComponentRenderer::DrawPendingWire(wxGraphicsContext& gc, const wxPoint& fromWorld,
                                        PinSide startSide,
                                        const std::vector<wxPoint>& confirmedPath,
                                        const wxPoint& toWorld,
                                        const GridTransform& transform) const
{
    const wxPoint lastConfirmed = confirmedPath.empty() ? fromWorld : confirmedPath.back();

    // 已确认段：用户每点一次左键就固定一段，这里用实线画出来，
    // 与仍跟随鼠标的末段（虚线）形成区分
    if (!confirmedPath.empty())
    {
        // 已确认段同样要正交补全：用户自由点击的折点之间常常既不同行也不同列，
        // 直接连点会画出斜线，与落线后的折线形态不一致
        std::vector<wxPoint> confirmedWorld;
        AppendOrthogonalPoint(confirmedWorld, fromWorld);
        wxPoint previous = fromWorld;
        for (const wxPoint& pt : confirmedPath)
        {
            AppendOrthogonalSegment(confirmedWorld, previous, pt);
            previous = pt;
        }

        wxGraphicsPath confirmed = gc.CreatePath();
        const wxPoint first = transform.WorldToScreen(confirmedWorld.front());
        confirmed.MoveToPoint(first.x, first.y);
        for (size_t i = 1; i < confirmedWorld.size(); ++i)
        {
            const wxPoint screen = transform.WorldToScreen(confirmedWorld[i]);
            confirmed.AddLineToPoint(screen.x, screen.y);
        }

        gc.SetPen(wxPen(wxColour(0, 120, 215), 2));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(confirmed);
    }

    // 末段路径：尚未确认任何折点时沿用引脚引出规则（与自动正交走线一致）；
    // 已有折点时从最后一个折点用纯正交段连到光标，因为该点已在路上，不再需要引出段
    std::vector<wxPoint> tail;
    if (confirmedPath.empty())
    {
        const wxPoint delta(toWorld.x - fromWorld.x, toWorld.y - fromWorld.y);
        const int absDx = delta.x >= 0 ? delta.x : -delta.x;
        const int absDy = delta.y >= 0 ? delta.y : -delta.y;

        // 终点是自由光标、没有真实引脚：按位移主轴推断入线朝向，取与位移相反的一侧，
        // 使末段引出点落在光标与起点之间，预览线不会绕远
        PinSide endSide = PinSide::Left;
        if (absDx >= absDy)
        {
            endSide = (delta.x >= 0) ? PinSide::Left : PinSide::Right;
        }
        else
        {
            endSide = (delta.y >= 0) ? PinSide::Top : PinSide::Bottom;
        }

        tail = BuildOrthogonalPath(fromWorld, startSide, toWorld, endSide);
    }
    else
    {
        AppendOrthogonalPoint(tail, lastConfirmed);
        AppendOrthogonalSegment(tail, lastConfirmed, toWorld);
    }

    wxGraphicsPath dashed = gc.CreatePath();
    const wxPoint tailStart = transform.WorldToScreen(tail.front());
    dashed.MoveToPoint(tailStart.x, tailStart.y);
    for (size_t i = 1; i < tail.size(); ++i)
    {
        const wxPoint screen = transform.WorldToScreen(tail[i]);
        dashed.AddLineToPoint(screen.x, screen.y);
    }

    gc.SetPen(wxPen(wxColour(0, 120, 215), 2, wxPENSTYLE_SHORT_DASH));
    gc.SetBrush(*wxTRANSPARENT_BRUSH);
    gc.StrokePath(dashed);

    // 起点圆点保留：提示用户这根线是从哪个引脚拉出来的
    const wxPoint origin = transform.WorldToScreen(fromWorld);
    gc.SetPen(*wxTRANSPARENT_PEN);
    gc.SetBrush(wxBrush(wxColour(0, 120, 215)));
    gc.DrawEllipse(origin.x - 3.0, origin.y - 3.0, 6.0, 6.0);
}

void ComponentRenderer::DrawPlacementPreview(wxGraphicsContext& gc, ComponentType type,
                                             const wxPoint& worldPos,
                                             const GridTransform& transform) const
{
    // 用 id = -1 造一个临时元件：它不进入 Circuit，仅在内存中用于预览，
    // 因此无需复制一套符号几何数据，天然与真实放置结果保持一致
    const std::unique_ptr<Component> preview = ComponentFactory::Create(type, -1);
    if (!preview)
    {
        return;
    }
    preview->SetPosition(worldPos);

    SymbolGeometry geo;
    preview->DescribeSymbol(geo);

    DrawBody(gc, *preview, transform, wxColour(150, 160, 180), wxColour(220, 235, 250));
    DrawInversionBubble(gc, geo, *preview, transform, wxColour(220, 235, 250));

    const wxRect worldBounds = preview->GetWorldBounds();
    for (const Pin& pin : preview->GetTransformedPins())
    {
        DrawPin(gc, preview->GetPosition() + pin.offset, worldBounds, transform, false);
    }
}

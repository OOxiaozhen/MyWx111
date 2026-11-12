#include "IconFactory.h"

#include <map>
#include <memory>
#include <tuple>
#include <cstring>

#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/pen.h>

// 图标绘制规格参考 KiCad 官方图标设计指南（dev-docs.kicad.org 的 Icon Design Guidelines）：
// 24x24 画布、水平/垂直线统一 2px、其余线条不小于 1.5px、纯色扁平填充、
// 禁止渐变与 3D 光照效果、极简造型（24px 下不保留精细特征）。

namespace
{
    /// 图标设计的逻辑画布边长（像素）。
    /// 所有图标路径都按这套 24x24 坐标系书写，输出时整体等比缩放，
    /// 这样线宽与间距能随目标尺寸和屏幕 DPI 同步放大，避免高 DPI 下线条过细而糊成一片。
    constexpr double kCanvasSize = 24.0;

    /// 主干线宽（逻辑像素）。遵循 KiCad 规范：传达主要含义的水平/垂直线统一 2px。
    constexpr double kStrokeWidth = 2.0;

    /// 细节线宽（逻辑像素）。用于镜内符号这类从属元素，避免与主干线争夺视觉重量。
    constexpr double kDetailWidth = 1.5;

    /// 缓存键：图标标识 + 输出尺寸 + 颜色 RGBA + 激活态标记。
    /// 颜色用 RGBA 整数参与比较，避免 wxColour 之间做浮点/字符串比较。
    /// 激活态标记必须参与区分，否则同一图标的常态位图与激活态位图会互相覆盖。
    using CacheKey = std::tuple<int, int, unsigned long, bool>;

    /// 图标位图缓存。
    /// 工具条在窗口重绘、AUI 布局变化时会反复取图标，缓存可避免每次都走一遍路径渲染。
    /// 进程生命周期内不清理：图标数量少且尺寸固定，缓存无增长压力。
    std::map<CacheKey, wxBitmap>& GetCache()
    {
        static std::map<CacheKey, wxBitmap> cache;
        return cache;
    }

    /// 创建一张背景全透明的位图，供后续矢量绘制使用。
    /// 通过 wxImage 显式建立 alpha 通道，避免依赖位图初始内容：
    /// 若透明区域未被正确置零，在浅色工具条上会渲染成难看的黑色方块底衬。
    /// @param size 位图边长（像素），需大于 0
    /// @param color 描边颜色
    /// @return 带 alpha 通道的位图；其 RGB 通道预填为描边色
    wxBitmap CreateTransparentBitmap(int size, const wxColour& color)
    {
        wxImage image(size, size);
        image.InitAlpha();

        // RGB 预填描边色而非黑色：图标边缘存在抗锯齿的半透明像素，
        // 让底色与线条同色可避免边缘渗出黑边或白边
        unsigned char* rgb = image.GetData();
        for (int i = 0; i < size * size; ++i)
        {
            rgb[i * 3 + 0] = color.Red();
            rgb[i * 3 + 1] = color.Green();
            rgb[i * 3 + 2] = color.Blue();
        }

        std::memset(image.GetAlpha(), wxIMAGE_ALPHA_TRANSPARENT,
                    static_cast<size_t>(size) * static_cast<size_t>(size));
        return wxBitmap(image);
    }

    /// 创建圆头圆角的描边笔。
    /// KiCad 规范要求等宽描边且避免「手绘笔触」观感，圆头圆角是达成该效果的关键。
    /// @param gc 目标绘图上下文
    /// @param color 描边颜色
    /// @param width 线宽（逻辑像素）
    /// @return 可用于 SetPen 的图形笔
    wxGraphicsPen MakeStrokePen(wxGraphicsContext& gc, const wxColour& color,
                                double width = kStrokeWidth)
    {
        wxGraphicsPenInfo info(color, width);
        info.Cap(wxCAP_ROUND).Join(wxJOIN_ROUND);
        return gc.CreatePen(info);
    }

    /// 绘制实心圆点，用于表示导线引脚结点。
    /// 填色后不描边，边缘由抗锯齿自然柔化。
    /// @param gc 目标绘图上下文
    /// @param color 填充颜色
    /// @param cx 圆心 X（逻辑坐标）
    /// @param cy 圆心 Y（逻辑坐标）
    /// @param radius 半径（逻辑像素）
    void FillDisc(wxGraphicsContext& gc, const wxColour& color,
                  double cx, double cy, double radius)
    {
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(color));
        gc.DrawEllipse(cx - radius, cy - radius, radius * 2.0, radius * 2.0);
    }

    /// 绘制「选择」图标：鼠标箭头指针。
    /// 按指针真实轮廓取点：左上角为箭头尖端，向左下延伸到底部后折回，
    /// 中部留出箭头尾部缺口，这样在 24px 下仍能一眼认出是光标。
    /// @param gc 目标绘图上下文
    /// @param color 填充颜色
    void DrawSelectIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(8.0, 4.0);
        path.AddLineToPoint(8.0, 18.0);
        path.AddLineToPoint(11.5, 14.5);
        path.AddLineToPoint(14.0, 20.0);
        path.AddLineToPoint(16.0, 19.0);
        path.AddLineToPoint(13.5, 13.5);
        path.AddLineToPoint(18.0, 13.5);
        path.CloseSubpath();

        // 指针整体实心填充：箭头本身是不规则多边形，描边会在缺口处堆积出脏边
        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(color));
        gc.FillPath(path);
    }

    /// 绘制「连线」图标：一段正交走线，两端带引脚结点。
    /// 走线采用「水平—垂直—水平」的正交折线，与电路导线的实际走线方式一致，
    /// 借此与单纯的斜线区分开。
    /// @param gc 目标绘图上下文
    /// @param color 线条与结点颜色
    void DrawWireIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(4.5, 18.0);
        path.AddLineToPoint(4.5, 12.0);
        path.AddLineToPoint(12.0, 12.0);
        path.AddLineToPoint(12.0, 6.0);
        path.AddLineToPoint(19.5, 6.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);

        // 两端补结点圆点：导线在原理图中总是以结点收尾，加上后辨识度显著提升
        FillDisc(gc, color, 4.5, 18.0, 1.8);
        FillDisc(gc, color, 19.5, 6.0, 1.8);
    }

    /// 绘制放大镜公共部分：镜圈与手柄。
    /// 两个缩放图标只有镜内符号不同，公共外形抽在这里避免重复代码。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawMagnifierBody(wxGraphicsContext& gc, const wxColour& color)
    {
        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);

        // 镜圈：圆心 (10, 10)、半径 6，为右下角的手柄让出空间
        gc.DrawEllipse(4.0, 4.0, 12.0, 12.0);

        // 手柄沿 45° 自镜圈右下引出。
        // 起点取圆周位置 10 + 6/√2 ≈ 14.24，这里用 14.5 略微外移，
        // 让手柄根部与镜圈衔接但不在交点处形成厚重黑点
        wxGraphicsPath handle = gc.CreatePath();
        handle.MoveToPoint(14.5, 14.5);
        handle.AddLineToPoint(19.5, 19.5);
        gc.StrokePath(handle);
    }

    /// 绘制「放大」图标：放大镜加镜内十字。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawZoomInIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        DrawMagnifierBody(gc, color);

        // 镜内符号用细线，否则与镜圈叠加后中心区域过黑
        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        wxGraphicsPath plus = gc.CreatePath();
        plus.MoveToPoint(7.0, 10.0);
        plus.AddLineToPoint(13.0, 10.0);
        plus.MoveToPoint(10.0, 7.0);
        plus.AddLineToPoint(10.0, 13.0);
        gc.StrokePath(plus);
    }

    /// 绘制「缩小」图标：放大镜加镜内短横线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawZoomOutIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        DrawMagnifierBody(gc, color);

        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        wxGraphicsPath minus = gc.CreatePath();
        minus.MoveToPoint(7.0, 10.0);
        minus.AddLineToPoint(13.0, 10.0);
        gc.StrokePath(minus);
    }

    /// 绘制「网格」图标：2x2 方格。
    /// 24px 画布下若画 3x3 以上，线条间距不足会让整块糊成灰团，
    /// 故按 KiCad 的极简原则只用外框加中线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawGridIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);

        wxGraphicsPath frame = gc.CreatePath();
        frame.AddRectangle(4.0, 4.0, 16.0, 16.0);
        gc.StrokePath(frame);

        wxGraphicsPath cross = gc.CreatePath();
        cross.MoveToPoint(12.0, 4.0);
        cross.AddLineToPoint(12.0, 20.0);
        cross.MoveToPoint(4.0, 12.0);
        cross.AddLineToPoint(20.0, 12.0);
        gc.StrokePath(cross);
    }

    /// 绘制「撤销」图标：向左折返的箭头。
    /// 用折线而非圆弧：24x24 画布下弧线的可用像素太少，折返造型在 18px 实际显示尺寸下
    /// 辨识度更高，同时也避开了手工推算弧角带来的误差。
    /// @param gc 目标绘图上下文
    /// @param color 线条与箭头颜色
    void DrawUndoIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(19.0, 17.0);
        path.AddLineToPoint(19.0, 9.0);
        path.AddLineToPoint(8.0, 9.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);

        // 箭尖朝左：三点三角形实心填充，小尺寸下比描边更清晰
        wxGraphicsPath arrow = gc.CreatePath();
        arrow.MoveToPoint(4.0, 9.0);
        arrow.AddLineToPoint(10.0, 5.5);
        arrow.AddLineToPoint(10.0, 12.5);
        arrow.CloseSubpath();

        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(color));
        gc.FillPath(arrow);
    }

    /// 绘制「重做」图标：DrawUndoIcon 关于 x = 12 的水平镜像，箭尖朝右。
    /// @param gc 目标绘图上下文
    /// @param color 线条与箭头颜色
    void DrawRedoIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(5.0, 17.0);
        path.AddLineToPoint(5.0, 9.0);
        path.AddLineToPoint(16.0, 9.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);

        wxGraphicsPath arrow = gc.CreatePath();
        arrow.MoveToPoint(20.0, 9.0);
        arrow.AddLineToPoint(14.0, 5.5);
        arrow.AddLineToPoint(14.0, 12.5);
        arrow.CloseSubpath();

        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(color));
        gc.FillPath(arrow);
    }

    /// 绘制「旋转」图标：顺时针方向的折线箭头。
    /// 与撤销/重做图标同样选用折线而非圆弧：18px 实际尺寸下弧线可用像素太少，
    /// 方形路径的走向反而更容易让人认出「顺时针旋转」。
    /// @param gc 目标绘图上下文
    /// @param color 线条与箭头颜色
    void DrawRotateIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        // 从左下向上、再向右、最后向下，末段箭头朝下表示顺时针
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(7.0, 17.0);
        path.AddLineToPoint(7.0, 7.0);
        path.AddLineToPoint(17.0, 7.0);
        path.AddLineToPoint(17.0, 13.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);

        // 箭头朝下，与末段方向一致
        wxGraphicsPath arrow = gc.CreatePath();
        arrow.MoveToPoint(17.0, 18.0);
        arrow.AddLineToPoint(13.5, 12.5);
        arrow.AddLineToPoint(20.5, 12.5);
        arrow.CloseSubpath();

        gc.SetPen(*wxTRANSPARENT_PEN);
        gc.SetBrush(wxBrush(color));
        gc.FillPath(arrow);
    }

    /// 绘制「删除」图标：叉号。
    /// 叉号在小尺寸下辨识度最高，也与「删除」的语义直接对应。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawDeleteIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(7.0, 7.0);
        path.AddLineToPoint(17.0, 17.0);
        path.MoveToPoint(17.0, 7.0);
        path.AddLineToPoint(7.0, 17.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    /// 绘制「粘贴」图标：下方纸页 + 顶部夹子。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawPasteIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        // 纸页主体
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(6.0, 8.0);
        body.AddLineToPoint(18.0, 8.0);
        body.AddLineToPoint(18.0, 20.0);
        body.AddLineToPoint(6.0, 20.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        // 顶部夹子，稍微压住纸页上沿，暗示「贴上去」
        wxGraphicsPath clip = gc.CreatePath();
        clip.MoveToPoint(9.0, 4.0);
        clip.AddLineToPoint(15.0, 4.0);
        clip.AddLineToPoint(15.0, 8.0);
        clip.AddLineToPoint(9.0, 8.0);
        clip.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.SetBrush(wxBrush(color));
        gc.StrokePath(clip);
    }

    /// 绘制「文本标注」图标：横线 + 竖线构成 T 字。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawTextLabelIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(5.0, 7.0);
        path.AddLineToPoint(19.0, 7.0);
        path.MoveToPoint(12.0, 7.0);
        path.AddLineToPoint(12.0, 19.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    /// 绘制「高电平 VCC」图标：T 形。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawVccIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(6.0, 8.0);
        path.AddLineToPoint(18.0, 8.0);
        path.MoveToPoint(12.0, 8.0);
        path.AddLineToPoint(12.0, 19.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    /// 绘制「低电平 GND」图标：三条递减横线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawGndIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(6.0, 8.0);
        path.AddLineToPoint(18.0, 8.0);
        path.MoveToPoint(8.0, 13.0);
        path.AddLineToPoint(16.0, 13.0);
        path.MoveToPoint(10.0, 18.0);
        path.AddLineToPoint(14.0, 18.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    /// 绘制「时钟」图标：方波。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawClockIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath path = gc.CreatePath();
        path.MoveToPoint(4.0, 15.0);
        path.AddLineToPoint(9.0, 15.0);
        path.AddLineToPoint(9.0, 8.0);
        path.AddLineToPoint(15.0, 8.0);
        path.AddLineToPoint(15.0, 15.0);
        path.AddLineToPoint(20.0, 15.0);

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(path);
    }

    /// 绘制「同或门」图标：三角形 + 输出端小圆。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawXnorIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(5.0, 7.0);
        body.AddLineToPoint(15.0, 12.0);
        body.AddLineToPoint(5.0, 17.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawEllipse(16.0, 9.5, 5.0, 5.0);
    }

    /// 绘制「缓冲器」图标：三角形。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawBufferIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(5.0, 6.0);
        body.AddLineToPoint(19.0, 12.0);
        body.AddLineToPoint(5.0, 18.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);
    }

    /// 绘制「三态门」图标：三角形 + 顶部使能短线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawTriStateIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(5.0, 8.0);
        body.AddLineToPoint(19.0, 13.0);
        body.AddLineToPoint(5.0, 18.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        wxGraphicsPath stub = gc.CreatePath();
        stub.MoveToPoint(11.0, 10.0);
        stub.AddLineToPoint(11.0, 4.0);
        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.StrokePath(stub);
    }

    /// 绘制「D 触发器」图标：矩形 + 内部时钟标记。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawDFlipFlopIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(7.0, 6.0);
        body.AddLineToPoint(19.0, 6.0);
        body.AddLineToPoint(19.0, 18.0);
        body.AddLineToPoint(7.0, 18.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        wxGraphicsPath clk = gc.CreatePath();
        clk.MoveToPoint(11.0, 12.0);
        clk.AddLineToPoint(11.0, 18.0);
        clk.AddLineToPoint(7.0, 15.0);
        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.StrokePath(clk);
    }

    /// 绘制「RS 锁存器」图标：矩形 + 内部斜线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawRsLatchIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(7.0, 6.0);
        body.AddLineToPoint(19.0, 6.0);
        body.AddLineToPoint(19.0, 18.0);
        body.AddLineToPoint(7.0, 18.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        wxGraphicsPath divider = gc.CreatePath();
        divider.MoveToPoint(13.0, 6.0);
        divider.AddLineToPoint(13.0, 18.0);
        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.StrokePath(divider);
    }

    /// 绘制「LED」图标：圆形 + 两条射线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawLedIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.DrawEllipse(6.0, 8.0, 10.0, 10.0);

        wxGraphicsPath rays = gc.CreatePath();
        rays.MoveToPoint(17.0, 8.0);
        rays.AddLineToPoint(20.0, 5.0);
        rays.MoveToPoint(19.0, 12.0);
        rays.AddLineToPoint(22.0, 11.0);
        gc.SetPen(MakeStrokePen(gc, color, kDetailWidth));
        gc.StrokePath(rays);
    }

    /// 绘制「逻辑探针」图标：小三角 + 尾部短线。
    /// @param gc 目标绘图上下文
    /// @param color 线条颜色
    void DrawProbeIcon(wxGraphicsContext& gc, const wxColour& color)
    {
        wxGraphicsPath body = gc.CreatePath();
        body.MoveToPoint(8.0, 7.0);
        body.AddLineToPoint(19.0, 12.0);
        body.AddLineToPoint(8.0, 17.0);
        body.CloseSubpath();

        gc.SetPen(MakeStrokePen(gc, color));
        gc.SetBrush(*wxTRANSPARENT_BRUSH);
        gc.StrokePath(body);

        wxGraphicsPath lead = gc.CreatePath();
        lead.MoveToPoint(4.0, 12.0);
        lead.AddLineToPoint(8.0, 12.0);
        gc.StrokePath(lead);
    }
}

wxBitmap IconFactory::GetIcon(IconId id, int size, const wxColour& color, bool highlighted)
{
    // 尺寸兜底：尺寸非法时退回设计基准 24px，避免创建零尺寸位图导致后续绘制断言
    if (size <= 0)
    {
        size = static_cast<int>(kCanvasSize);
    }

    auto& cache = GetCache();
    const CacheKey key{ static_cast<int>(id), size, color.GetRGBA(), highlighted };

    const auto it = cache.find(key);
    if (it != cache.end() && it->second.IsOk())
    {
        return it->second;
    }

    wxBitmap bmp = RenderIcon(id, size, color, highlighted);
    if (bmp.IsOk())
    {
        cache.emplace(key, bmp);
    }
    return bmp;
}

wxBitmap IconFactory::RenderIcon(IconId id, int size, const wxColour& color, bool highlighted)
{
    wxBitmap bmp = CreateTransparentBitmap(size, color);

    // 内存 DC 与绘图上下文都限制在局部作用域内：
    // 位图在仍被选入 DC 时无法安全交还调用方，因此必须在返回前完成解绑
    {
        wxMemoryDC memDc(bmp);
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(memDc));
        if (!gc)
        {
            // 系统未启用 wxUSE_GRAPHICS_CONTEXT 时退化为空白透明图，避免崩溃
            return bmp;
        }

        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // 把 24x24 逻辑坐标等比映射到目标像素尺寸。
        // 用缩放而非逐尺寸重算坐标，使同一套路径在任意尺寸/DPI 下都保持一致的粗细比例
        const double scale = static_cast<double>(size) / kCanvasSize;
        gc->Scale(scale, scale);

        // 激活态底色铺满整个 24x24 逻辑画布，缩放到目标尺寸后正好成为图标底衬；
        // 圆角半径 3 逻辑像素，与其他图标的圆头圆角风格保持一致
        if (highlighted)
        {
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(wxColour(204, 228, 247)));
            gc->DrawRoundedRectangle(0.0, 0.0, kCanvasSize, kCanvasSize, 3.0);
        }

        switch (id)
        {
        case IconId::Select:
            DrawSelectIcon(*gc, color);
            break;
        case IconId::Wire:
            DrawWireIcon(*gc, color);
            break;
        case IconId::ZoomIn:
            DrawZoomInIcon(*gc, color);
            break;
        case IconId::ZoomOut:
            DrawZoomOutIcon(*gc, color);
            break;
        case IconId::Grid:
            DrawGridIcon(*gc, color);
            break;
        case IconId::Undo:
            DrawUndoIcon(*gc, color);
            break;
        case IconId::Redo:
            DrawRedoIcon(*gc, color);
            break;
        case IconId::Rotate:
            DrawRotateIcon(*gc, color);
            break;
        case IconId::Delete:
            DrawDeleteIcon(*gc, color);
            break;
        case IconId::Paste:
            DrawPasteIcon(*gc, color);
            break;
        case IconId::TextLabel:
            DrawTextLabelIcon(*gc, color);
            break;
        case IconId::Vcc:
            DrawVccIcon(*gc, color);
            break;
        case IconId::Gnd:
            DrawGndIcon(*gc, color);
            break;
        case IconId::Clock:
            DrawClockIcon(*gc, color);
            break;
        case IconId::Xnor:
            DrawXnorIcon(*gc, color);
            break;
        case IconId::Buffer:
            DrawBufferIcon(*gc, color);
            break;
        case IconId::TriState:
            DrawTriStateIcon(*gc, color);
            break;
        case IconId::DFlipFlop:
            DrawDFlipFlopIcon(*gc, color);
            break;
        case IconId::RsLatch:
            DrawRsLatchIcon(*gc, color);
            break;
        case IconId::Led:
            DrawLedIcon(*gc, color);
            break;
        case IconId::Probe:
            DrawProbeIcon(*gc, color);
            break;
        default:
            break;
        }

        // 显式提交绘制命令，确保位图在 DC 解绑前已完成光栅化
        gc->Flush();
    }

    return bmp;
}

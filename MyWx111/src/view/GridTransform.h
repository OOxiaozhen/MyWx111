#pragma once

#include <algorithm>
#include <cmath>

#include <wx/dc.h>
#include <wx/gdicmn.h>

/// 世界坐标（电路逻辑坐标）与屏幕坐标之间的换算，以及网格吸附与网格绘制。
/// 约定：screen = pan + world * zoom，
/// 其中 pan 表示「世界原点在屏幕上的像素位置」，zoom 为缩放比例。
class GridTransform
{
public:
    static constexpr double kMinZoom = 0.25;    ///< 最小缩放比例
    static constexpr double kMaxZoom = 4.0;     ///< 最大缩放比例
    static constexpr int kDefaultGridSize = 16; ///< 默认网格间距（世界单位）

    /// @param world 世界坐标点
    /// @return 对应的屏幕坐标点
    wxPoint WorldToScreen(const wxPoint& world) const
    {
        return wxPoint(static_cast<int>(std::lround(m_pan.x + world.x * m_zoom)),
                       static_cast<int>(std::lround(m_pan.y + world.y * m_zoom)));
    }

    /// @param screen 屏幕坐标点
    /// @return 对应的世界坐标点
    wxPoint ScreenToWorld(const wxPoint& screen) const
    {
        return wxPoint(static_cast<int>(std::lround((screen.x - m_pan.x) / m_zoom)),
                       static_cast<int>(std::lround((screen.y - m_pan.y) / m_zoom)));
    }

    /// 把世界坐标吸附到最近的网格交点。
    /// @param world 世界坐标点
    /// @return 吸附后的世界坐标点
    wxPoint SnapToGrid(const wxPoint& world) const
    {
        return wxPoint(SnapValue(world.x), SnapValue(world.y));
    }

    /// @return 当前网格间距（世界单位）
    int GetGridSize() const { return m_gridSize; }

    /// @param size 新的网格间距，内部钳制到 [4, 128]
    void SetGridSize(int size) { m_gridSize = std::clamp(size, 4, 128); }

    /// @return 当前缩放比例
    double GetZoom() const { return m_zoom; }

    /// @param zoom 新的缩放比例，内部钳制到 [kMinZoom, kMaxZoom]
    void SetZoom(double zoom) { m_zoom = std::clamp(zoom, kMinZoom, kMaxZoom); }

    /// @return 世界原点在屏幕上的位置
    const wxPoint& GetPanOffset() const { return m_pan; }

    /// @param pan 新的世界原点屏幕位置
    void SetPanOffset(const wxPoint& pan) { m_pan = pan; }

    /// 按屏幕像素位移平移画布。
    /// @param deltaScreen 屏幕坐标系下的位移量
    void PanBy(const wxSize& deltaScreen)
    {
        m_pan.x += deltaScreen.x;
        m_pan.y += deltaScreen.y;
    }

    /// 以指定屏幕点为轴心缩放，保证该点下方的世界坐标不漂移。
    /// @param screenAnchor 轴心（通常是鼠标位置）
    /// @param factor 缩放因子（大于 1 放大，小于 1 缩小）
    /// @return 缩放比例实际发生变化时返回 true
    bool ZoomAt(const wxPoint& screenAnchor, double factor)
    {
        const double newZoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
        if (std::abs(newZoom - m_zoom) < 1e-9)
        {
            return false;
        }

        // 先记录缩放前鼠标点下的世界坐标，缩放后反推 pan，
        // 使「同一个世界点仍落在同一屏幕位置」，实现鼠标不漂移的缩放体验
        const wxPoint worldUnderCursor = ScreenToWorld(screenAnchor);
        m_zoom = newZoom;
        m_pan.x = screenAnchor.x - static_cast<int>(std::lround(worldUnderCursor.x * m_zoom));
        m_pan.y = screenAnchor.y - static_cast<int>(std::lround(worldUnderCursor.y * m_zoom));
        return true;
    }

    /// 把视图中心对准指定世界坐标点。
    /// @param world 目标世界坐标点
    /// @param clientSize 画布客户区尺寸
    void CenterOn(const wxPoint& world, const wxSize& clientSize)
    {
        m_pan.x = clientSize.x / 2 - static_cast<int>(std::lround(world.x * m_zoom));
        m_pan.y = clientSize.y / 2 - static_cast<int>(std::lround(world.y * m_zoom));
    }

    /// 在指定 DC 上绘制网格背景。
    /// 网格线直接按屏幕像素绘制（不做缩放），保证任意缩放下线条都是清晰的 1 像素。
    /// @param dc 绘制目标
    /// @param clientSize 客户区尺寸
    void DrawGrid(wxDC& dc, const wxSize& clientSize) const
    {
        // 网格线在屏幕上的间距；缩得太小就整体不画，
        // 否则密集线条会糊成一片，反而干扰元件识别
        const double stepScreen = m_gridSize * m_zoom;
        if (stepScreen < 6.0)
        {
            return;
        }

        const int step = static_cast<int>(std::lround(stepScreen));
        if (step <= 0)
        {
            return;
        }

        dc.SetPen(wxPen(wxColour(226, 226, 234), 1));

        // 把 pan 对步长取模得到第一条线的位置，使网格随平移连续滚动而不跳变
        int firstX = m_pan.x % step;
        if (firstX > 0)
        {
            firstX -= step;
        }
        int firstY = m_pan.y % step;
        if (firstY > 0)
        {
            firstY -= step;
        }

        for (int x = firstX; x < clientSize.x; x += step)
        {
            dc.DrawLine(x, 0, x, clientSize.y);
        }
        for (int y = firstY; y < clientSize.y; y += step)
        {
            dc.DrawLine(0, y, clientSize.x, y);
        }
    }

private:
    /// 把单个坐标值吸附到最近的网格倍数。
    /// @param value 世界坐标值
    /// @return 吸附后的坐标值
    int SnapValue(int value) const
    {
        if (m_gridSize <= 0)
        {
            return value;
        }
        const int half = m_gridSize / 2;
        return ((value >= 0) ? (value + half) : (value - half)) / m_gridSize * m_gridSize;
    }

    double m_zoom = 1.0;               ///< 缩放比例
    wxPoint m_pan;                     ///< 世界原点在屏幕上的位置
    int m_gridSize = kDefaultGridSize; ///< 网格间距（世界单位）
};

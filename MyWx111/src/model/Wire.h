#pragma once

#include <vector>

#include <wx/gdicmn.h>

/// 指向某个元件某个引脚的引用。
struct PinRef
{
    int componentId = -1; ///< 元件 id；-1 表示无效引用
    int pinIndex = -1;    ///< 引脚序号；-1 表示无效引用

    /// @return 元件 id 与引脚序号均有效时返回 true
    bool IsValid() const { return componentId >= 0 && pinIndex >= 0; }

    /// @param other 另一个引脚引用
    /// @return 两个引用是否指向同一根引脚
    bool operator==(const PinRef& other) const
    {
        return componentId == other.componentId && pinIndex == other.pinIndex;
    }
};

/// 引脚相对元件的几何朝向，决定导线从引脚出线时的走向。
/// 与 Pin::direction（电气方向 Input/Output）无关，这里是纯几何信息。
enum class PinSide
{
    Left,  ///< 引脚位于元件左侧，导线向 -x 出线
    Right, ///< 引脚位于元件右侧，导线向 +x 出线
    Top,   ///< 引脚位于元件上方，导线向 -y 出线
    Bottom ///< 引脚位于元件下方，导线向 +y 出线
};

/// 把引脚朝向换算为单位方向向量。
/// @param side 引脚所在侧
/// @return 长度为 1 的方向向量（世界坐标系，y 轴向下为正）
inline wxPoint PinSideToDir(PinSide side)
{
    switch (side)
    {
    case PinSide::Left:
        return wxPoint(-1, 0);
    case PinSide::Right:
        return wxPoint(1, 0);
    case PinSide::Top:
        return wxPoint(0, -1);
    case PinSide::Bottom:
        return wxPoint(0, 1);
    }
    return wxPoint(1, 0);
}

/// 向路径尾部追加一个折点，并顺手消除退化线段。
/// 正交走线极易产生「相邻两点重合」或「三点共线」的中间点，
/// 若原样保留，渲染层会画出零长度线段，端点圆点也会重叠成一坨。
/// @param path 正在构建的路径点列
/// @param pt 待追加的折点（世界坐标）
inline void AppendOrthogonalPoint(std::vector<wxPoint>& path, const wxPoint& pt)
{
    if (!path.empty() && path.back() == pt)
    {
        return; // 与上一个点完全重合，直接丢弃
    }

    if (path.size() >= 2)
    {
        const wxPoint& prev = path[path.size() - 2];
        const wxPoint& last = path.back();
        // 三点共线时用新点替换中间点，等效于删掉冗余拐点
        const bool sameColumn = (prev.x == last.x && last.x == pt.x);
        const bool sameRow = (prev.y == last.y && last.y == pt.y);
        if (sameColumn || sameRow)
        {
            path.back() = pt;
            return;
        }
    }

    path.push_back(pt);
}

/// 把一段 from → to 以正交方式追加到路径尾部。
/// 已同行或同列时直接追加；否则插入一个 L 形拐点（先横后竖），
/// 保证手动折点路径同样横平竖直，不会在末段连出斜线。
/// @param path 正在构建的路径
/// @param from 线段起点（世界坐标）
/// @param to 线段终点（世界坐标）
inline void AppendOrthogonalSegment(std::vector<wxPoint>& path, const wxPoint& from,
                                    const wxPoint& to)
{
    if (from.x == to.x || from.y == to.y)
    {
        AppendOrthogonalPoint(path, to);
        return;
    }

    // 先沿 x 走到目标列，再沿 y 到终点：拐点取 to.x 与 from.y 的交点
    AppendOrthogonalPoint(path, wxPoint(to.x, from.y));
    AppendOrthogonalPoint(path, to);
}

/// 生成两个引脚之间的正交折线路径（世界坐标）。
/// 走线规则：
/// 1. 两端各沿引脚朝向引出一段固定长度的「引出段」，避免导线贴着元件本体拐弯；
/// 2. 两端都水平时用 Z 形（中间竖线在两端 x 的中点处）；
/// 3. 两端都垂直时用横版 Z 形（中间横线在两端 y 的中点处）；
/// 4. 一横一竖时用 L 形单拐点。
/// 全部点列相邻两项必定 x 或 y 相等，即严格横平竖直，不会出现斜线段。
/// @param startWorld 起点引脚的世界坐标
/// @param startSide 起点引脚所在侧
/// @param endWorld 终点引脚的世界坐标
/// @param endSide 终点引脚所在侧
/// @return 起点 → 若干折点 → 终点的点列；不抛异常
inline std::vector<wxPoint> BuildOrthogonalPath(const wxPoint& startWorld, PinSide startSide,
                                               const wxPoint& endWorld, PinSide endSide)
{
    // 引出段长度（世界单位）：约两格，保证拐点不会压在元件符号上
    constexpr int kStubLength = 20;

    const wxPoint startDir = PinSideToDir(startSide);
    const wxPoint endDir = PinSideToDir(endSide);

    const wxPoint stubStart(startWorld.x + startDir.x * kStubLength,
                            startWorld.y + startDir.y * kStubLength);
    const wxPoint stubEnd(endWorld.x + endDir.x * kStubLength,
                          endWorld.y + endDir.y * kStubLength);

    const bool startHorizontal = (startDir.y == 0);
    const bool endHorizontal = (endDir.y == 0);

    std::vector<wxPoint> path;
    path.reserve(6);
    AppendOrthogonalPoint(path, startWorld);
    AppendOrthogonalPoint(path, stubStart);

    if (startHorizontal && endHorizontal)
    {
        // 两端都是水平出线：中间一段竖线落在两端引出点的 x 中点上
        if (stubStart.y != stubEnd.y)
        {
            const int midX = (stubStart.x + stubEnd.x) / 2;
            AppendOrthogonalPoint(path, wxPoint(midX, stubStart.y));
            AppendOrthogonalPoint(path, wxPoint(midX, stubEnd.y));
        }
    }
    else if (!startHorizontal && !endHorizontal)
    {
        // 两端都是垂直出线：中间一段横线落在两端引出点的 y 中点上
        if (stubStart.x != stubEnd.x)
        {
            const int midY = (stubStart.y + stubEnd.y) / 2;
            AppendOrthogonalPoint(path, wxPoint(stubStart.x, midY));
            AppendOrthogonalPoint(path, wxPoint(stubEnd.x, midY));
        }
    }
    else if (startHorizontal)
    {
        // 起点水平、终点垂直：单拐点取两者交点，避免路径往回折
        AppendOrthogonalPoint(path, wxPoint(stubEnd.x, stubStart.y));
    }
    else
    {
        // 起点垂直、终点水平
        AppendOrthogonalPoint(path, wxPoint(stubStart.x, stubEnd.y));
    }

    AppendOrthogonalPoint(path, stubEnd);
    AppendOrthogonalPoint(path, endWorld);
    return path;
}

/// 电路连线：以两个引脚引用为端点，可选若干手动折点。
/// 实现全部内联在头文件中：本类没有需要隐藏的实现细节，
/// 内联可避免为一个几十行的数据结构单独维护一个 .cpp。
class Wire
{
public:
    /// @param id   连线唯一 id
    /// @param from 起点引脚引用（应为输出脚）
    /// @param to   终点引脚引用（应为输入脚）
    Wire(int id, const PinRef& from, const PinRef& to)
        : m_id(id)
        , m_from(from)
        , m_to(to)
    {
    }

    /// @return 连线唯一 id
    int GetId() const { return m_id; }

    /// @return 起点引脚引用
    const PinRef& GetFrom() const { return m_from; }

    /// @return 终点引脚引用
    const PinRef& GetTo() const { return m_to; }

    /// 设置手动折点，覆盖自动正交走线。
    /// @param bends 折点列表（世界坐标）；传空列表则恢复自动正交走线
    void SetBendPoints(std::vector<wxPoint> bends) { m_bendPoints = std::move(bends); }

    /// @return 手动折点列表（世界坐标），为空表示当前使用自动正交走线
    const std::vector<wxPoint>& GetBendPoints() const { return m_bendPoints; }

    /// 生成完整折线路径。
    /// 若已设置手动折点，则严格按「起点 → 折点… → 终点」走线并忽略引脚朝向；
    /// 否则按两端引脚朝向生成正交多段折线。
    /// @param startWorld 起点引脚的世界坐标
    /// @param startSide  起点引脚所在侧
    /// @param endWorld   终点引脚的世界坐标
    /// @param endSide    终点引脚所在侧
    /// @return 依次包含 起点 → 折点… → 终点 的点列
    std::vector<wxPoint> GetPath(const wxPoint& startWorld, PinSide startSide,
                                 const wxPoint& endWorld, PinSide endSide) const
    {
        if (!m_bendPoints.empty())
        {
            // 手动折点同样要做正交补全：直接串点会产生斜线段，
            // 因此逐段拼装，末段也一并补成横平竖直
            std::vector<wxPoint> path;
            path.reserve(m_bendPoints.size() * 2 + 2);
            AppendOrthogonalPoint(path, startWorld);
            wxPoint previous = startWorld;
            for (const wxPoint& bend : m_bendPoints)
            {
                AppendOrthogonalSegment(path, previous, bend);
                previous = bend;
            }
            AppendOrthogonalSegment(path, previous, endWorld);
            return path;
        }

        return BuildOrthogonalPath(startWorld, startSide, endWorld, endSide);
    }

private:
    int m_id;                          ///< 连线唯一 id
    PinRef m_from;                     ///< 起点引脚引用
    PinRef m_to;                       ///< 终点引脚引用
    std::vector<wxPoint> m_bendPoints;  ///< 手动折点（世界坐标），为空时使用自动正交走线
};

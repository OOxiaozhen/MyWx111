#include "Circuit.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

#include <wx/dcscreen.h>
#include <wx/font.h>

#include "GateComponents.h"

// ───────────────────────── 元件管理 ─────────────────────────

Component* Circuit::AddComponent(ComponentType type, const wxPoint& worldPos)
{
    const int newId = m_nextComponentId;
    std::unique_ptr<Component> created = ComponentFactory::Create(type, newId);
    if (!created)
    {
        return nullptr;
    }

    ++m_nextComponentId;
    created->SetPosition(worldPos);
    created->SetName(MakeDefaultName(type, newId));

    Component* raw = created.get();
    m_components.push_back(std::move(created));

    // 新建后立即选中它：用户刚放置的元件通常就是要继续调整的目标；
    // SelectOnly 内部已触发变更通知，这里无需重复调用 NotifyChanged
    SelectOnly(raw->GetId());

    // 选中本身不算结构变更，但新增元件算，需单独通知撤销栈
    NotifyStructureChanged();
    return raw;
}

void Circuit::Clear()
{
    // 先清空连线再清空元件：与 DeleteSelected 的约束一致，
    // 避免连线在解析端点时访问已释放的元件
    m_wires.clear();
    m_components.clear();

    // 选中集合与引脚占用索引都是对已删除对象的引用，必须一并重置
    m_selection.clear();
    m_selectedWires.clear();
    m_pinToWire.clear();

    // 注释与元件/连线无关，但属于文档内容，同样要清空
    m_notes.clear();
    m_selectedNoteId = -1;

    NotifyChanged();
    NotifyStructureChanged();
}

bool Circuit::RemoveComponent(int id)
{
    // 必须先删除挂接在该元件上的连线，否则残留的连线会持有指向已释放元件的 id
    RemoveWiresForComponent(id);

    const auto it = std::find_if(m_components.begin(), m_components.end(),
        [id](const std::unique_ptr<Component>& comp) { return comp->GetId() == id; });
    if (it == m_components.end())
    {
        return false;
    }

    m_components.erase(it);
    EraseFromSelection(id);
    NotifyChanged();
    NotifyStructureChanged();
    return true;
}

Component* Circuit::FindComponent(int id) const
{
    for (const auto& comp : m_components)
    {
        if (comp->GetId() == id)
        {
            // 容器本身在 const 方法内不可改动结构，但元素指向的对象允许修改
            return comp.get();
        }
    }
    return nullptr;
}

// ───────────────────────── 连线管理 ─────────────────────────

Wire* Circuit::AddWire(const PinRef& from, const PinRef& to)
{
    return AddWire(from, to, std::vector<wxPoint>{});
}

Wire* Circuit::AddWire(const PinRef& from, const PinRef& to,
                       const std::vector<wxPoint>& bendPoints)
{
    if (!CanConnect(from, to, nullptr))
    {
        return nullptr;
    }

    const int newId = m_nextWireId++;
    m_wires.push_back(std::make_unique<Wire>(newId, from, to));

    // 手动折点来自多击式走线时用户逐段确认的位置，入库后由 Wire::GetPath
    // 做正交补全，因此这里只需原样保存，不必预先整理
    if (!bendPoints.empty())
    {
        m_wires.back()->SetBendPoints(bendPoints);
    }

    // 引脚占用状态集中保存在 Circuit 侧：Pin 是 BuildPins() 返回的值对象，
    // 不会持有在元件内部，因此占用关系必须由模型统一登记。
    // 用 insert 而非 operator[]：multimap 下一个输出引脚可挂多条连线（扇出）
    m_pinToWire.insert({ { from.componentId, from.pinIndex }, newId });
    m_pinToWire.insert({ { to.componentId, to.pinIndex }, newId });

    NotifyChanged();
    NotifyStructureChanged();
    return m_wires.back().get();
}

bool Circuit::RemoveWire(int id)
{
    if (!RemoveWireInternal(id))
    {
        return false;
    }
    NotifyChanged();
    NotifyStructureChanged();
    return true;
}

bool Circuit::RemoveWireInternal(int wireId)
{
    const auto it = std::find_if(m_wires.begin(), m_wires.end(),
        [wireId](const std::unique_ptr<Wire>& wire) { return wire->GetId() == wireId; });
    if (it == m_wires.end())
    {
        return false;
    }

    const PinRef from = (*it)->GetFrom();
    const PinRef to = (*it)->GetTo();

    // multimap 下同一个键可能对应多条连线（输出扇出），
    // 必须只删除当前这条 wireId，不能把该引脚的所有连线一并抹掉
    const auto eraseOne = [this, wireId](const PinRef& pin)
    {
        const auto range = m_pinToWire.equal_range({ pin.componentId, pin.pinIndex });
        for (auto entry = range.first; entry != range.second; ++entry)
        {
            if (entry->second == wireId)
            {
                m_pinToWire.erase(entry);
                return;
            }
        }
    };
    eraseOne(from);
    eraseOne(to);

    m_wires.erase(it);
    return true;
}

bool Circuit::HasWireBetween(const PinRef& a, const PinRef& b) const
{
    const auto samePair = [&a, &b](const std::unique_ptr<Wire>& wire)
    {
        const PinRef& from = wire->GetFrom();
        const PinRef& to = wire->GetTo();
        // 方向不限：同一对引脚无论谁作起点都算同一根连线
        return (from == a && to == b) || (from == b && to == a);
    };

    return std::find_if(m_wires.begin(), m_wires.end(), samePair) != m_wires.end();
}

bool Circuit::CanStartWire(const PinRef& from, wxString* errorMsg) const
{
    const auto fail = [errorMsg](const wxString& text)
    {
        if (errorMsg)
        {
            *errorMsg = text;
        }
        return false;
    };

    if (!from.IsValid())
    {
        return fail(wxString(L"引脚引用无效"));
    }

    const Component* fromComp = FindComponent(from.componentId);
    if (!fromComp)
    {
        return fail(wxString(L"引脚所属元件不存在"));
    }

    const std::vector<Pin> fromPins = fromComp->GetTransformedPins();
    if (from.pinIndex < 0 || from.pinIndex >= static_cast<int>(fromPins.size()))
    {
        return fail(wxString(L"引脚序号越界"));
    }

    // 输出与双向引脚才能驱动连线；输入引脚只能当终点。
    // 在起点按下时就拦住，避免用户辛苦走完整条折线才被告知起点不合法；
    // 输出引脚已挂多条线属于正常扇出，这里刻意不判占用。
    const PinDirection dir = fromPins[static_cast<size_t>(from.pinIndex)].direction;
    if (dir != PinDirection::Output && dir != PinDirection::Bidi)
    {
        return fail(wxString(L"请从输出引脚开始连线"));
    }

    return true;
}

bool Circuit::CanConnect(const PinRef& from, const PinRef& to, wxString* errorMsg) const
{
    const auto fail = [errorMsg](const wxString& text)
    {
        if (errorMsg)
        {
            *errorMsg = text;
        }
        return false;
    };

    if (!from.IsValid() || !to.IsValid())
    {
        return fail(wxString(L"引脚引用无效"));
    }

    if (from.componentId == to.componentId)
    {
        return fail(wxString(L"不能把元件连到自己身上"));
    }

    const Component* fromComp = FindComponent(from.componentId);
    const Component* toComp = FindComponent(to.componentId);
    if (!fromComp || !toComp)
    {
        return fail(wxString(L"引脚所属元件不存在"));
    }

    const std::vector<Pin> fromPins = fromComp->GetTransformedPins();
    const std::vector<Pin> toPins = toComp->GetTransformedPins();
    if (from.pinIndex >= static_cast<int>(fromPins.size()) ||
        to.pinIndex >= static_cast<int>(toPins.size()))
    {
        return fail(wxString(L"引脚序号越界"));
    }

    const PinDirection fromDir = fromPins[static_cast<size_t>(from.pinIndex)].direction;
    const PinDirection toDir = toPins[static_cast<size_t>(to.pinIndex)].direction;

    const bool fromIsSource = (fromDir == PinDirection::Output || fromDir == PinDirection::Bidi);
    const bool toIsSink = (toDir == PinDirection::Input || toDir == PinDirection::Bidi);

    if (!fromIsSource)
    {
        return fail(wxString(L"请从输出引脚开始连线"));
    }
    if (!toIsSink)
    {
        return fail(wxString(L"连线终点必须是输入引脚"));
    }

    // 扇出规则：输出引脚允许挂多条连线（一个输出驱动多个输入），
    // 因此起点不判占用；输入引脚仍只允许一个驱动源，避免多驱动争主。
    if (m_pinToWire.count({ to.componentId, to.pinIndex }) != 0)
    {
        return fail(wxString(L"该输入引脚已被其他连线占用"));
    }

    // 扇出后容易把同一对引脚反复连两次，结果只是两根完全重合的线，需去重
    if (HasWireBetween(from, to))
    {
        return fail(wxString(L"这两个引脚之间已经存在连线"));
    }

    return true;
}

std::vector<wxPoint> Circuit::GetWirePath(const Wire& wire) const
{
    const Component* fromComp = FindComponent(wire.GetFrom().componentId);
    const Component* toComp = FindComponent(wire.GetTo().componentId);
    if (!fromComp || !toComp)
    {
        return {};
    }

    const wxPoint start = fromComp->GetPinWorldPos(wire.GetFrom().pinIndex);
    const wxPoint end = toComp->GetPinWorldPos(wire.GetTo().pinIndex);

    // 走线需要引脚几何朝向：从哪一侧出线决定了引出段方向，缺了朝向只能连出斜线
    const PinSide startSide = fromComp->GetPinSide(wire.GetFrom().pinIndex);
    const PinSide endSide = toComp->GetPinSide(wire.GetTo().pinIndex);
    return wire.GetPath(start, startSide, end, endSide);
}

void Circuit::PopulatePinStates(int componentId, std::vector<Pin>& pins) const
{
    for (Pin& pin : pins)
    {
        const auto range = m_pinToWire.equal_range({ componentId, pin.index });
        pin.connected = (range.first != range.second);
        // 扇出时一个引脚对应多条连线，Pin 只能记录首条；
        // 需要完整列表的调用方应使用 GetWiresForPin
        pin.wireId = pin.connected ? range.first->second : -1;
    }
}

std::vector<int> Circuit::GetWiresForPin(int componentId, int pinIndex) const
{
    const auto range = m_pinToWire.equal_range({ componentId, pinIndex });
    std::vector<int> wireIds;
    for (auto entry = range.first; entry != range.second; ++entry)
    {
        wireIds.push_back(entry->second);
    }
    return wireIds;
}

// ───────────────────────── 分叉圆点（junction）─────────────────────────

namespace
{
    /// wxPoint 的严格弱序，供 std::map 当键使用（wxPoint 没有 operator<）。
    /// 用 map 而非 unordered_map 是为了让 ComputeJunctions 的输出顺序稳定，
    /// 避免同一张图每次重绘得到不同的圆点顺序（缓存比对与调试都依赖确定性）。
    struct JunctionPointLess
    {
        bool operator()(const wxPoint& a, const wxPoint& b) const
        {
            return a.x != b.x ? a.x < b.x : a.y < b.y;
        }
    };

    /// 判断点是否严格落在一条线段的内部（不含两端点）。
    /// 本项目的走线被强制为正交折线，因此只按同列 / 同行两种情形比较坐标区间，
    /// 斜线段直接返回 false：与其做通用几何判定，不如让斜线漏判，
    /// 因为斜线在本工程里本就代表走线算法出错，无圆点反而更容易被发现。
    /// @param pt 待判定的世界坐标点
    /// @param a  线段起点
    /// @param b  线段终点
    /// @return 点位于线段内部（严格在两端之间）时返回 true
    bool IsPointInsideSegment(const wxPoint& pt, const wxPoint& a, const wxPoint& b)
    {
        if (a == b)
        {
            return false; // 退化线段没有内部
        }

        if (a.y == b.y)
        {
            const int minX = std::min(a.x, b.x);
            const int maxX = std::max(a.x, b.x);
            return pt.y == a.y && pt.x > minX && pt.x < maxX;
        }

        if (a.x == b.x)
        {
            const int minY = std::min(a.y, b.y);
            const int maxY = std::max(a.y, b.y);
            return pt.x == a.x && pt.y > minY && pt.y < maxY;
        }

        return false; // 斜线段不参与 junction 判定
    }
}

std::vector<Circuit::Junction> Circuit::ComputeJunctions() const
{
    // 先把所有连线的正交路径取出来，后续判定全部在几何层完成，不再回查模型
    struct WirePath
    {
        int id = -1;                    ///< 连线 id
        std::vector<wxPoint> points;    ///< 该连线的完整折线路径（世界坐标）
    };

    std::vector<WirePath> paths;
    paths.reserve(m_wires.size());
    for (const auto& wire : m_wires)
    {
        std::vector<wxPoint> points = GetWirePath(*wire);
        if (points.size() < 2)
        {
            continue; // 端点失效（元件已删除）的连线不参与分叉统计
        }
        paths.push_back(WirePath{ wire->GetId(), std::move(points) });
    }

    // 交汇点 → 聚合信息
    struct Meeting
    {
        std::vector<int> wireIds; ///< 交汇于该点的连线 id（去重）
        int incidentEnds = 0;     ///< 汇入线段端点数：连线端点算 1，内部折点算 2
        int endWireCount = 0;     ///< 以该点作为自身端点的连线数量，用于识别引脚扇出
    };

    std::map<wxPoint, Meeting, JunctionPointLess> meetings;

    // 第一遍：登记每条连线自己的端点与折点
    for (const WirePath& path : paths)
    {
        const wxPoint& first = path.points.front();
        const wxPoint& last = path.points.back();

        Meeting& head = meetings[first];
        head.wireIds.push_back(path.id);
        head.incidentEnds += 1;
        head.endWireCount += 1;

        if (!(last == first))
        {
            Meeting& tail = meetings[last];
            tail.wireIds.push_back(path.id);
            tail.incidentEnds += 1;
            tail.endWireCount += 1;
        }

        // 内部折点：一条线在此拐弯，等效两条线段在此相接，故计 2
        for (size_t i = 1; i + 1 < path.points.size(); ++i)
        {
            Meeting& inner = meetings[path.points[i]];
            inner.wireIds.push_back(path.id);
            inner.incidentEnds += 2;
        }
    }

    // 第二遍：补上「T 形分叉」——另一条线的端点或折点落在本线段的内部，
    // 而该点又不是本连线自己的顶点，第一遍统计不到，必须做点在段上的判定
    for (auto& entry : meetings)
    {
        Meeting& meeting = entry.second;
        for (const WirePath& path : paths)
        {
            if (std::find(meeting.wireIds.begin(), meeting.wireIds.end(), path.id) !=
                meeting.wireIds.end())
            {
                continue; // 该连线已在此点登记过（自身端点或折点）
            }

            bool crossesHere = false;
            for (size_t i = 0; i + 1 < path.points.size(); ++i)
            {
                if (IsPointInsideSegment(entry.first, path.points[i], path.points[i + 1]))
                {
                    crossesHere = true;
                    break;
                }
            }

            if (crossesHere)
            {
                meeting.wireIds.push_back(path.id);
                meeting.incidentEnds += 2; // 从线段中间穿过，视为两个线段端点
            }
        }
    }

    // 第三遍：按规则筛出真正的分叉点
    std::vector<Junction> junctions;
    for (const auto& entry : meetings)
    {
        const Meeting& meeting = entry.second;
        if (meeting.wireIds.size() < 2)
        {
            continue; // 只有一条线经过（自身拐弯），不是分叉
        }

        // 同一引脚扇出时汇入线段数只有 2（两条线共用引脚端点），但视觉上就是分叉处，
        // 因此把「两条以上连线共享同一端点」也当作需要圆点的情形单独放行
        const bool pinFanout = meeting.endWireCount >= 2;
        if (meeting.incidentEnds < 3 && !pinFanout)
        {
            continue;
        }

        Junction junction;
        junction.position = entry.first;
        junction.wireIds = meeting.wireIds;
        junction.incidentEnds = meeting.incidentEnds;
        junction.atPinFanout = pinFanout;
        junctions.push_back(std::move(junction));
    }

    // meetings 以 (x, y) 为键，遍历顺序天然满足「按坐标升序」的约定，无需再排序
    return junctions;
}

// ───────────────────────── 命中与选中 ─────────────────────────

Component* Circuit::HitTestComponent(const wxPoint& worldPt) const
{
    // 逆序遍历：后添加的元件绘制在更上层，命中测试也应优先命中它
    for (auto it = m_components.rbegin(); it != m_components.rend(); ++it)
    {
        if ((*it)->HitTest(worldPt))
        {
            return it->get();
        }
    }
    return nullptr;
}

PinRef Circuit::HitTestPin(const wxPoint& worldPt, int tolerance) const
{
    for (auto it = m_components.rbegin(); it != m_components.rend(); ++it)
    {
        const Component& comp = **it;
        const std::vector<Pin> pins = comp.GetTransformedPins();
        for (const Pin& pin : pins)
        {
            const wxPoint worldPos = comp.GetPosition() + pin.offset;
            if (std::abs(worldPos.x - worldPt.x) <= tolerance &&
                std::abs(worldPos.y - worldPt.y) <= tolerance)
            {
                return PinRef{ comp.GetId(), pin.index };
            }
        }
    }
    return PinRef{};
}

int Circuit::HitTestWire(const wxPoint& worldPt, int tolerance) const
{
    // 点到线段的最短距离：返回距离平方，全程整数运算避开浮点误差，
    // 因此比较时与 tolerance 的平方对照即可
    const auto distanceSquared = [](const wxPoint& pt, const wxPoint& a, const wxPoint& b)
    {
        const long long dx = b.x - a.x;
        const long long dy = b.y - a.y;
        const long long lengthSquared = dx * dx + dy * dy;
        if (lengthSquared == 0)
        {
            // 退化线段（两端点重合）：等价于点到点距离
            const long long px = pt.x - a.x;
            const long long py = pt.y - a.y;
            return px * px + py * py;
        }

        // 把 pt 投影到线段参数上，越界时退化为到最近端点的距离
        const long long vx = pt.x - a.x;
        const long long vy = pt.y - a.y;
        const long long t = vx * dx + vy * dy;
        if (t <= 0)
        {
            return vx * vx + vy * vy;
        }
        if (t >= lengthSquared)
        {
            const long long px = pt.x - b.x;
            const long long py = pt.y - b.y;
            return px * px + py * py;
        }

        // 投影落在线段内部：用叉积算垂直距离。
        // 分子与分母同时放大 lengthSquared 倍，避免整数除法提前截断
        const long long scaledX = vx * lengthSquared - t * dx;
        const long long scaledY = vy * lengthSquared - t * dy;
        return (scaledX * scaledX + scaledY * scaledY) / (lengthSquared * lengthSquared);
    };

    const long long toleranceSquared = static_cast<long long>(tolerance) * tolerance;

    // 逆序遍历：后添加的连线绘制在上层，命中测试也应优先命中它
    int hitId = -1;
    long long bestDistance = 0;
    for (auto it = m_wires.rbegin(); it != m_wires.rend(); ++it)
    {
        const std::vector<wxPoint> path = GetWirePath(**it);
        for (size_t i = 1; i < path.size(); ++i)
        {
            const long long distance = distanceSquared(worldPt, path[i - 1], path[i]);
            if (distance > toleranceSquared)
            {
                continue;
            }
            if (hitId < 0 || distance < bestDistance)
            {
                hitId = (*it)->GetId();
                bestDistance = distance;
            }
        }
    }
    return hitId;
}

bool Circuit::IsWireSelected(int wireId) const
{
    return std::find(m_selectedWires.begin(), m_selectedWires.end(), wireId)
           != m_selectedWires.end();
}

void Circuit::SelectOnlyWire(int wireId)
{
    const auto exists = std::find_if(m_wires.begin(), m_wires.end(),
        [wireId](const std::unique_ptr<Wire>& wire) { return wire->GetId() == wireId; });
    if (exists == m_wires.end())
    {
        return;
    }

    // 元件、连线、注释三类选中互斥：选中连线时清掉另外两类
    m_selection.clear();
    m_selectedWires.clear();
    m_selectedNoteId = -1;
    m_selectedWires.push_back(wireId);
    NotifyChanged();
}

void Circuit::ToggleWireSelect(int wireId)
{
    const auto exists = std::find_if(m_wires.begin(), m_wires.end(),
        [wireId](const std::unique_ptr<Wire>& wire) { return wire->GetId() == wireId; });
    if (exists == m_wires.end())
    {
        return;
    }

    const auto it = std::find(m_selectedWires.begin(), m_selectedWires.end(), wireId);
    if (it != m_selectedWires.end())
    {
        m_selectedWires.erase(it);
    }
    else
    {
        m_selectedWires.push_back(wireId);
    }
    NotifyChanged();
}

void Circuit::ClearSelection()
{
    if (m_selection.empty() && m_selectedWires.empty() && m_selectedNoteId < 0)
    {
        return;
    }
    m_selection.clear();
    m_selectedWires.clear();
    m_selectedNoteId = -1;
    NotifyChanged();
}

void Circuit::SelectOnly(int componentId)
{
    if (FindComponent(componentId) == nullptr)
    {
        return;
    }
    m_selection.clear();
    m_selection.push_back(componentId);
    m_selectedNoteId = -1;
    NotifyChanged();
}

void Circuit::ToggleSelect(int componentId)
{
    if (FindComponent(componentId) == nullptr)
    {
        return;
    }

    const auto it = std::find(m_selection.begin(), m_selection.end(), componentId);
    if (it != m_selection.end())
    {
        m_selection.erase(it);
    }
    else
    {
        m_selection.push_back(componentId);
    }
    NotifyChanged();
}

void Circuit::SelectAll()
{
    m_selection.clear();
    m_selection.reserve(m_components.size());
    for (const auto& comp : m_components)
    {
        m_selection.push_back(comp->GetId());
    }
    NotifyChanged();
}

bool Circuit::IsSelected(int componentId) const
{
    return std::find(m_selection.begin(), m_selection.end(), componentId) != m_selection.end();
}

int Circuit::GetPrimarySelection() const
{
    return m_selection.empty() ? -1 : m_selection.front();
}

void Circuit::EraseFromSelection(int componentId)
{
    const auto it = std::find(m_selection.begin(), m_selection.end(), componentId);
    if (it != m_selection.end())
    {
        m_selection.erase(it);
    }
}

// ───────────────────────── 属性修改 ─────────────────────────

void Circuit::SetComponentPosition(int componentId, const wxPoint& worldPos)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetPosition() == worldPos)
    {
        return;
    }
    comp->SetPosition(worldPos);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentPositionSilent(int componentId, const wxPoint& worldPos)
{
    if (Component* comp = FindComponent(componentId))
    {
        comp->SetPosition(worldPos);
    }
}

void Circuit::SetComponentName(int componentId, const wxString& name)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetName() == name)
    {
        return;
    }
    comp->SetName(name);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentBitWidth(int componentId, int width)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetBitWidth() == width)
    {
        return;
    }

    // 当前所有元件的引脚数都固定（分线器固定 3 路输出），因此位宽变化不会让已有连线失效；
    // 若后续引入引脚数随位宽变化的元件，这里需要清理越界引脚上的连线
    comp->SetBitWidth(width);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentOrientation(int componentId, int degrees)
{
    Component* comp = FindComponent(componentId);
    if (!comp)
    {
        return;
    }
    comp->SetOrientation(degrees);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentRefDes(int componentId, const wxString& refDes)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetRefDes() == refDes)
    {
        return;
    }
    comp->SetRefDes(refDes);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentFootprint(int componentId, const wxString& footprint)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetFootprint() == footprint)
    {
        return;
    }
    comp->SetFootprint(footprint);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentValue(int componentId, const wxString& value)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetValue() == value)
    {
        return;
    }
    comp->SetValue(value);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentSlot(int componentId, int slot)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetSlot() == slot)
    {
        return;
    }
    comp->SetSlot(slot);
    NotifyChanged();
    NotifyStructureChanged();
}

bool Circuit::ToggleInputLevel(int componentId)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::InputPin)
    {
        return false;
    }

    InputPin* pin = static_cast<InputPin*>(comp);
    pin->SetLevel(pin->GetLevel() == LogicLevel::High ? LogicLevel::Low : LogicLevel::High);

    // 走结构变更通知：切换电平是一次真实的文档修改，应当可被 Ctrl+Z 回退
    NotifyChanged();
    NotifyStructureChanged();
    return true;
}

bool Circuit::ToggleLevelByCircle(const wxPoint& worldPt)
{
    // 半径取本体边长的 1/4：既能可靠点中，又不会吃掉方框四角的选中热区
    constexpr int kToggleRadius = kComponentBodySize / 4;

    for (const auto& compPtr : m_components)
    {
        if (!compPtr || !compPtr->HasLevelToggle())
        {
            continue;
        }

        // 本体包围盒宽高相等且旋转不变（见 Component::GetWorldBounds 的说明），
        // 因此直接用世界包围盒中心作为圆心，无需再套一层朝向变换
        const wxRect bounds = compPtr->GetWorldBounds();
        const wxPoint center(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
        const wxPoint delta = worldPt - center;
        if (delta.x * delta.x + delta.y * delta.y > kToggleRadius * kToggleRadius)
        {
            continue;
        }

        const int componentId = compPtr->GetId();
        switch (compPtr->GetType())
        {
        case ComponentType::InputPin:
            ToggleInputLevel(componentId);
            return true;
        case ComponentType::LevelSource:
        {
            const auto* source = static_cast<const LevelSource*>(compPtr.get());
            SetComponentSourceLevel(
                componentId,
                source->GetLevel() == LogicLevel::High ? LogicLevel::Low : LogicLevel::High);
            return true;
        }
        default:
            break;
        }
    }

    return false;
}

void Circuit::SetComponentInputLevel(int componentId, LogicLevel level)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::InputPin)
    {
        return;
    }

    InputPin* pin = static_cast<InputPin*>(comp);
    if (pin->GetLevel() == level)
    {
        return;
    }
    pin->SetLevel(level);
    NotifyChanged();
    NotifyStructureChanged();
}

LogicLevel Circuit::GetComponentInputLevel(int componentId) const
{
    const Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::InputPin)
    {
        return LogicLevel::Unknown;
    }
    return static_cast<const InputPin*>(comp)->GetLevel();
}

void Circuit::SetComponentClockFrequency(int componentId, int hz)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::ClockSource)
    {
        return;
    }

    ClockSource* clock = static_cast<ClockSource*>(comp);
    if (clock->GetFrequencyHz() == hz)
    {
        return;
    }
    clock->SetFrequencyHz(hz);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentClockDuty(int componentId, double duty)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::ClockSource)
    {
        return;
    }

    ClockSource* clock = static_cast<ClockSource*>(comp);
    // 浮点比较用容差，避免 UI 输入 0.5 与内部存储值因精度差异反复触发变更
    if (std::abs(clock->GetDutyCycle() - duty) < 1e-9)
    {
        return;
    }
    clock->SetDutyCycle(duty);
    NotifyChanged();
    NotifyStructureChanged();
}

int Circuit::GetComponentClockFrequency(int componentId) const
{
    const Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::ClockSource)
    {
        return 0;
    }
    return static_cast<const ClockSource*>(comp)->GetFrequencyHz();
}

double Circuit::GetComponentClockDuty(int componentId) const
{
    const Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::ClockSource)
    {
        return 0.0;
    }
    return static_cast<const ClockSource*>(comp)->GetDutyCycle();
}

void Circuit::SetComponentText(int componentId, const wxString& text)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::TextLabel)
    {
        return;
    }

    TextLabel* label = static_cast<TextLabel*>(comp);
    if (label->GetText() == text)
    {
        return;
    }
    label->SetText(text);
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetComponentSourceLevel(int componentId, LogicLevel level)
{
    Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::LevelSource)
    {
        return;
    }

    auto* source = static_cast<LevelSource*>(comp);
    if (source->GetLevel() == level)
    {
        return;
    }
    source->SetLevel(level);
    NotifyChanged();
    NotifyStructureChanged();
}

LogicLevel Circuit::GetComponentSourceLevel(int componentId) const
{
    const Component* comp = FindComponent(componentId);
    if (!comp || comp->GetType() != ComponentType::LevelSource)
    {
        return LogicLevel::Unknown;
    }
    return static_cast<const LevelSource*>(comp)->GetLevel();
}

std::vector<Circuit::ChipGroup> Circuit::GetChipGroups() const
{
    // 74xx 四门器件的标准形态：一颗 DIP14 封装内含 4 个独立的门
    constexpr size_t kGatesPerChip = 4;

    std::vector<ChipGroup> groups;

    // 显式填了 refDes 的元件按 refDes 聚合；未填的先按类型分桶，稍后自动分配
    std::map<wxString, ChipGroup> explicitGroups;
    std::map<int, std::vector<const Component*>> autoBuckets;

    for (const auto& compPtr : m_components)
    {
        if (!compPtr)
        {
            continue;
        }
        const Component& comp = *compPtr;

        // Splitter 是纯逻辑元件（把一根线分成多路），不占物理封装
        if (comp.GetType() == ComponentType::Splitter)
        {
            continue;
        }

        if (!comp.GetRefDes().empty())
        {
            ChipGroup& group = explicitGroups[comp.GetRefDes()];
            group.refDes = comp.GetRefDes();
            group.gates.emplace_back(comp.GetId(), comp.GetSlot());

            // 同一片芯片的多个门通常填相同的型号/封装，
            // 取组内首个非空值即可表达整片芯片
            if (group.value.empty() && !comp.GetValue().empty())
            {
                group.value = comp.GetValue();
            }
            if (group.footprint.empty() && !comp.GetFootprint().empty())
            {
                group.footprint = comp.GetFootprint();
            }
        }
        else
        {
            autoBuckets[static_cast<int>(comp.GetType())].push_back(&comp);
        }
    }

    for (const auto& entry : explicitGroups)
    {
        groups.push_back(entry.second);
    }

    // 自动分配的编号从显式组已用的最大数字之后继续，避免与用户填写的编号撞车
    int nextIndex = 1;
    for (const ChipGroup& group : groups)
    {
        wxString digits;
        for (const wxUniChar ch : group.refDes)
        {
            if (ch >= L'0' && ch <= L'9')
            {
                digits += ch;
            }
        }

        long parsed = 0;
        if (!digits.empty() && digits.ToLong(&parsed))
        {
            nextIndex = std::max(nextIndex, static_cast<int>(parsed) + 1);
        }
    }

    for (const auto& bucket : autoBuckets)
    {
        const std::vector<const Component*>& comps = bucket.second;
        for (size_t i = 0; i < comps.size(); i += kGatesPerChip)
        {
            ChipGroup group;
            group.refDes = wxString::Format(L"U%d", nextIndex++);
            group.autoAssigned = true;

            const size_t end = std::min(i + kGatesPerChip, comps.size());
            int slot = 1;
            for (size_t j = i; j < end; ++j, ++slot)
            {
                group.gates.emplace_back(comps[j]->GetId(), slot);
                if (group.value.empty() && !comps[j]->GetValue().empty())
                {
                    group.value = comps[j]->GetValue();
                }
                if (group.footprint.empty() && !comps[j]->GetFootprint().empty())
                {
                    group.footprint = comps[j]->GetFootprint();
                }
            }
            groups.push_back(group);
        }
    }

    std::sort(groups.begin(), groups.end(),
              [](const ChipGroup& lhs, const ChipGroup& rhs) { return lhs.refDes < rhs.refDes; });
    return groups;
}

bool Circuit::HasSlotConflict(wxString* message) const
{
    const std::vector<ChipGroup> groups = GetChipGroups();
    for (const ChipGroup& group : groups)
    {
        // 自动分配的组由本方法生成，slot 天然连续唯一，无需校验
        if (group.autoAssigned)
        {
            continue;
        }

        std::set<int> usedSlots;
        for (const auto& gate : group.gates)
        {
            if (!usedSlots.insert(gate.second).second)
            {
                if (message != nullptr)
                {
                    *message =
                        wxString::Format(L"%s 的 slot %d 被多个门占用。", group.refDes, gate.second);
                }
                return true;
            }
        }
    }
    return false;
}

void Circuit::RotateSelected()
{
    if (m_selection.empty())
    {
        return;
    }
    for (int id : m_selection)
    {
        if (Component* comp = FindComponent(id))
        {
            comp->Rotate();
        }
    }
    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::DeleteSelected()
{
    if (m_selection.empty() && m_selectedWires.empty() && m_selectedNoteId < 0)
    {
        return;
    }

    // 先删除选中的连线：连线不依附元件，单独处理即可。
    // 这里走 RemoveWireInternal，避免每删一条都触发一次变更通知
    const std::vector<int> wireIds = m_selectedWires;
    for (int id : wireIds)
    {
        RemoveWireInternal(id);
    }
    m_selectedWires.clear();

    // 再删除选中的注释：注释不依附元件，单独处理
    if (m_selectedNoteId >= 0)
    {
        const int noteId = m_selectedNoteId;
        m_selectedNoteId = -1;
        m_notes.erase(std::remove_if(m_notes.begin(), m_notes.end(),
                                     [noteId](const Note& note) { return note.id == noteId; }),
                      m_notes.end());
    }

    // 先复制一份 id：下面会清空 m_selection，且删除过程中不能依赖它的内容
    const std::vector<int> ids = m_selection;

    // 顺序很重要：必须先清理连线，再删除元件，避免连线解析到已释放的元件
    for (int id : ids)
    {
        RemoveWiresForComponent(id);
    }

    m_components.erase(
        std::remove_if(m_components.begin(), m_components.end(),
            [&ids](const std::unique_ptr<Component>& comp)
            {
                return std::find(ids.begin(), ids.end(), comp->GetId()) != ids.end();
            }),
        m_components.end());

    m_selection.clear();
    NotifyChanged();
    NotifyStructureChanged();
}

// ───────────────────────── 画布注释 ─────────────────────────

int Circuit::AddNote(const wxPoint& worldPos, const wxString& text)
{
    Note note;
    note.id = m_nextNoteId++;
    note.position = worldPos;
    note.text = text;
    note.size = MeasureNoteSize(text);

    m_notes.push_back(note);
    NotifyChanged();
    NotifyStructureChanged();
    return note.id;
}

bool Circuit::RemoveNote(int id)
{
    const auto it = std::find_if(m_notes.begin(), m_notes.end(),
                                 [id](const Note& note) { return note.id == id; });
    if (it == m_notes.end())
    {
        return false;
    }

    m_notes.erase(it);
    if (m_selectedNoteId == id)
    {
        m_selectedNoteId = -1;
    }

    NotifyChanged();
    NotifyStructureChanged();
    return true;
}

void Circuit::SetNoteText(int id, const wxString& text)
{
    Note* note = FindNote(id);
    if (note == nullptr || note->text == text)
    {
        return;
    }

    note->text = text;
    // 文本变化会改变包围盒，必须同步刷新，否则命中区域与显示不符
    note->size = MeasureNoteSize(text);

    NotifyChanged();
    NotifyStructureChanged();
}

void Circuit::SetNotePositionSilent(int id, const wxPoint& worldPos)
{
    if (Note* note = FindNote(id))
    {
        note->position = worldPos;
    }
}

Note* Circuit::FindNote(int id)
{
    for (Note& note : m_notes)
    {
        if (note.id == id)
        {
            return &note;
        }
    }
    return nullptr;
}

const Note* Circuit::FindNote(int id) const
{
    for (const Note& note : m_notes)
    {
        if (note.id == id)
        {
            return &note;
        }
    }
    return nullptr;
}

Note* Circuit::HitTestNote(const wxPoint& worldPt) const
{
    // 逆序：后插入的注释绘制在上层，命中测试也应优先命中它
    for (auto it = m_notes.rbegin(); it != m_notes.rend(); ++it)
    {
        if (wxRect(it->position, it->size).Contains(worldPt))
        {
            return const_cast<Note*>(&(*it));
        }
    }
    return nullptr;
}

void Circuit::SelectOnlyNote(int id)
{
    if (FindNote(id) == nullptr)
    {
        return;
    }

    // 三类选中互斥：选中注释时清掉元件与连线，避免多处同时高亮
    m_selection.clear();
    m_selectedWires.clear();
    m_selectedNoteId = id;
    NotifyChanged();
}

wxSize Circuit::MeasureNoteSize(const wxString& text)
{
    // 注释在世界坐标里排版：zoom=1 时 1 世界单位 = 1 像素，
    // 因此直接用屏幕 DC 的字体度量结果作为世界单位尺寸
    constexpr int kPadding = 8;
    constexpr int kMinWidth = 40;
    constexpr int kMinHeight = 20;

    wxFont font(wxFontInfo(10));
    wxScreenDC dc;
    dc.SetFont(font);

    const wxString content = text.empty() ? wxString(L"(空注释)") : text;

    int lineHeight = 0;
    dc.GetTextExtent(wxString(L"Hg"), &lineHeight, &lineHeight);

    int maxWidth = 0;
    int lineCount = 0;
    wxString remaining = content;
    while (true)
    {
        const int newlinePos = remaining.Find(L'\n');
        const wxString line = (newlinePos == wxNOT_FOUND) ? remaining : remaining.Left(newlinePos);

        int lineWidth = 0;
        int unusedHeight = 0;
        dc.GetTextExtent(line, &lineWidth, &unusedHeight);
        maxWidth = std::max(maxWidth, lineWidth);
        ++lineCount;

        if (newlinePos == wxNOT_FOUND)
        {
            break;
        }
        remaining = remaining.Mid(static_cast<size_t>(newlinePos) + 1);
    }

    // 行高留 2 像素行距，整体高度另加一份内边距
    const int width = std::max(kMinWidth, maxWidth + kPadding * 2);
    const int height = std::max(kMinHeight, lineCount * (lineHeight + 2) + kPadding);
    return wxSize(width, height);
}

// ───────────────────────── 变更通知 ─────────────────────────

void Circuit::NotifyChanged() const
{
    // 抑制窗口内不派发：反序列化回放会反复改动模型，
    // 若每次都刷新画布与属性面板，会产生大量无效重绘
    if (m_notifySuppressDepth > 0)
    {
        return;
    }

    if (m_changeHandler)
    {
        m_changeHandler();
    }
}

void Circuit::NotifyStructureChanged() const
{
    // 同样受抑制：回放期间的变更不得进入撤销栈，否则会形成递归
    if (m_notifySuppressDepth > 0)
    {
        return;
    }

    if (m_structureChangeHandler)
    {
        m_structureChangeHandler();
    }
}

void Circuit::BeginSuppressNotifications()
{
    ++m_notifySuppressDepth;
}

void Circuit::EndSuppressNotifications()
{
    if (m_notifySuppressDepth > 0)
    {
        --m_notifySuppressDepth;
    }
}

// ───────────────────────── 私有辅助 ─────────────────────────

wxString Circuit::MakeDefaultName(ComponentType type, int id) const
{
    return wxString::Format(L"%s %d", GetComponentDisplayName(type), id);
}

void Circuit::RemoveWiresForComponent(int componentId)
{
    // 先收集待删连线 id：直接在遍历中删除容器元素会破坏迭代器
    std::vector<int> wireIds;
    for (const auto& wire : m_wires)
    {
        if (wire->GetFrom().componentId == componentId ||
            wire->GetTo().componentId == componentId)
        {
            wireIds.push_back(wire->GetId());
        }
    }

    for (int id : wireIds)
    {
        RemoveWireInternal(id);
    }
}

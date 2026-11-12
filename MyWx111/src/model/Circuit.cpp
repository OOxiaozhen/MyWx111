#include "Circuit.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>

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
    // 不会持有在元件内部，因此占用关系必须由模型统一登记
    m_pinToWire[{ from.componentId, from.pinIndex }] = newId;
    m_pinToWire[{ to.componentId, to.pinIndex }] = newId;

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

    m_pinToWire.erase({ from.componentId, from.pinIndex });
    m_pinToWire.erase({ to.componentId, to.pinIndex });

    m_wires.erase(it);
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

    if (m_pinToWire.count({ from.componentId, from.pinIndex }) != 0 ||
        m_pinToWire.count({ to.componentId, to.pinIndex }) != 0)
    {
        return fail(wxString(L"该引脚已被其他连线占用"));
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
        const auto it = m_pinToWire.find({ componentId, pin.index });
        pin.connected = (it != m_pinToWire.end());
        pin.wireId = pin.connected ? it->second : -1;
    }
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

    // 元件与连线两类选中互斥：选中连线时清掉元件选中，避免属性面板显示上一个元件
    m_selection.clear();
    m_selectedWires.clear();
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
    if (m_selection.empty() && m_selectedWires.empty())
    {
        return;
    }
    m_selection.clear();
    m_selectedWires.clear();
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
    if (m_selection.empty() && m_selectedWires.empty())
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

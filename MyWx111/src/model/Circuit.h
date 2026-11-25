#pragma once

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/string.h>

#include "Component.h"
#include "Wire.h"

/// 画布文本注释。不是元件：不参与网表导出、封装映射与连通性计算，
/// 也不进 m_components，从而与元件/连线的命中、框选、序列化路径彻底隔离。
struct Note
{
    int id = -1;      ///< 注释唯一 id（与元件/连线 id 计数器独立）
    wxPoint position; ///< 包围盒左上角，世界坐标
    wxString text;    ///< 注释正文，可含 '\n' 多行
    wxSize size;      ///< 世界坐标包围盒尺寸，由 MeasureNoteSize 在插入/改文本时刷新
};

/// 电路文档模型：独占持有全部元件与连线，并维护选中集合与引脚占用索引。
/// 任何结构性修改后都会调用 NotifyChanged()，由外部（MainFrame）驱动重绘与面板刷新。
class Circuit
{
public:
    /// 模型变更通知回调类型。
    using ChangeHandler = std::function<void()>;

    Circuit() = default;
    ~Circuit() = default;

    // 持有 unique_ptr 且回调与视图强绑定，拷贝无意义
    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;

    // ── 元件管理 ──
    /// 新建元件并加入电路，同时选中它。
    /// @param type     元件类型
    /// @param worldPos 元件世界坐标原点（调用方负责吸附网格）
    /// @return 新建元件的裸指针（所有权仍在 Circuit）；类型未知时返回 nullptr
    Component* AddComponent(ComponentType type, const wxPoint& worldPos);

    /// 删除指定元件及其挂接的全部连线。
    /// @param id 元件 id
    /// @return 删除成功返回 true；id 不存在返回 false
    bool RemoveComponent(int id);

    /// 清空电路：删除全部连线与元件，并重置选中集合与引脚占用索引。
    /// 不重置 id 计数器，避免与已释放对象的外部引用产生混淆。
    void Clear();

    /// @param id 元件 id
    /// @return 找到返回裸指针，否则 nullptr
    /// @note 形参为 const 只是为了在 const 方法（如连接校验）中复用；
    ///       容器元素的指向目标本身允许被修改，故此处 const_cast 是安全的
    Component* FindComponent(int id) const;

    /// @return 全部元件的只读容器
    const std::vector<std::unique_ptr<Component>>& GetComponents() const { return m_components; }

    // ── 连线管理 ──
    /// 在两个引脚之间建立连线，并登记两端引脚的占用关系。
    /// @param from 起点引脚（必须是 Output 或 Bidi）
    /// @param to   终点引脚（必须是 Input 或 Bidi）
    /// @return 新建连线指针；参数非法或引脚已占用时返回 nullptr
    Wire* AddWire(const PinRef& from, const PinRef& to);

    /// 在两端引脚之间建立连线，并使用指定的手动折点（世界坐标）。
    /// 手动折点由走线交互逐段确认，最终路径仍会被补成横平竖直。
    /// @param from       起点引脚（必须是 Output 或 Bidi）
    /// @param to         终点引脚（必须是 Input 或 Bidi）
    /// @param bendPoints 手动折点（世界坐标）；传空列表等同于自动正交走线
    /// @return 新建连线指针；参数非法或引脚已占用时返回 nullptr
    Wire* AddWire(const PinRef& from, const PinRef& to, const std::vector<wxPoint>& bendPoints);

    /// 删除连线，并复位两端引脚的占用记录。
    /// @param id 连线 id
    /// @return 删除成功返回 true
    bool RemoveWire(int id);

    /// @return 全部连线的只读容器
    const std::vector<std::unique_ptr<Wire>>& GetWires() const { return m_wires; }

    /// 校验两个引脚能否连接。连接规则集中在此，画布只负责展示错误文本。
    /// @param from     起点引脚
    /// @param to       终点引脚
    /// @param[out] errorMsg 非法时写入中文提示；可为 nullptr
    /// @return 可以连接返回 true
    bool CanConnect(const PinRef& from, const PinRef& to, wxString* errorMsg = nullptr) const;

    /// @param wire 目标连线
    /// @return 该连线在世界坐标下的完整折线路径；端点失效时返回空列表
    std::vector<wxPoint> GetWirePath(const Wire& wire) const;

    /// 把引脚占用索引回填到引脚列表上（供渲染与命中测试使用）。
    /// @param componentId 引脚所属元件 id
    /// @param[in,out] pins 待回填的引脚列表
    void PopulatePinStates(int componentId, std::vector<Pin>& pins) const;

    // ── 命中与选中 ──
    /// @param worldPt 世界坐标点
    /// @return 命中的最上层元件；未命中返回 nullptr
    Component* HitTestComponent(const wxPoint& worldPt) const;

    /// 命中测试引脚端点。
    /// @param worldPt   世界坐标点
    /// @param tolerance 命中容差（世界单位）
    /// @return 命中的引脚引用；未命中返回无效 PinRef
    PinRef HitTestPin(const wxPoint& worldPt, int tolerance = 6) const;

    /// 清空选中集合。
    void ClearSelection();

    /// 只选中指定元件（先清空其它）。
    /// @param componentId 元件 id
    void SelectOnly(int componentId);

    /// 切换指定元件的选中态。
    /// @param componentId 元件 id
    void ToggleSelect(int componentId);

    /// 选中全部元件。
    void SelectAll();

    /// @return 当前选中元件的 id 集合
    const std::vector<int>& GetSelection() const { return m_selection; }

    /// @param componentId 元件 id
    /// @return 该元件是否处于选中态
    bool IsSelected(int componentId) const;

    /// @return 主选中元件 id（选中集合的首个元素）；无选中返回 -1
    int GetPrimarySelection() const;

    /// 命中测试连线。
    /// @param worldPt 世界坐标点
    /// @param tolerance 命中容差（世界单位）
    /// @return 命中的连线 id；未命中返回 -1
    int HitTestWire(const wxPoint& worldPt, int tolerance = 6) const;

    /// @param wireId 连线 id
    /// @return 该连线是否处于选中态
    bool IsWireSelected(int wireId) const;

    /// 只选中指定连线（先清空所有选中）。
    /// @param wireId 连线 id
    void SelectOnlyWire(int wireId);

    /// 切换指定连线的选中态。
    /// @param wireId 连线 id
    void ToggleWireSelect(int wireId);

    /// @return 当前选中连线的 id 集合
    const std::vector<int>& GetWireSelection() const { return m_selectedWires; }

    // ── 属性修改（统一入口，保证 NotifyChanged 被调用）──
    /// @param componentId 元件 id
    /// @param worldPos    新的世界坐标位置
    void SetComponentPosition(int componentId, const wxPoint& worldPos);

    /// 只改坐标、不触发变更通知。供画布拖动过程中高频调用，
    /// 避免每一帧都刷新属性面板造成抖动。
    /// @param componentId 元件 id
    /// @param worldPos    新的世界坐标位置
    void SetComponentPositionSilent(int componentId, const wxPoint& worldPos);

    /// @param componentId 元件 id
    /// @param name        新的显示名称
    void SetComponentName(int componentId, const wxString& name);

    /// @param componentId 元件 id
    /// @param width       新的位宽（内部钳制到 1~32）
    void SetComponentBitWidth(int componentId, int width);

    /// @param componentId 元件 id
    /// @param degrees     新的朝向（归一化到 90 度整数倍）
    void SetComponentOrientation(int componentId, int degrees);

    /// 设置元件参考编号（触发结构变更通知，保证撤销快照与界面刷新）。
    /// @param componentId 元件 id
    /// @param refDes      新的参考编号；空字符串表示由导出层自动分配
    void SetComponentRefDes(int componentId, const wxString& refDes);

    /// 设置元件封装名（触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param footprint   新的封装名；空字符串表示按类型映射
    void SetComponentFootprint(int componentId, const wxString& footprint);

    /// 设置元件器件型号（触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param value       新的器件型号；空字符串表示按类型默认
    void SetComponentValue(int componentId, const wxString& value);

    /// 设置元件门序号（触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param slot        门序号；小于 1 时按 1 处理
    void SetComponentSlot(int componentId, int slot);

    /// 切换输入引脚的高/低电平（供画布单击交互调用）。
    /// @param componentId 元件 id
    /// @return 发生了切换返回 true；非 InputPin 元件或不存在时返回 false
    bool ToggleInputLevel(int componentId);

    /// 若世界坐标落在某个带电平元件的切换圆圈内，则翻转其电平并触发结构变更。
    /// 输入引脚与电平源共用这一入口，画布只需调一次即可，不必逐个判断类型。
    /// @param worldPt 世界坐标点
    /// @return 发生了切换返回 true；未命中圆圈、或元件不支持切换时返回 false
    bool ToggleLevelByCircle(const wxPoint& worldPt);

    /// 设置输入引脚电平（触发结构变更通知，因此可撤销）。
    /// @param componentId 元件 id
    /// @param level       新电平
    void SetComponentInputLevel(int componentId, LogicLevel level);

    /// @param componentId 元件 id
    /// @return 指定输入引脚的电平；非 InputPin 或无此元件时返回 Unknown
    LogicLevel GetComponentInputLevel(int componentId) const;

    /// 设置时钟源频率（触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param hz          频率（Hz）；<= 0 时忽略
    void SetComponentClockFrequency(int componentId, int hz);

    /// 设置时钟源占空比（触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param duty        占空比；越界时忽略
    void SetComponentClockDuty(int componentId, double duty);

    /// @param componentId 元件 id
    /// @return 时钟源频率（Hz）；非 ClockSource 或无此元件时返回 0
    int GetComponentClockFrequency(int componentId) const;

    /// @param componentId 元件 id
    /// @return 时钟源占空比；非 ClockSource 或无此元件时返回 0.0
    double GetComponentClockDuty(int componentId) const;

    /// 设置文本标注内容（写入元件 name，触发结构变更通知）。
    /// @param componentId 元件 id
    /// @param text        新文本
    void SetComponentText(int componentId, const wxString& text);

    /// 设置电平源元件的输出电平（触发结构变更通知，因此可撤销）。
    /// @param componentId 元件 id
    /// @param level       目标电平，仅 High / Low
    void SetComponentSourceLevel(int componentId, LogicLevel level);

    /// @param componentId 元件 id
    /// @return 该元件的输出电平；非电平源或无此元件时返回 Unknown
    LogicLevel GetComponentSourceLevel(int componentId) const;

    /// 把全部选中元件顺时针旋转 90 度。
    void RotateSelected();

    /// 删除全部选中元件（连带它们的连线）。
    void DeleteSelected();

    // ── 画布注释（不是元件，与元件/连线隔离）──
    /// 在指定世界坐标插入空注释。
    /// @param worldPos 注释左上角世界坐标
    /// @param text     初始文本，默认空
    /// @return 新注释 id
    int AddNote(const wxPoint& worldPos, const wxString& text = wxString());

    /// 删除指定注释。
    /// @param id 注释 id
    /// @return 命中并删除返回 true
    bool RemoveNote(int id);

    /// 修改注释文本并刷新包围盒，触发结构变更通知（可撤销）。
    /// @param id   注释 id
    /// @param text 新文本（可为多行）
    void SetNoteText(int id, const wxString& text);

    /// 拖动过程中静默移动注释，不触发通知（由 EndDrag 统一提交一次）。
    /// @param id       注释 id
    /// @param worldPos 新左上角世界坐标
    void SetNotePositionSilent(int id, const wxPoint& worldPos);

    /// @return 全部注释（只读）
    const std::vector<Note>& GetNotes() const { return m_notes; }

    /// @param id 注释 id
    /// @return 找到返回指针，否则 nullptr
    Note* FindNote(int id);

    /// @param id 注释 id
    /// @return 找到返回指针，否则 nullptr
    const Note* FindNote(int id) const;

    /// 按包围盒逆序命中注释（后插入的在上层）。
    /// @param worldPt 世界坐标点
    /// @return 命中的注释，未命中返回 nullptr
    Note* HitTestNote(const wxPoint& worldPt) const;

    /// 仅选中该注释，并清空元件/连线的选中态。
    /// @param id 注释 id
    void SelectOnlyNote(int id);

    /// @param id 注释 id
    /// @return 该注释是否处于选中态
    bool IsNoteSelected(int id) const { return m_selectedNoteId == id; }

    /// @return 当前选中的注释 id，未选中返回 -1
    int GetSelectedNoteId() const { return m_selectedNoteId; }

    /// 测量注释正文在 zoom=1 下的尺寸，作为世界坐标包围盒。
    /// @param text 注释正文
    /// @return 含内边距的包围盒尺寸（世界单位）
    static wxSize MeasureNoteSize(const wxString& text);

    // ── 物理芯片分组（gEDA 网表 / pcb 布局导出用）──
    /// 一片物理芯片的聚合信息。
    struct ChipGroup
    {
        wxString refDes;    ///< 参考编号（显式填写值或自动分配结果）
        wxString value;     ///< 器件型号；组内首个非空值，否则留空由导出层按类型默认
        wxString footprint; ///< 封装名；组内首个非空值，否则留空由导出层按类型映射
        std::vector<std::pair<int, int>> gates; ///< 组内各门：(元件 id, 门序号 slot)
        bool autoAssigned = false; ///< true 表示该 refDes 是本次查询自动生成的
    };

    /// 按物理芯片聚合所有可封装元件。
    /// 规则：显式填了 refDes 的按 refDes 聚合；未填的按「同类型每 4 门一片」自动分配。
    /// 本方法为 const 纯查询，不修改任何元件字段，避免查询本身触发变更通知。
    /// @return 按 refDes 字典序排序的芯片组列表
    std::vector<ChipGroup> GetChipGroups() const;

    /// 校验芯片分组是否存在 slot 冲突（同一 refDes 下 slot 重复）。
    /// @param message 冲突描述；可为 nullptr
    /// @return true 表示存在冲突
    bool HasSlotConflict(wxString* message) const;

    // ── 变更通知 ──
    /// 画面刷新处理器：任何需要重绘的变化都会调用（含选中变化）。
    /// @param handler 新的画面刷新回调；传空的 std::function 可解除绑定
    void SetChangeHandler(ChangeHandler handler) { m_changeHandler = std::move(handler); }

    /// 结构变更处理器：文档内容（元件/连线/属性）真正发生变化时才会调用，
    /// 单纯的选中变化不触发。撤销栈依赖它区分「需要记录」的操作。
    using StructureChangeHandler = std::function<void()>;

    /// 注册结构变更处理器（与 SetChangeHandler 并存，语义不同）。
    /// @param handler 结构变更回调；传空则解除绑定
    void SetStructureChangeHandler(StructureChangeHandler handler)
    {
        m_structureChangeHandler = std::move(handler);
    }

    /// 通知抑制的 RAII 守卫：构造时进入抑制，析构时退出，异常安全。
    /// 用于反序列化回放与整文件加载，避免回放过程反向触发各类回调。
    class SuppressScope
    {
    public:
        /// 进入抑制状态。
        /// @param circuit 目标电路；本守卫存活期间该对象必须存活
        explicit SuppressScope(Circuit& circuit)
            : m_circuit(circuit)
        {
            m_circuit.BeginSuppressNotifications();
        }

        /// 退出抑制状态。
        ~SuppressScope() { m_circuit.EndSuppressNotifications(); }

        SuppressScope(const SuppressScope&) = delete;
        SuppressScope& operator=(const SuppressScope&) = delete;

    private:
        Circuit& m_circuit; ///< 被抑制的电路
    };

    /// 进入通知抑制（支持嵌套，必须与 EndSuppressNotifications 成对调用）。
    void BeginSuppressNotifications();

    /// 退出通知抑制。
    void EndSuppressNotifications();

    /// @return 当前是否处于通知抑制中
    bool IsNotificationSuppressed() const { return m_notifySuppressDepth > 0; }

    /// 主动触发画面刷新回调（受抑制开关控制）。
    void NotifyChanged() const;

    /// 主动触发结构变更回调（受抑制开关控制）。
    /// 供「一次操作包含多步但对外只算一次结构变更」的场景显式调用，
    /// 例如多击式走线落线后由画布统一提交。
    void NotifyStructureChanged() const;

private:
    /// @param type 元件类型
    /// @param id   元件 id
    /// @return 形如「与门 3」的默认名称
    wxString MakeDefaultName(ComponentType type, int id) const;

    /// 从选中集合中移除指定元件。
    /// @param componentId 元件 id
    void EraseFromSelection(int componentId);

    /// 删除挂接在指定元件上的全部连线（不触发变更通知）。
    /// @param componentId 元件 id
    void RemoveWiresForComponent(int componentId);

    /// 按 id 删除连线（不触发变更通知）。
    /// @param wireId 连线 id
    /// @return 删除成功返回 true
    bool RemoveWireInternal(int wireId);

    std::vector<std::unique_ptr<Component>> m_components; ///< 元件容器（独占所有权）
    std::vector<std::unique_ptr<Wire>> m_wires;           ///< 连线容器（独占所有权）
    std::vector<int> m_selection;                         ///< 选中元件 id 集合
    std::vector<int> m_selectedWires;                     ///< 选中连线 id 集合
    std::vector<Note> m_notes;                            ///< 全部注释，绘制在元件与连线之上
    int m_nextNoteId = 1;                                 ///< 注释 id 计数器，按既有约定不因 Clear() 重置
    int m_selectedNoteId = -1;                            ///< 当前选中的注释 id，-1 表示无
    std::map<std::pair<int, int>, int> m_pinToWire;       ///< (元件 id, 引脚序号) → 连线 id
    int m_nextComponentId = 1;                            ///< 下一个可用元件 id
    int m_nextWireId = 1;                                 ///< 下一个可用连线 id
    ChangeHandler m_changeHandler;                        ///< 变更回调
    StructureChangeHandler m_structureChangeHandler;      ///< 结构变更回调
    int m_notifySuppressDepth = 0;                        ///< 通知抑制嵌套深度，>0 时不派发任何通知
};

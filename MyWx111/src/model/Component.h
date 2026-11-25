#pragma once

#include <memory>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/string.h>

#include "ComponentType.h"
#include "LogicLevel.h"
#include "Pin.h"
#include "Wire.h"

/// 符号的一段圆弧（用于门电路元件的弧形轮廓）。
struct SymbolArc
{
    wxPoint center;          ///< 圆心（元件本地坐标）
    int radius = 0;          ///< 半径
    double startAngle = 0.0; ///< 起始角（度；屏幕坐标系下 0 度指向正右，顺时针为正）
    double endAngle = 0.0;   ///< 结束角（度）
};

/// 符号中的一条折线。
struct SymbolPolyline
{
    std::vector<wxPoint> points; ///< 折线顶点（元件本地坐标）
    bool closed = false;         ///< 是否闭合
};

/// 元件符号的几何描述。
/// 模型层只产出几何数据，真正的抗锯齿绘制统一交给视图层的 ComponentRenderer。
/// 这样模型层可以完全脱离绘制 API，既便于单元测试，也便于日后替换渲染后端。
struct SymbolGeometry
{
    std::vector<wxPoint> body;             ///< 主体多边形顶点（顶点数 >= 3 时填充绘制）
    std::vector<SymbolPolyline> polylines; ///< 附加折线（如异或门左侧的背弧）
    std::vector<SymbolArc> arcs;           ///< 附加圆弧
    wxPoint inversionCenter;               ///< 反相小圆圆心（本地坐标）
    int inversionRadius = 0;               ///< 反相小圆半径；0 表示不绘制
    wxPoint labelAnchor;                   ///< 文字锚点（本地坐标）；目前仅 TextLabel 的文本绘制使用
};

/// 电路元件抽象基类。
/// 子类只需回答三件事：引脚布局、本地包围盒、符号几何，完全不涉及绘制。
class Component
{
public:
    /// @param type 元件类型
    /// @param id   元件在 Circuit 内的唯一 id
    Component(ComponentType type, int id);

    /// 虚析构：保证通过基类指针删除子类对象时正确析构。
    virtual ~Component() = default;

    // ── 子类必须实现的三个钩子 ──
    /// 按当前位宽计算引脚布局（未应用朝向变换的原始布局）。
    /// @return 引脚列表，容器顺序即为引脚 index 顺序
    virtual std::vector<Pin> BuildPins() const = 0;

    /// @return 元件在本地坐标系（未平移、未旋转）下的包围盒；各实现的宽高相等
    virtual wxRect GetLocalBounds() const = 0;

    /// 填充元件符号的几何描述。
    /// @param[out] out 由子类按自身形状填写
    virtual void DescribeSymbol(SymbolGeometry& out) const = 0;

    /// 组合逻辑求值。
    /// 默认实现返回 Unknown：文本标注、时钟源、触发器、LED/探针等「不参与传播」的元件
    /// 无需逐个显式重写即可自动失效，避免在仿真器里维护一份类型 switch。
    /// @param inputs 按引脚 index 升序排列的输入引脚电平（仅含 Input/Bidi 引脚）
    /// @return 该元件输出的电平；多输出元件本次不参与仿真，统一返回 Unknown
    virtual LogicLevel Evaluate(const std::vector<LogicLevel>& inputs) const
    {
        (void)inputs;
        return LogicLevel::Unknown;
    }

    /// @return 该元件是否需要在仿真中被求值；无输出元件（如文本标注）返回 false
    virtual bool ParticipatesInSimulation() const { return true; }

    /// @return 该元件是否提供「点击切换电平」的圆圈热区。
    /// 只有带电平的元件（输入引脚、电平源）会返回 true，
    /// 其余元件用默认 false 即可，无需逐个重写。
    virtual bool HasLevelToggle() const { return false; }

    /// @return 圆圈热区中心（本地未变换坐标）；仅 HasLevelToggle() 为 true 时有意义
    virtual wxPoint GetLevelToggleCenter() const { return wxPoint(0, 0); }

    // ── 标识 ──
    /// @return 元件唯一 id
    int GetId() const { return m_id; }

    /// @return 元件类型
    ComponentType GetType() const { return m_type; }

    // ── 名称 ──
    /// @return 元件显示名称
    const wxString& GetName() const { return m_name; }

    /// @param name 新的显示名称
    void SetName(const wxString& name) { m_name = name; }

    // ── 位置 ──
    /// @return 元件世界坐标原点（本地包围盒左上角对应位置）
    const wxPoint& GetPosition() const { return m_position; }

    /// @param pos 新的世界坐标位置（调用方负责吸附网格）
    void SetPosition(const wxPoint& pos) { m_position = pos; }

    // ── 朝向 ──
    /// @return 朝向角度，取值 0 / 90 / 180 / 270
    int GetOrientation() const { return m_orientation; }

    /// @param degrees 朝向角度，内部归一化到 90 度的整数倍
    void SetOrientation(int degrees);

    /// 顺时针旋转 90 度。
    void Rotate();

    // ── 位宽 ──
    /// @return 位宽
    int GetBitWidth() const { return m_bitWidth; }

    /// @param width 新的位宽，内部钳制到 [1, 32]
    void SetBitWidth(int width);

    // ── 物理芯片属性（导出 gEDA 网表与 pcb 布局用）──
    /// @return 参考编号（如 "U1"）；空字符串表示未指定，由导出层按类型自动分配
    const wxString& GetRefDes() const { return m_refDes; }

    /// 设置参考编号。空字符串恢复为「自动分配」。
    /// 注意：同一物理芯片的多个门必须填相同 refDes，用 slot 区分门，
    /// 否则 PCB 会把它当成多片独立芯片。
    /// @param refDes 新的参考编号
    void SetRefDes(const wxString& refDes) { m_refDes = refDes; }

    /// @return 封装名（如 "DIP14"）；空字符串表示按元件类型走内置映射表
    const wxString& GetFootprint() const { return m_footprint; }

    /// 设置封装名。空字符串恢复为按类型自动映射。
    /// @param footprint 封装名
    void SetFootprint(const wxString& footprint) { m_footprint = footprint; }

    /// @return 器件型号（如 "7486"）；空字符串表示按类型取默认型号
    const wxString& GetValue() const { return m_value; }

    /// 设置器件型号。
    /// @param value 器件型号
    void SetValue(const wxString& value) { m_value = value; }

    /// @return 门序号：同一 refDes 下表示这是芯片中的第几个门，从 1 起
    int GetSlot() const { return m_slot; }

    /// 设置门序号。
    /// @param slot 门序号；小于 1 时按 1 处理
    void SetSlot(int slot) { m_slot = (slot < 1) ? 1 : slot; }

    // ── 几何计算（非虚工具方法）──
    /// 应用朝向变换后的引脚列表，坐标仍为相对偏移。
    /// @return 已按朝向旋转的引脚列表
    std::vector<Pin> GetTransformedPins() const;

    /// @param pinIndex 引脚序号
    /// @return 指定引脚端点的世界坐标；序号越界时返回元件原点
    wxPoint GetPinWorldPos(int pinIndex) const;

    /// 取引脚相对元件的几何朝向，供导线正交走线决定出线方向。
    /// 与 Pin::direction（电气方向）无关，这里只看引脚落在本体的哪一侧。
    /// @param pinIndex 引脚序号
    /// @return 已应用朝向变换后的几何朝向；序号越界时返回 PinSide::Right
    PinSide GetPinSide(int pinIndex) const;

    /// 命中测试。
    /// @param worldPt 世界坐标点
    /// @return 点落在包围盒（含引脚外扩 4 像素）内返回 true
    bool HitTest(const wxPoint& worldPt) const;

    /// @return 元件在世界坐标下的包围盒
    wxRect GetWorldBounds() const;

    /// @return 全部引脚端点的世界坐标列表
    std::vector<wxPoint> GetPinWorldPositions() const;

    /// 属性面板用：返回第 i 个可编辑属性的标签名。
    /// @param i 属性序号
    /// @return 标签名；序号越界返回空串
    virtual wxString GetPropertyLabel(int i) const;

    /// @return 可编辑属性条目总数（名称 / 朝向 / 位宽）
    virtual int GetPropertyCount() const { return 3; }

protected:
    int m_id;               ///< 元件唯一 id
    ComponentType m_type;   ///< 元件类型
    wxString m_name;        ///< 显示名称，如「与门 1」
    wxPoint m_position;     ///< 世界坐标原点
    int m_orientation = 0;  ///< 朝向：0 / 90 / 180 / 270
    int m_bitWidth = 1;     ///< 位宽 1~32（当前绘制忽略，仅参与数据与属性面板）
    wxString m_refDes;      ///< 参考编号；空表示由导出层自动分配
    wxString m_footprint;   ///< 封装名覆盖值；空表示按类型映射
    wxString m_value;       ///< 器件型号覆盖值；空表示按类型默认
    int m_slot = 1;         ///< 门序号（1 起），同一 refDes 下需唯一
};

/// 元件工厂：集中按类型创建具体元件，避免各调用点散落 switch。
/// 实现位于 GateComponents.cpp，因为工厂需要依赖全部具体元件类型的定义。
class ComponentFactory
{
public:
    /// @param type 元件类型
    /// @param id   新元件的 id
    /// @return 新建元件对象；未知类型返回 nullptr
    static std::unique_ptr<Component> Create(ComponentType type, int id);
};

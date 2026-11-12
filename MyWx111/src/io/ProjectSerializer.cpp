#include "ProjectSerializer.h"

#include "../model/Circuit.h"
#include "../model/Component.h"
#include "../model/ComponentType.h"
#include "../model/GateComponents.h"

#include <wx/txtstrm.h>
#include <wx/wfstream.h>

#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace
{
    /// 当前工程文件版本。
    /// 注意：元件类型以 ComponentType 的整数值写入文件，
    /// 因此一旦枚举顺序发生变化，必须提升该版本号，否则旧文件会把类型读错位。
    /// v2：新增 refdes / footprint / value / slot 四个物理芯片属性。
    /// v3：新增输入引脚电平、时钟频率与占空比（按类型写出）。
    constexpr int kCurrentVersion = 3;

    /// 文件格式标识，用于区分本工程的 JSON 与任意其它 JSON 文件。
    const wxString kFormatTag = wxString(L"MyWx111-Circuit");

    /// 解析过程中暂存的元件数据。
    /// 先解析到临时结构、全部校验通过后再写入 Circuit，这样加载失败时原电路保持完好。
    struct TempComponent
    {
        int id = -1;              ///< 文件中的元件 id，用于建立到新 id 的映射
        int type = 0;             ///< ComponentType 的整数值
        wxString name;            ///< 元件显示名称（可含中文）
        int x = 0;                ///< 世界坐标 X
        int y = 0;                ///< 世界坐标 Y
        int orientation = 0;      ///< 朝向角度
        int bitWidth = 1;         ///< 位宽
        wxString refDes;          ///< 参考编号；空表示由导出层自动分配
        wxString footprint;       ///< 封装名覆盖值；空表示按类型映射
        wxString value;           ///< 器件型号覆盖值；空表示按类型默认
        int slot = 1;             ///< 门序号（1 起），同一 refDes 下需唯一
        int inputLevel = 0;       ///< 输入引脚电平：0=低 1=高 2=未知（v3）
        int clockFreqHz = 1;      ///< 时钟频率（Hz，v3）
        int clockDuty = 50;       ///< 时钟占空比，百分比整数以避免浮点解析差异（v3）
    };

    /// 解析过程中暂存的连线数据。
    struct TempWire
    {
        int fromComponent = -1;        ///< 起点元件在文件中的 id
        int fromPin = -1;              ///< 起点引脚序号
        int toComponent = -1;          ///< 终点元件在文件中的 id
        int toPin = -1;                ///< 终点引脚序号
        std::vector<wxPoint> bends;    ///< 手动折点（世界坐标）
    };

    /// 最小 JSON 扫描器。
    /// 只服务于本模块自产的文件格式，不追求通用性：
    /// 遇到未知键时通过 SkipValue 跳过，从而在字段增减时保持前向兼容。
    class JsonReader
    {
    public:
        /// @param text 待解析的完整文本
        explicit JsonReader(const wxString& text)
            : m_text(text)
        {
        }

        /// 跳过空白字符
        void SkipWhitespace()
        {
            while (m_pos < m_text.length())
            {
                const wxUniChar ch = m_text[m_pos];
                if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n')
                {
                    ++m_pos;
                    continue;
                }
                break;
            }
        }

        /// @return 下一个非空白字符；已到末尾返回 '\0'
        wxUniChar Peek()
        {
            SkipWhitespace();
            return (m_pos < m_text.length()) ? m_text[m_pos] : wxUniChar(L'\0');
        }

        /// @param expected 期望匹配的字符
        /// @return 匹配并消耗成功返回 true
        bool Consume(wxUniChar expected)
        {
            if (Peek() != expected)
            {
                return false;
            }
            ++m_pos;
            return true;
        }

        /// 读取双引号字符串并处理常见转义。
        /// @param[out] out 读取结果
        /// @return 成功返回 true
        bool ReadString(wxString& out)
        {
            if (!Consume(L'"'))
            {
                return false;
            }

            out.clear();
            while (m_pos < m_text.length())
            {
                const wxUniChar ch = m_text[m_pos++];
                if (ch == L'"')
                {
                    return true;
                }
                if (ch != L'\\')
                {
                    out += ch;
                    continue;
                }

                if (m_pos >= m_text.length())
                {
                    return false;
                }
                const wxUniChar esc = m_text[m_pos++];
                switch (esc.GetValue())
                {
                case L'"':
                    out += L'"';
                    break;
                case L'\\':
                    out += L'\\';
                    break;
                case L'/':
                    out += L'/';
                    break;
                case L'n':
                    out += L'\n';
                    break;
                case L'r':
                    out += L'\r';
                    break;
                case L't':
                    out += L'\t';
                    break;
                case L'b':
                    out += L'\b';
                    break;
                case L'f':
                    out += L'\f';
                    break;
                case L'u':
                {
                    // \uXXXX：本模块写文件时不会产生这种转义，这里仅为容错
                    if (m_pos + 4 > m_text.length())
                    {
                        return false;
                    }
                    unsigned long code = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        const wxUniChar digit = m_text[m_pos++];
                        code <<= 4;
                        if (digit >= L'0' && digit <= L'9')
                        {
                            code += static_cast<unsigned long>(digit.GetValue() - L'0');
                        }
                        else if (digit >= L'a' && digit <= L'f')
                        {
                            code += static_cast<unsigned long>(digit.GetValue() - L'a') + 10UL;
                        }
                        else if (digit >= L'A' && digit <= L'F')
                        {
                            code += static_cast<unsigned long>(digit.GetValue() - L'A') + 10UL;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    out += wxUniChar(static_cast<wxUint32>(code));
                    break;
                }
                default:
                    return false;
                }
            }
            return false;
        }

        /// 读取整数（允许前导负号）。
        /// @param[out] out 读取结果
        /// @return 成功返回 true
        bool ReadInt(int& out)
        {
            SkipWhitespace();
            const size_t start = m_pos;
            if (m_pos < m_text.length() && (m_text[m_pos] == L'-' || m_text[m_pos] == L'+'))
            {
                ++m_pos;
            }
            while (m_pos < m_text.length() && m_text[m_pos] >= L'0' && m_text[m_pos] <= L'9')
            {
                ++m_pos;
            }
            if (m_pos == start)
            {
                return false;
            }

            long value = 0;
            if (!m_text.Mid(start, m_pos - start).ToLong(&value))
            {
                return false;
            }
            out = static_cast<int>(value);
            return true;
        }

        /// 跳过一个任意 JSON 值，用于未知键的前向兼容。
        /// @return 成功跳过返回 true
        bool SkipValue()
        {
            const wxUniChar ch = Peek();
            if (ch == L'"')
            {
                wxString dummy;
                return ReadString(dummy);
            }

            if (ch == L'{' || ch == L'[')
            {
                const wxUniChar close = (ch == L'{') ? L'}' : L']';
                if (!Consume(ch))
                {
                    return false;
                }

                while (true)
                {
                    const wxUniChar next = Peek();
                    if (next == close)
                    {
                        ++m_pos;
                        return true;
                    }
                    if (next == L'\0')
                    {
                        return false;
                    }

                    // 对象成员还需跳过 "键" : 这两段，数组元素直接跳值
                    if (ch == L'{' && next == L'"')
                    {
                        wxString key;
                        if (!ReadString(key))
                        {
                            return false;
                        }
                        if (!Consume(L':'))
                        {
                            return false;
                        }
                    }

                    if (!SkipValue())
                    {
                        return false;
                    }
                    if (Peek() == L',')
                    {
                        ++m_pos;
                    }
                }
            }

            // 数字、true / false / null：消耗连续的非分隔字符
            const size_t start = m_pos;
            while (m_pos < m_text.length())
            {
                const wxUniChar cur = m_text[m_pos];
                if (cur == L',' || cur == L'}' || cur == L']' || cur == L' ' || cur == L'\t' ||
                    cur == L'\r' || cur == L'\n')
                {
                    break;
                }
                ++m_pos;
            }
            return m_pos > start;
        }

    private:
        wxString m_text;  ///< 待解析文本
        size_t m_pos = 0; ///< 当前游标位置
    };

    /// 把文本转义为可放进 JSON 双引号内的内容（不含两侧引号）。
    /// 只处理双引号、反斜杠与三个常见控制字符：元件名称不含其它控制字符，
    /// 因此不必生成 \u00XX 形式，保持文件可读。
    /// @param text 原始文本
    /// @return 转义后的文本
    wxString EscapeJsonText(const wxString& text)
    {
        wxString result;
        result.reserve(text.length() + 8);

        for (const wxUniChar ch : text)
        {
            switch (ch.GetValue())
            {
            case L'"':
                result += L"\\\"";
                break;
            case L'\\':
                result += L"\\\\";
                break;
            case L'\n':
                result += L"\\n";
                break;
            case L'\r':
                result += L"\\r";
                break;
            case L'\t':
                result += L"\\t";
                break;
            default:
                result += ch;
                break;
            }
        }
        return result;
    }

    /// 解析折点数组。
    /// @param reader JSON 扫描器
    /// @param[out] bends 解析出的折点
    /// @param[out] error 失败时的中文错误描述
    /// @return 成功返回 true
    bool ParseBends(JsonReader& reader, std::vector<wxPoint>& bends, wxString& error)
    {
        if (!reader.Consume(L'['))
        {
            error = wxString(L"工程文件格式错误：bends 不是数组");
            return false;
        }

        while (true)
        {
            const wxUniChar next = reader.Peek();
            if (next == L']')
            {
                reader.Consume(L']');
                return true;
            }
            if (next == L'\0')
            {
                error = wxString(L"工程文件格式错误：bends 数组未闭合");
                return false;
            }
            if (!reader.Consume(L'{'))
            {
                error = wxString(L"工程文件格式错误：bends 元素不是对象");
                return false;
            }

            wxPoint point;
            while (true)
            {
                const wxUniChar ch = reader.Peek();
                if (ch == L'}')
                {
                    reader.Consume(L'}');
                    break;
                }
                if (ch == L'\0')
                {
                    error = wxString(L"工程文件格式错误：bends 元素未闭合");
                    return false;
                }

                wxString key;
                if (!reader.ReadString(key) || !reader.Consume(L':'))
                {
                    error = wxString(L"工程文件格式错误：bends 键无效");
                    return false;
                }

                bool ok = true;
                if (key == L"x")
                {
                    ok = reader.ReadInt(point.x);
                }
                else if (key == L"y")
                {
                    ok = reader.ReadInt(point.y);
                }
                else
                {
                    ok = reader.SkipValue();
                }

                if (!ok)
                {
                    error = wxString(L"工程文件格式错误：bends 坐标读取失败");
                    return false;
                }
                if (reader.Peek() == L',')
                {
                    reader.Consume(L',');
                }
            }

            bends.push_back(point);
            if (reader.Peek() == L',')
            {
                reader.Consume(L',');
            }
        }
    }

    /// 解析 components 数组。
    /// @param reader JSON 扫描器
    /// @param[out] comps 解析出的元件列表
    /// @param[out] error 失败时的中文错误描述
    /// @return 成功返回 true
    bool ParseComponents(JsonReader& reader, std::vector<TempComponent>& comps, wxString& error)
    {
        if (!reader.Consume(L'['))
        {
            error = wxString(L"工程文件格式错误：components 不是数组");
            return false;
        }

        while (true)
        {
            const wxUniChar next = reader.Peek();
            if (next == L']')
            {
                reader.Consume(L']');
                return true;
            }
            if (next == L'\0')
            {
                error = wxString(L"工程文件格式错误：components 数组未闭合");
                return false;
            }
            if (!reader.Consume(L'{'))
            {
                error = wxString(L"工程文件格式错误：components 元素不是对象");
                return false;
            }

            TempComponent item;
            while (true)
            {
                const wxUniChar ch = reader.Peek();
                if (ch == L'}')
                {
                    reader.Consume(L'}');
                    break;
                }
                if (ch == L'\0')
                {
                    error = wxString(L"工程文件格式错误：components 元素未闭合");
                    return false;
                }

                wxString key;
                if (!reader.ReadString(key) || !reader.Consume(L':'))
                {
                    error = wxString(L"工程文件格式错误：components 键无效");
                    return false;
                }

                bool ok = true;
                if (key == L"id")
                {
                    ok = reader.ReadInt(item.id);
                }
                else if (key == L"type")
                {
                    ok = reader.ReadInt(item.type);
                }
                else if (key == L"name")
                {
                    ok = reader.ReadString(item.name);
                }
                else if (key == L"x")
                {
                    ok = reader.ReadInt(item.x);
                }
                else if (key == L"y")
                {
                    ok = reader.ReadInt(item.y);
                }
                else if (key == L"orientation")
                {
                    ok = reader.ReadInt(item.orientation);
                }
                else if (key == L"bitWidth")
                {
                    ok = reader.ReadInt(item.bitWidth);
                }
                else if (key == L"refdes")
                {
                    ok = reader.ReadString(item.refDes);
                }
                else if (key == L"footprint")
                {
                    ok = reader.ReadString(item.footprint);
                }
                else if (key == L"value")
                {
                    ok = reader.ReadString(item.value);
                }
                else if (key == L"slot")
                {
                    ok = reader.ReadInt(item.slot);
                }
                else if (key == L"inputLevel")
                {
                    ok = reader.ReadInt(item.inputLevel);
                }
                else if (key == L"clockFreqHz")
                {
                    ok = reader.ReadInt(item.clockFreqHz);
                }
                else if (key == L"clockDuty")
                {
                    ok = reader.ReadInt(item.clockDuty);
                }
                else
                {
                    ok = reader.SkipValue();
                }

                if (!ok)
                {
                    error = wxString(L"工程文件格式错误：components 字段读取失败");
                    return false;
                }
                if (reader.Peek() == L',')
                {
                    reader.Consume(L',');
                }
            }

            comps.push_back(item);
            if (reader.Peek() == L',')
            {
                reader.Consume(L',');
            }
        }
    }

    /// 解析 wires 数组。
    /// @param reader JSON 扫描器
    /// @param[out] wires 解析出的连线列表
    /// @param[out] error 失败时的中文错误描述
    /// @return 成功返回 true
    bool ParseWires(JsonReader& reader, std::vector<TempWire>& wires, wxString& error)
    {
        if (!reader.Consume(L'['))
        {
            error = wxString(L"工程文件格式错误：wires 不是数组");
            return false;
        }

        while (true)
        {
            const wxUniChar next = reader.Peek();
            if (next == L']')
            {
                reader.Consume(L']');
                return true;
            }
            if (next == L'\0')
            {
                error = wxString(L"工程文件格式错误：wires 数组未闭合");
                return false;
            }
            if (!reader.Consume(L'{'))
            {
                error = wxString(L"工程文件格式错误：wires 元素不是对象");
                return false;
            }

            TempWire item;
            while (true)
            {
                const wxUniChar ch = reader.Peek();
                if (ch == L'}')
                {
                    reader.Consume(L'}');
                    break;
                }
                if (ch == L'\0')
                {
                    error = wxString(L"工程文件格式错误：wires 元素未闭合");
                    return false;
                }

                wxString key;
                if (!reader.ReadString(key) || !reader.Consume(L':'))
                {
                    error = wxString(L"工程文件格式错误：wires 键无效");
                    return false;
                }

                bool ok = true;
                if (key == L"fromComponent")
                {
                    ok = reader.ReadInt(item.fromComponent);
                }
                else if (key == L"fromPin")
                {
                    ok = reader.ReadInt(item.fromPin);
                }
                else if (key == L"toComponent")
                {
                    ok = reader.ReadInt(item.toComponent);
                }
                else if (key == L"toPin")
                {
                    ok = reader.ReadInt(item.toPin);
                }
                else if (key == L"bends")
                {
                    ok = ParseBends(reader, item.bends, error);
                }
                else
                {
                    ok = reader.SkipValue();
                }

                if (!ok)
                {
                    if (error.empty())
                    {
                        error = wxString(L"工程文件格式错误：wires 字段读取失败");
                    }
                    return false;
                }
                if (reader.Peek() == L',')
                {
                    reader.Consume(L',');
                }
            }

            wires.push_back(item);
            if (reader.Peek() == L',')
            {
                reader.Consume(L',');
            }
        }
    }

    /// 解析整个工程文档，并校验格式标识与版本。
    /// @param content 文件全文
    /// @param[out] comps 解析出的元件
    /// @param[out] wires 解析出的连线
    /// @param[out] error 失败时的中文错误描述
    /// @return 成功返回 true
    bool ParseDocument(const wxString& content, std::vector<TempComponent>& comps,
                       std::vector<TempWire>& wires, wxString& error)
    {
        JsonReader reader(content);
        if (!reader.Consume(L'{'))
        {
            error = wxString(L"工程文件格式错误：缺少顶层对象");
            return false;
        }

        bool hasFormat = false;
        wxString format;
        int version = 0;

        while (true)
        {
            const wxUniChar next = reader.Peek();
            if (next == L'}')
            {
                reader.Consume(L'}');
                break;
            }
            if (next == L'\0')
            {
                error = wxString(L"工程文件格式错误：顶层对象未闭合");
                return false;
            }

            wxString key;
            if (!reader.ReadString(key) || !reader.Consume(L':'))
            {
                error = wxString(L"工程文件格式错误：顶层键无效");
                return false;
            }

            if (key == L"format")
            {
                if (!reader.ReadString(format))
                {
                    error = wxString(L"工程文件格式错误：format 字段无效");
                    return false;
                }
                hasFormat = true;
            }
            else if (key == L"version")
            {
                if (!reader.ReadInt(version))
                {
                    error = wxString(L"工程文件格式错误：version 字段无效");
                    return false;
                }
            }
            else if (key == L"components")
            {
                if (!ParseComponents(reader, comps, error))
                {
                    return false;
                }
            }
            else if (key == L"wires")
            {
                if (!ParseWires(reader, wires, error))
                {
                    return false;
                }
            }
            else if (!reader.SkipValue())
            {
                error = wxString(L"工程文件格式错误：无法跳过多余字段");
                return false;
            }

            if (reader.Peek() == L',')
            {
                reader.Consume(L',');
            }
        }

        if (!hasFormat || format != kFormatTag)
        {
            error = wxString(L"不是 MyWx111 工程文件（缺少格式标识）");
            return false;
        }
        // 只拒绝无法识别的版本；对未来更高版本采取宽容策略：
        // 记录提示后继续尝试读取（未知键会被 SkipValue 跳过），
        // 避免因小版本前向不兼容而完全打不开工程
        if (version <= 0)
        {
            error = wxString::Format(L"工程文件版本号无效（%d）", version);
            return false;
        }
        if (version > kCurrentVersion)
        {
            error = wxString::Format(L"工程文件版本 %d 高于本程序支持的 %d，"
                                     L"已按兼容模式读取，部分新属性可能被忽略。",
                                     version, kCurrentVersion);
        }
        return true;
    }
}

namespace project
{
bool SerializeCircuitToJson(const Circuit& circuit, wxString& out, wxString* error)
{
    // 只做内存拼装、不触碰磁盘：撤销快照每步都要序列化，
    // 走文件流会产生大量 I/O，而撤销栈本身只需要文本
    wxUnusedVar(error);

    out.clear();
    out.reserve(1024);

    out += L"{\n";
    out += wxString::Format(L"  \"format\": \"%s\",\n", kFormatTag);
    out += wxString::Format(L"  \"version\": %d,\n", kCurrentVersion);

    const auto& comps = circuit.GetComponents();
    out += L"  \"components\": [\n";
    for (size_t i = 0; i < comps.size(); ++i)
    {
        if (!comps[i])
        {
            continue;
        }
        const Component& comp = *comps[i];

        // 按类型写出专属参数：InputPin 存电平，ClockSource 存频率与占空比。
        // 其他类型不产生这些键，避免无关字段把文件撑大；占空比存百分比整数，
        // 避开浮点在 JSON 中的表示差异。
        wxString extra;
        if (comp.GetType() == ComponentType::InputPin)
        {
            const auto* input = static_cast<const InputPin*>(&comp);
            extra = wxString::Format(L", \"inputLevel\": %d", static_cast<int>(input->GetLevel()));
        }
        else if (comp.GetType() == ComponentType::ClockSource)
        {
            const auto* clock = static_cast<const ClockSource*>(&comp);
            extra = wxString::Format(L", \"clockFreqHz\": %d, \"clockDuty\": %d",
                                     clock->GetFrequencyHz(),
                                     static_cast<int>(std::lround(clock->GetDutyCycle() * 100.0)));
        }

        // 四个物理芯片属性即便为空/默认也照常写出：撤销快照依赖固定字段集，
        // 缺键会让「回滚到该快照」的结果随版本漂移
        out += wxString::Format(
            L"    { \"id\": %d, \"type\": %d, \"name\": \"%s\", \"x\": %d, \"y\": %d, "
            L"\"orientation\": %d, \"bitWidth\": %d, \"refdes\": \"%s\", "
            L"\"footprint\": \"%s\", \"value\": \"%s\", \"slot\": %d%s }%s\n",
            comp.GetId(), static_cast<int>(comp.GetType()), EscapeJsonText(comp.GetName()),
            comp.GetPosition().x, comp.GetPosition().y, comp.GetOrientation(), comp.GetBitWidth(),
            EscapeJsonText(comp.GetRefDes()), EscapeJsonText(comp.GetFootprint()),
            EscapeJsonText(comp.GetValue()), comp.GetSlot(), extra,
            (i + 1 == comps.size()) ? L"" : L",");
    }
    out += L"  ],\n";

    const auto& wires = circuit.GetWires();
    out += L"  \"wires\": [\n";
    for (size_t i = 0; i < wires.size(); ++i)
    {
        if (!wires[i])
        {
            continue;
        }
        const Wire& wire = *wires[i];
        out += wxString::Format(
            L"    { \"fromComponent\": %d, \"fromPin\": %d, \"toComponent\": %d, \"toPin\": %d, "
            L"\"bends\": [",
            wire.GetFrom().componentId, wire.GetFrom().pinIndex, wire.GetTo().componentId,
            wire.GetTo().pinIndex);

        const auto& bends = wire.GetBendPoints();
        for (size_t b = 0; b < bends.size(); ++b)
        {
            out += wxString::Format(L"{ \"x\": %d, \"y\": %d }%s", bends[b].x, bends[b].y,
                                    (b + 1 == bends.size()) ? L"" : L", ");
        }

        out += wxString::Format(L"] }%s\n", (i + 1 == wires.size()) ? L"" : L",");
    }
    out += L"  ]\n";
    out += L"}\n";
    return true;
}

bool SaveProjectJson(const Circuit& circuit, const wxString& path, wxString* error)
{
    wxString json;
    if (!SerializeCircuitToJson(circuit, json, error))
    {
        return false;
    }

    wxFileOutputStream stream(path);
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"无法创建或写入文件：") + path;
        }
        return false;
    }

    // 固定 Unix 换行：同一电路在不同平台上保存的文件逐字节一致，便于版本比对
    wxTextOutputStream out(stream, wxEOL_UNIX);
    out << json;

    out.Flush();
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"写入文件失败：") + path;
        }
        return false;
    }

    stream.Sync();
    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"写入文件后同步失败，文件可能不完整：") + path;
        }
        return false;
    }
    return true;
}

bool DeserializeCircuitFromJson(Circuit& circuit, const wxString& json, wxString* error)
{
    std::vector<TempComponent> temps;
    std::vector<TempWire> tempWires;
    wxString parseError;
    if (!ParseDocument(json, temps, tempWires, parseError))
    {
        // 解析未通过，此时完全没碰 circuit，原电路保持完好
        if (error != nullptr)
        {
            *error = parseError;
        }
        return false;
    }

    circuit.Clear();

    // 重建时 AddComponent 会重新分配元件 id，与 JSON 里的 id 不一定相同，
    // 因此必须建立旧 id → 新 id 的映射，否则连线会指向错误的元件；
    // 同一份快照内的连线引用都被同步重映射，故快照内容始终自洽
    std::map<int, int> idMap;
    wxString warnings;

    for (const TempComponent& temp : temps)
    {
        Component* comp = circuit.AddComponent(static_cast<ComponentType>(temp.type),
                                               wxPoint(temp.x, temp.y));
        if (comp == nullptr)
        {
            warnings += wxString::Format(L"元件 %d 类型无效，已跳过\r\n", temp.id);
            continue;
        }

        comp->SetName(temp.name);
        comp->SetOrientation(temp.orientation);
        comp->SetBitWidth(temp.bitWidth);

        // 直接调 Component 的 setter 而非 Circuit::SetComponentXxx：
        // 批量重建阶段不应逐元件触发撤销快照，由调用方在结束后统一打点
        comp->SetRefDes(temp.refDes);
        comp->SetFootprint(temp.footprint);
        comp->SetValue(temp.value);
        comp->SetSlot(temp.slot);

        // 按类型回填专属参数；同样直接调 Component 的 setter，不走 Circuit 包装
        if (static_cast<ComponentType>(temp.type) == ComponentType::InputPin)
        {
            static_cast<InputPin*>(comp)->SetLevel(static_cast<LogicLevel>(temp.inputLevel));
        }
        else if (static_cast<ComponentType>(temp.type) == ComponentType::ClockSource)
        {
            auto* clock = static_cast<ClockSource*>(comp);
            clock->SetFrequencyHz(temp.clockFreqHz);
            clock->SetDutyCycle(static_cast<double>(temp.clockDuty) / 100.0);
        }

        idMap[temp.id] = comp->GetId();
    }

    for (const TempWire& temp : tempWires)
    {
        const auto fromIt = idMap.find(temp.fromComponent);
        const auto toIt = idMap.find(temp.toComponent);
        if (fromIt == idMap.end() || toIt == idMap.end())
        {
            warnings += wxString::Format(L"连线端点引用了不存在的元件（%d → %d），已跳过\r\n",
                                         temp.fromComponent, temp.toComponent);
            continue;
        }

        if (circuit.AddWire(PinRef{ fromIt->second, temp.fromPin },
                            PinRef{ toIt->second, temp.toPin }, temp.bends) == nullptr)
        {
            warnings += wxString::Format(L"连线 %d.%d → %d.%d 不合法，已跳过\r\n",
                                         fromIt->second, temp.fromPin, toIt->second, temp.toPin);
        }
    }

    circuit.NotifyChanged();

    if (error != nullptr)
    {
        *error = warnings;
    }
    return true;
}

bool LoadProjectJson(Circuit& circuit, const wxString& path, wxString* error)
{
    wxFileInputStream input(path);
    if (!input.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"无法打开文件：") + path;
        }
        return false;
    }

    // 逐行读入再拼接：JSON 的结构不依赖换行，而字符串内的换行已被转义为 \\n，
    // 因此按行读取不会破坏任何字段内容
    wxTextInputStream text(input, wxString(L" \t\r\n"), wxConvUTF8);
    wxString content;
    while (!input.Eof())
    {
        content += text.ReadLine();
        content += L'\n';
    }

    return DeserializeCircuitFromJson(circuit, content, error);
}
} // namespace project

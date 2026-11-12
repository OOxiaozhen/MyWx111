#include "PcbFileExporter.h"

#include "../model/Circuit.h"
#include "../model/Component.h"
#include "../model/ComponentType.h"

#include <wx/datetime.h>
#include <wx/txtstrm.h>
#include <wx/utils.h>
#include <wx/wfstream.h>

#include <vector>

#include "GedaFootprints.h"

namespace geda
{
namespace
{
    /// pcb 坐标单位：1/100 mil（1 mil = 1/1000 英寸），故 100 mil 间距 = 10000。
    constexpr int kDipPinPitch = 10000;
    /// DIP 两列间距 300 mil
    constexpr int kDipRowSpacing = 30000;
    /// 焊盘（外径）
    constexpr int kPadSize = 6000;
    /// 焊盘阻焊环间隙
    constexpr int kPadMask = 1000;
    /// 钻孔直径
    constexpr int kPadDrill = 4000;
    /// 丝印线宽
    constexpr int kSilkWidth = 1000;

    /// 生成单个焊盘的 pcb 文本行。
    /// @param x 焊盘 X（相对元件原点）
    /// @param y 焊盘 Y（相对元件原点）
    /// @param number 引脚编号，从 1 起
    /// @param square 是否用方形焊盘（用于标记第 1 脚）
    /// @return 一行 Pin 定义
    wxString MakePinLine(int x, int y, int number, bool square)
    {
        return wxString::Format(L"\tPin[%d %d %d %d %d %d \"%d\" \"%d\" \"%s\"]\n", x, y, kPadSize,
                                kPadMask, kPadSize, kPadDrill, number, number,
                                square ? L"square" : L"");
    }

    /// 生成 DIP14 的 14 个焊盘与丝印外框。
    /// 标准排布：pin1..7 在左列自上而下，pin8..14 在右列自下而上，
    /// 两列间距 300 mil、同一列内引脚间距 100 mil。
    /// @return 元件体文本
    wxString MakeDip14Body()
    {
        wxString out;
        for (int i = 0; i < 7; ++i)
        {
            out += MakePinLine(0, i * kDipPinPitch, i + 1, i == 0);
        }
        for (int i = 0; i < 7; ++i)
        {
            out += MakePinLine(kDipRowSpacing, 6 * kDipPinPitch - i * kDipPinPitch, i + 8, false);
        }

        // 丝印方框：便于在 pcb 中肉眼识别元件边界与朝向
        out += wxString::Format(L"\tElementLine[0 0 %d 0 %d]\n", kDipRowSpacing, kSilkWidth);
        out += wxString::Format(L"\tElementLine[%d 0 %d %d %d]\n", kDipRowSpacing, kDipRowSpacing,
                                6 * kDipPinPitch, kSilkWidth);
        out += wxString::Format(L"\tElementLine[%d %d 0 %d %d]\n", kDipRowSpacing,
                                6 * kDipPinPitch, 6 * kDipPinPitch, kSilkWidth);
        out += wxString::Format(L"\tElementLine[0 %d 0 0 %d]\n", 6 * kDipPinPitch, kSilkWidth);
        return out;
    }

    /// 生成单脚连接器的焊盘与丝印。
    /// @return 元件体文本
    wxString MakeSinglePinBody()
    {
        wxString out;
        out += MakePinLine(0, 0, 1, true);
        out += wxString::Format(L"\tElementLine[-5000 -5000 5000 -5000 %d]\n", kSilkWidth);
        out += wxString::Format(L"\tElementLine[5000 -5000 5000 5000 %d]\n", kSilkWidth);
        out += wxString::Format(L"\tElementLine[5000 5000 -5000 5000 %d]\n", kSilkWidth);
        out += wxString::Format(L"\tElementLine[-5000 5000 -5000 -5000 %d]\n", kSilkWidth);
        return out;
    }
} // namespace

wxString BuildPcbText(const Circuit& circuit, wxString* error)
{
    const std::vector<Circuit::ChipGroup> groups = circuit.GetChipGroups();

    wxString out;
    out += wxString::Format(L"# release: pcb 20140316\n");
    out += wxString::Format(L"# date: %s\n", wxDateTime::Now().FormatISOCombined(' '));
    out += wxString::Format(L"# user: %s\n", wxGetUserName());
    out += wxString::Format(L"# host: %s\n", wxGetHostName());
    out += L"\n";

    out += L"FileVersion[20140316]\n\n";
    out += L"PCB[\"\" 400000 300000]\n\n";
    out += L"Grid[10000 0 0 0]\n";
    out += L"Cursor[0 0 0]\n\n";

    // 只声明用得到的两个铜层；pcb 允许其余层由用户后续添加
    out += L"Layer(1 \"top\")\n(\n)\n";
    out += L"Layer(2 \"bottom\")\n(\n)\n\n";

    // 元件按网格排布：每行 5 片，行距 800 mil、列距 600 mil，
    // 避免所有封装叠在同一位置（pcb 载入后仍需 Disperse 才能进一步布局）
    constexpr int kColumnStep = 60000;
    constexpr int kRowStep = 80000;
    constexpr int kColumnsPerRow = 5;
    constexpr int kStartX = 10000;
    constexpr int kStartY = 10000;

    int placed = 0;
    for (const Circuit::ChipGroup& group : groups)
    {
        // 封装与型号：用户填写优先，否则按类型默认
        wxString footprint = group.footprint;
        wxString value = group.value;

        for (const auto& gate : group.gates)
        {
            const Component* comp = circuit.FindComponent(gate.first);
            if (comp == nullptr)
            {
                continue;
            }
            if (footprint.empty())
            {
                footprint = comp->GetFootprint();
                if (footprint.empty())
                {
                    footprint = DefaultFootprint(comp->GetType());
                }
            }
            if (value.empty())
            {
                value = comp->GetValue();
                if (value.empty())
                {
                    value = DefaultValue(comp->GetType());
                }
            }
            break;
        }

        if (footprint.empty())
        {
            continue;
        }

        const int x = kStartX + (placed % kColumnsPerRow) * kColumnStep;
        const int y = kStartY + (placed / kColumnsPerRow) * kRowStep;
        ++placed;

        out += wxString::Format(L"Element[\"\" \"%s\" \"%s\" \"%s\" %d %d 0 100 \"\"]\n(\n",
                                group.refDes, value, footprint, x, y);

        if (footprint == wxString(kDip14Footprint))
        {
            out += MakeDip14Body();
        }
        else
        {
            out += MakeSinglePinBody();
        }

        out += L")\n\n";
    }

    out += L"Mark[0 0]\n";

    if (!ValidatePcbText(out, error))
    {
        return wxString();
    }

    if (error != nullptr)
    {
        error->clear();
    }
    return out;
}

bool ValidatePcbText(const wxString& text, wxString* error)
{
    if (text.empty())
    {
        if (error != nullptr)
        {
            *error = wxString(L"生成的 pcb 文本为空");
        }
        return false;
    }

    // 括号配平：pcb 的解析器遇到不配对的括号会直接失败，
    // 而人工核对上百行文本很容易漏看，这里用计数硬校验
    int parens = 0;
    int brackets = 0;
    for (const wxUniChar ch : text)
    {
        if (ch == L'(')
        {
            ++parens;
        }
        else if (ch == L')')
        {
            --parens;
            if (parens < 0)
            {
                break;
            }
        }
        else if (ch == L'[')
        {
            ++brackets;
        }
        else if (ch == L']')
        {
            --brackets;
            if (brackets < 0)
            {
                break;
            }
        }
    }

    if (parens != 0 || brackets != 0)
    {
        if (error != nullptr)
        {
            *error = wxString::Format(L"生成的 pcb 文本括号不配平（圆括号差 %d、方括号差 %d）",
                                      parens, brackets);
        }
        return false;
    }

    const wchar_t* const kRequiredTokens[] = { L"FileVersion[", L"PCB[", L"Mark[" };
    for (const wchar_t* token : kRequiredTokens)
    {
        if (text.Find(token) == wxNOT_FOUND)
        {
            if (error != nullptr)
            {
                *error = wxString::Format(L"生成的 pcb 文本缺少必需记号：%s", token);
            }
            return false;
        }
    }
    return true;
}

bool ExportPcbFile(const Circuit& circuit, const wxString& path, wxString* error)
{
    const wxString text = BuildPcbText(circuit, error);
    if (text.empty())
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

    // 固定 Unix 换行：gEDA 工具链在 Linux 下运行
    wxTextOutputStream out(stream, wxEOL_UNIX);
    out << text;
    out.Flush();

    if (!stream.IsOk())
    {
        if (error != nullptr)
        {
            *error = wxString(L"写入文件失败：") + path;
        }
        return false;
    }
    return true;
}
} // namespace geda

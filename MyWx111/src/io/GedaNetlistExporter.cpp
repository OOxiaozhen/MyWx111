#include "GedaNetlistExporter.h"

#include "../model/Circuit.h"
#include "../model/Component.h"
#include "../model/ComponentType.h"

#include <wx/datetime.h>
#include <wx/txtstrm.h>
#include <wx/utils.h>
#include <wx/wfstream.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "GedaFootprints.h"
#include "NetlistExporter.h"

namespace geda
{
namespace
{
    /// 元件所属的物理芯片与门序号。
    struct ChipSlot
    {
        wxString refDes; ///< 芯片参考编号
        int slot = 1;    ///< 该元件是芯片中的第几个门
    };

    /// 一片芯片的封装与型号。
    struct ChipInfo
    {
        wxString footprint; ///< 封装名
        wxString value;     ///< 器件型号
    };

    /// 按元件类型与门序号把逻辑引脚号换算成封装上的物理引脚号。
    /// @param type 元件类型
    /// @param slot 门序号
    /// @param logicalPin 元件内的引脚索引
    /// @param[in,out] error 无法映射时累积中文错误
    /// @return 物理引脚号；无法映射返回 -1
    int ResolvePin(ComponentType type, int slot, int logicalPin, wxString& error)
    {
        const int pin = MapLogicalPinToDip14(type, slot, logicalPin);
        if (pin < 0)
        {
            error += wxString::Format(L"元件类型 %d 的引脚 %d 无法映射到封装脚位\r\n",
                                      static_cast<int>(type), logicalPin);
        }
        return pin;
    }
} // namespace

wxString BuildGedaNetlistText(const Circuit& circuit, wxString* error,
                              const NetlistOptions& options)
{
    wxString localError;

    // 芯片分组决定每个元件属于哪片物理芯片、占第几个门
    const std::vector<Circuit::ChipGroup> groups = circuit.GetChipGroups();

    std::map<int, ChipSlot> idToChip;
    for (const Circuit::ChipGroup& group : groups)
    {
        for (const auto& gate : group.gates)
        {
            idToChip[gate.first] = ChipSlot{ group.refDes, gate.second };
        }
    }

    // 芯片级封装与型号：用户填了就优先取用户值，否则按元件类型取默认值。
    // 四门器件同型，组内任一元件即可代表整片芯片。
    std::map<wxString, ChipInfo> chipInfo;
    for (const Circuit::ChipGroup& group : groups)
    {
        ChipInfo info{ group.footprint, group.value };

        for (const auto& gate : group.gates)
        {
            const Component* comp = circuit.FindComponent(gate.first);
            if (comp == nullptr)
            {
                continue;
            }
            if (info.footprint.empty())
            {
                info.footprint = comp->GetFootprint();
                if (info.footprint.empty())
                {
                    info.footprint = DefaultFootprint(comp->GetType());
                }
            }
            if (info.value.empty())
            {
                info.value = comp->GetValue();
                if (info.value.empty())
                {
                    info.value = DefaultValue(comp->GetType());
                }
            }
            break;
        }

        if (!info.footprint.empty())
        {
            chipInfo[group.refDes] = info;
        }
    }

    // 网络划分复用 KiCad 导出那套并查集规则
    const std::vector<std::vector<netlist::PinKey>> nets = netlist::BuildNets(circuit);

    std::vector<std::vector<wxString>> netNodes;
    netNodes.reserve(nets.size());

    for (const std::vector<netlist::PinKey>& net : nets)
    {
        std::vector<wxString> nodes;
        nodes.reserve(net.size());

        for (const netlist::PinKey& key : net)
        {
            const auto chipIt = idToChip.find(key.componentId);
            if (chipIt == idToChip.end())
            {
                continue;
            }

            const Component* comp = circuit.FindComponent(key.componentId);
            if (comp == nullptr)
            {
                continue;
            }

            // 分线器只表达逻辑分支，不占物理封装，也不产生网表引脚
            if (comp->GetType() == ComponentType::Splitter)
            {
                continue;
            }

            const int physicalPin =
                ResolvePin(comp->GetType(), chipIt->second.slot, key.pinIndex, localError);
            if (physicalPin < 0)
            {
                continue;
            }

            nodes.push_back(
                wxString::Format(L"%s-%d", chipIt->second.refDes, physicalPin));
        }

        if (nodes.empty())
        {
            continue;
        }

        // 同一网络内按 "位号-引脚号" 字典序排序，保证输出可复现
        std::sort(nodes.begin(), nodes.end(),
                  [](const wxString& lhs, const wxString& rhs) { return lhs < rhs; });
        nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
        netNodes.push_back(std::move(nodes));
    }

    std::sort(netNodes.begin(), netNodes.end(),
              [](const std::vector<wxString>& lhs, const std::vector<wxString>& rhs)
              { return lhs.front() < rhs.front(); });

    wxString out;
    out += wxString::Format(L"# release: %s\n", options.releaseTag);
    out += wxString::Format(L"# date: %s\n", wxDateTime::Now().FormatISOCombined(' '));
    out += wxString::Format(L"# user: %s\n", wxGetUserName());
    out += wxString::Format(L"# host: %s\n", wxGetHostName());
    out += L"\n";

    int index = 1;
    for (const Circuit::ChipGroup& group : groups)
    {
        const auto infoIt = chipInfo.find(group.refDes);
        if (infoIt == chipInfo.end())
        {
            continue;
        }

        const ChipInfo& info = infoIt->second;
        if (options.includeValueOnTLine && !info.value.empty())
        {
            out += wxString::Format(L"T%d %s %s %s\n", index++, group.refDes, info.footprint,
                                    info.value);
        }
        else
        {
            out += wxString::Format(L"T%d %s %s\n", index++, group.refDes, info.footprint);
        }
    }

    out += L"\n";

    index = 1;
    for (const std::vector<wxString>& nodes : netNodes)
    {
        out += wxString::Format(L"N%d", index++);
        for (const wxString& node : nodes)
        {
            out += L" ";
            out += node;
        }
        out += L"\n";
    }

    if (options.appendEndMarker)
    {
        out += L"\n# End of netlist\n";
    }

    if (!localError.empty())
    {
        if (error != nullptr)
        {
            *error = localError;
        }
        return wxString();
    }

    if (error != nullptr)
    {
        error->clear();
    }
    return out;
}

bool ExportGedaNetlist(const Circuit& circuit, const wxString& path, wxString* error,
                       const NetlistOptions& options)
{
    // 同一片芯片内 slot 重复会导致两个门抢同一组焊盘，
    // 必须在写盘前拦下，而不是输出一份看似正常的错误网表
    wxString conflict;
    if (circuit.HasSlotConflict(&conflict))
    {
        if (error != nullptr)
        {
            *error = conflict + wxString(L"\n请先修正重复的 slot 后再导出。");
        }
        return false;
    }

    const wxString text = BuildGedaNetlistText(circuit, error, options);
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

    // 固定 Unix 换行：gEDA 工具链在 Linux 下运行，避免换行符差异
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

#pragma once
// Selection accessor: the R/use-key selected target.

#include "cube/common.hpp"

namespace cube
{
    class Selection
    {
    public:
        explicit Selection(const CubeApi* api) : m_api(api), m_valid(api && api->selection.getLast(api, &m_data) != 0) {}

        bool valid() const { return m_valid; } // false until the first selection happens
        bool refresh() { m_valid = m_api && m_api->selection.getLast(m_api, &m_data) != 0; return m_valid; }
        unsigned getAddress() const { return m_data.address; } // selected creature/object (0 = world)
        int getTypeByte() const { return m_data.typeByte; }
        SelectionKind getKind() const { return static_cast<SelectionKind>(m_data.kind); }
        const char* getKindName() const { return selectionKindName(getKind()); }
        unsigned getCount() const { return m_data.count; } // total selections this session
        bool isContainer() const { return getKind() == SelectionKind::Container; }
        bool isDialog() const { return getKind() == SelectionKind::Dialog; }
        bool isCreature() const { return getKind() == SelectionKind::Creature; }
        bool isWorld() const { return getKind() == SelectionKind::World; }
        const CubeSelection& raw() const { return m_data; }

    private:
        const CubeApi* m_api;
        CubeSelection m_data = {};
        bool m_valid;
    };
}

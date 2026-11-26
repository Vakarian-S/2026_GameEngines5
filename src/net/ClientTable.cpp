#include "net/ClientTable.h"

#include <algorithm>

NET::ClientSlot* NET::ClientTable::Find(const std::uint32_t connection)
{
    const auto slot = std::ranges::find(slots, connection, &ClientSlot::connection);
    return slot == slots.end() ? nullptr : &*slot;
}

std::size_t NET::ClientTable::PlayerCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(slots, [](const ClientSlot& slot)
    {
        return slot.playerNetworkId != 0;
    }));
}
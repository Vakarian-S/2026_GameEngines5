#include "ecs/Resources.h"

Resources::~Resources()
{
    for (size_t i = count; i > 0; i--)
    {
        const auto destructor = slots[order[i - 1]].destroy;
        destructor(slots[order[i - 1]].value, memory);
    }
}

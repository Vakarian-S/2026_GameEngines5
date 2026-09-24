#pragma once
#include <vector>

#include "Entity.h"
#include "EntitySlot.h"

class World
{
public:
    World();
    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    bool IsEntityAlive(Entity entity) const;

private:
    std::vector<EntitySlot> entitySlots;
    std::vector<uint32_t> freeSlots;
};

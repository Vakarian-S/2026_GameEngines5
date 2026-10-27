#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/** Turns every entity with a Spin about its up axis, at its own speed */
class SpinSystem : public System
{
public:
    /** @param world the world whose spinning entities this system turns */
    explicit SpinSystem(World& world);

    void Run(const SystemContext& context) override;

private:
    /** Every entity with a Transform and a Spin */
    Query spinners;
};
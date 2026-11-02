#pragma once
#include "ecs/Query.h"
#include "ecs/System.h"

class World;

/**
 * Draws a line between the current mouse position to the grabbed point of a Picked Entity
 */
class DrawGrabLineSystem : public System
{
public:
    /** @param world the world whose entities this system reads */
    explicit DrawGrabLineSystem(World& world);
    
    void Run(const SystemContext& context) override;
private:
    /** Picked Entities to draw the lines to */
    Query entities;
};

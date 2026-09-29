#pragma once
#include "core/Scene.h"
#include "ecs/CommandBuffer.h"
#include "ecs/Query.h"
#include "ecs/World.h"

class Scene0 : public Scene
{
public:
    Scene0();
    ~Scene0() override = default;

    void HandleEvents(const SDL_Event& sdlEvent) override;
    void MakeDrawList(DrawList& drawList) override;
    bool OnCreate() override;
    void OnDestroy() override;
    void Update(float deltaTime) override;

private:
    World world;
    CommandBuffer commandBuffer;

    Query testQuery;
};

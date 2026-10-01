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

    /** @return this scene's world, so Main can read its counters */
    const World& GetWorld() const { return world; }

private:
    World world;
    CommandBuffer commandBuffer;
    Query movers;

    /** Every entity this scene spawned, in spawn order */
    std::vector<Entity> entities;
    /** Frames this scene has updated */
    uint64_t frameNumber = 0;
    /** True while the first entities carry the Hit tag */
    bool hitOn = false;

    /** Entities OnCreate spawns */
    static constexpr uint32_t kEntityCount = 1000;
    /** Bytes in the world's slab. This scene's four archetypes use about 110,000. */
    static constexpr std::size_t kWorldArenaBytes = 256 * 1024;
    /** Entities each damage frame tags or untags */
    static constexpr int kDamageCount = 100;
    /** Frames between one damage frame and the next */
    static constexpr uint64_t kDamageEvery = 60;
};

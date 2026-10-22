#pragma once
#include <array>

#include "core/Camera.h"
#include "core/FlyCameraController.h"
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
    bool OnCreate(ResourceLoader& resourceLoader) override;
    void OnDestroy() override;
    void Update(float deltaTime) override;

    /** @return this scene's world, so Main can read its counters */
    const World& GetWorld() const override { return world; }

    /** @return the camera this scene is seen through */
    const Camera& GetCamera() const override { return camera; }

    /** @return the ambient light and this scene's one light */
    SceneLighting GetLighting() const override { return {.ambient = ambient, .lights = lights}; }

private:
    World world;
    CommandBuffer commandBuffer;
    Query movers;
    /** Where the scene is seen from. It looks at the centre of the grid from far enough back to see all of it */
    Camera camera;

    /** Light that reaches every surface, so the sides facing away from the light are not black */
    MATH::Vec3 ambient{0.05f, 0.05f, 0.2f};
    /** One white light above and in front of the grid, between it and the camera */
    std::array<Light, 1> lights{
        {
            {.position = MATH::Vec3(0.0f, 20.0f, 30.0f), .color = MATH::Vec3(1.0f, 1.0f, 1.0f)},
        }
    };

    /** The mesh every entity is drawn with */
    MeshHandle cubeMesh;
    /** The two materials the cubes alternate between, so neighbouring draw items differ */
    std::array<MaterialHandle, 2> materials;

    /** Every entity this scene spawned, in spawn order */
    std::vector<Entity> entities;
    /** Frames this scene has updated */
    uint64_t frameNumber = 0;
    /** True while the first entities carry the Hit tag */
    bool hitOn = false;

    /** Entities along each side of the grid OnCreate spawns */
    static constexpr uint32_t kGridSide = 10;
    /** Entities OnCreate spawns: one per grid cell */
    static constexpr uint32_t kEntityCount = kGridSide * kGridSide * kGridSide;
    /** Metres between neighbouring entities in the grid */
    static constexpr float kGridSpacing = 2.0f;
    /** Half the width of the box entities move in. Past one wall, an entity comes back in through the other */
    static constexpr float kBoxHalfWidth = 10.0f;
    /** Bytes in the world's slab. This scene's four archetypes use about 110,000. */
    static constexpr std::size_t kWorldArenaBytes = 256 * 1024;
    /** Entities each damage frame tags or untags */
    static constexpr int kDamageCount = 100;
    /** Frames between one damage frame and the next */
    static constexpr uint64_t kDamageEvery = 60;
    
    /** Controller to move the camera with user input */
    FlyCameraController flyCameraController;
};

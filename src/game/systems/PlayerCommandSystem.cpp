#include "game/systems/PlayerCommandSystem.h"

#include <iostream>

#include "VMath.h"
#include "core/InputState.h"
#include "ecs/ChunkView.h"
#include "ecs/SystemContext.h"

PlayerCommandSystem::PlayerCommandSystem(World& world) :
    System("PlayerCommand", 0, COMPONENTS::MaskOf<PlayerCommand>()),
    localPlayers(world, COMPONENTS::MaskOf<PlayerCommand, LocalPlayer>())
{
}

void PlayerCommandSystem::Run(const SystemContext& context)
{
    const InputState& input = context.Get<InputState>();

    /* W is up the screen, which is -Z from the overhead camera. Opposite keys cancel */
    MATH::Vec3 direction(0.0f, 0.0f, 0.0f);
    if (input.Held(SDL_SCANCODE_W)) direction.z -= 1.0f;
    if (input.Held(SDL_SCANCODE_S)) direction.z += 1.0f;
    if (input.Held(SDL_SCANCODE_A)) direction.x -= 1.0f;
    if (input.Held(SDL_SCANCODE_D)) direction.x += 1.0f;

    if (direction.x != 0.0f || direction.z != 0.0f) direction = MATH::VMath::normalize(direction);

    const PlayerCommand command{.moveDirection = direction, .fire = input.Held(SDL_SCANCODE_SPACE)};

    localPlayers.ForEachChunk([&](ChunkView chunk)
    {
        PlayerCommand* commands = chunk.Column<PlayerCommand>();
        const uint32_t rows = chunk.Count();
        for (uint32_t row = 0; row < rows; ++row)
        {
            commands[row] = command;
        }
    });
}
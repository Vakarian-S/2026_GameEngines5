#include "scenes/Scene0.h"

#include <iostream>
#include <SDL3/SDL.h>

Scene0::Scene0() : testQuery(world, 0)
{
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent)
{
    if (sdlEvent.type == SDL_EVENT_KEY_DOWN)
    {
        if (sdlEvent.key.key == SDLK_W)
        {
            std::cout << "W pressed" << std::endl;
        }
    }
}

void Scene0::MakeDrawList(DrawList& drawList)
{
    // drawList.clear();
}

bool Scene0::OnCreate()
{
    return true;
}

void Scene0::OnDestroy()
{
}

void Scene0::Update(float deltaTime)
{
}

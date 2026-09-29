#include "scenes/Scene0.h"

Scene0::Scene0() : testQuery(world, 0)
{
}

void Scene0::HandleEvents(const SDL_Event& sdlEvent)
{
}

void Scene0::MakeDrawList(DrawList& drawList)
{
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

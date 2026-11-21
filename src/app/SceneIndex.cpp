#include "app/SceneIndex.h"

#include <charconv>
#include <stdexcept>
#include <string>

#include "core/SceneManager.h"
#include "scenes/PacingScene.h"
#include "scenes/PhysicsScene.h"
#include "scenes/Scene0.h"
#include "scenes/Scene1.h"

namespace
{
    /** Every scene the program can run. F1 picks row 0, F2 row 1 and so on. At file scope, so it outlives the manager's span */
    constexpr SceneEntry kScenes[] = {
        {.name = "Scene0", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene0>(); }},
        {.name = "Scene1", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<Scene1>(); }},
        {.name = "Physics Scene", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<PhysicsScene>(); }},
        {.name = "Pacing Scene", .build = []() -> std::unique_ptr<Scene> { return std::make_unique<PacingScene>(); }}
    };
}

std::size_t PickScene(const int argc, char* argv[])
{
    constexpr std::size_t sceneCount = std::size(kScenes);

    /* Wrap argv in a span and skip argv[0] */
    const auto args = std::span(argv, argc).subspan(1);
    const auto flag = std::ranges::find(args, std::string_view("--scene"));
    if (flag == args.end()) return sceneCount - 1;

    const auto value = std::next(flag);
    if (value == args.end()) throw std::invalid_argument("--scene needs a row number, such as --scene 3");

    const std::string_view text = *value;
    std::size_t index = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), index);

    /* Bad when it is not a number, has extra characters after the number, or is past the table */
    if (error != std::errc() || end != text.data() + text.size() || index >= sceneCount)
    {
        std::string message = "no scene \"" + std::string(text) + "\". Pick one of:\n";
        for (std::size_t i = 0; i < sceneCount; ++i)
        {
            message += "  " + std::to_string(i) + ": " + kScenes[i].name + "\n";
        }
        throw std::invalid_argument(message);
    }
    return index;
}

std::span<const SceneEntry> Scenes()
{
    return kScenes;
}

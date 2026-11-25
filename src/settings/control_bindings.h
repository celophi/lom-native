#ifndef LOM_SETTINGS_CONTROL_BINDINGS_H
#define LOM_SETTINGS_CONTROL_BINDINGS_H

#include <array>

namespace lom::settings
{

constexpr int player_count = 2;

constexpr int button_count = 16;

using Bindings = std::array<std::array<int, button_count>, player_count>;

using BindingPresence = std::array<std::array<bool, button_count>, player_count>;

struct ControlBindings
{
    Bindings keys{};

    Bindings pads{};

    bool operator==(const ControlBindings& other) const
    {
        return keys == other.keys && pads == other.pads;
    }
};

}
#endif

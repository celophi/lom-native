#ifndef LOM_INPUT_BINDING_CAPTURE_H
#define LOM_INPUT_BINDING_CAPTURE_H

#include <SDL3/SDL_joystick.h>

namespace lom::settings
{
struct BindingCapture
{
    int port = -1;

    int button = -1;

    bool pad = false;

    SDL_JoystickID device = 0;

    /** Rebinding a control: first wait for every input to be let go, then
     * take the next key, button or trigger. */
    bool active() const;

    /** Still waiting for every input to be let go. */
    bool waiting() const;

    /** Taking the next key, button or trigger. */
    bool listening() const;

  private:
    friend class HostControls;

    enum class Phase
    {
        Idle,
        WaitingForRelease,
        Listening
    };

    Phase phase_ = Phase::Idle;
};

}
#endif

#ifndef LOM_INPUT_HOST_CONTROLS_H
#define LOM_INPUT_HOST_CONTROLS_H

#include "../settings/control_bindings.h"
#include "binding_capture.h"
#include <SDL3/SDL_events.h>
#include <string>

namespace lom::settings
{
class HostControls
{
  public:
    /**
     * @brief Rebind controls in a set of bindings.
     * @param bindings The bindings, which must outlive this.
     */
    explicit HostControls(ControlBindings& bindings);

    /**
     * @brief The bindings this changes.
     * @return The bindings.
     */
    const ControlBindings& bindings() const;

    /** The control being rebound, if any. */
    const BindingCapture& capture() const;

    /** What to tell the player about the last rebind, or empty. */
    const std::string& message() const;

    /** Whether the menu should ignore input until everything is let go. */
    bool navigation_blocked() const;

    /** True once after rebinding starts, stops or is reset, so the game drops
     * the input it was holding. */
    bool take_input_reset();

    /** Start rebinding one player's PS1 button (0 to 15) to a key, or to a
     * gamepad control if pad is set. */
    void begin(int port, int button, bool pad);

    /** Stop rebinding. The menu ignores input until everything is let go. */
    void cancel();

    /** Forget any rebind in progress, for when the menu changes page. */
    void reset_session();

    /** Call every frame. Cancels a rebind if the player's gamepad changed, and
     * starts listening once everything is let go. */
    void update();

    /** Pass SDL events here while rebinding. */
    void handle_capture_event(const SDL_Event& event);

    /** Bind the control to a key or gamepad value. A control the same player
     * already uses for another button swaps with it; a key the other player
     * uses is refused. */
    void assign(int value);

    /** Unbind the control being rebound. */
    void clear();

    /** Put one player's defaults back, unbinding any default key the other
     * player has taken. */
    void restore(int port);

  private:
    ControlBindings& bindings_;

    BindingCapture capture_;

    std::string message_;

    bool navigation_blocked_ = false;

    bool reset_input_ = false;
};

}
#endif

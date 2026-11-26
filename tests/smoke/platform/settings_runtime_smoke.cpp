#include "../../../src/input/binding_policy.h"
#include "../../../src/ui/menu_runtime.h"
#include "../../../src/settings/service.h"
#include "lom_native_widescreen.h"
#include "lom_native_audio_categories.h"
#include "lom_native_host_audio.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "../../support/lom_check.h"

static constexpr char storage_directory[] = "storage";

static constexpr auto main_game_window_id = 42;

static constexpr auto main_unrelated_window_id = 43;

static constexpr char preferences_filename[] = "preferences.ini";

static constexpr char success_message[] = "settings-runtime: pause/quit/release and capture/conflict transitions pass without ImGui or GPU";

static constexpr int ps1_up_bit = 12;

static constexpr int ps1_left_bit = 15;

static constexpr auto main_normal_argument_count = 2;

static bool keys[SDL_SCANCODE_COUNT];

/**
 * @brief Expose deterministic fixture keyboard state instead of physical keys.
 * @param count Receives the scancode count when non-null.
 * @return Borrowed fixture scancode array.
 */
extern "C" const bool* __wrap_SDL_GetKeyboardState(int* count)
{
    if (count)
    {
        *count = SDL_SCANCODE_COUNT;
    }

    return keys;
}

/**
 * @brief Construct a keyboard event for an ordered policy scenario.
 * @param code Keyboard scancode to inject.
 * @return Initialized key event.
 */
static SDL_Event key(SDL_Scancode code)
{
    SDL_Event event = {};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = code;
    return event;
}

/**
 * @brief Verify menu pause, quit, input release and remapping transitions.
 * @param argc Argument count supplied by the test driver.
 * @param argv Arguments containing the disposable fixture directory.
 * @return Zero on success; failed assertions terminate the process.
 */
int main(int argc, char** argv)
{
    using namespace lom::settings;
    CHECK(argc == main_normal_argument_count && SDL_Init(SDL_INIT_GAMEPAD));
    const auto directory = std::filesystem::path(argv[1]) / storage_directory;
    settings().enable((directory / preferences_filename).string().c_str());
    MenuRuntime runtime(settings());
    runtime.attach_window(main_game_window_id);
    CHECK(runtime.handle_event(key(SDL_SCANCODE_F1)).consumed && runtime.open());
    runtime.set_pause_unfocused(true);
    SDL_Event focus = {};
    focus.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    focus.window.windowID = main_unrelated_window_id;
    CHECK(!runtime.handle_event(focus).consumed);
    runtime.set_open(false);
    runtime.update();
    CHECK(!runtime.paused());
    CHECK(!settings().store().error().empty());
    CHECK(!settings().store().persisted().pause_unfocused);
    std::filesystem::create_directories(directory);
    settings().update();
    CHECK(!std::filesystem::exists(directory / preferences_filename));
    runtime.set_open(true);
    runtime.set_open(false);
    CHECK(settings().store().error().empty() && settings().store().persisted().pause_unfocused);
    focus.window.windowID = main_game_window_id;
    runtime.handle_event(focus);
    runtime.set_open(true);
    runtime.set_open(false);
    CHECK(runtime.paused());
    runtime.set_open(true);
    focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    runtime.handle_event(focus);
    CHECK(runtime.paused());
    runtime.request_quit();
    CHECK(runtime.confirming_quit());
    runtime.handle_event(key(SDL_SCANCODE_ESCAPE));
    CHECK(runtime.open() && !runtime.confirming_quit());
    runtime.set_open(false);
    keys[SDL_SCANCODE_X] = true;
    runtime.update();
    CHECK(runtime.blocks_input());
    CHECK(runtime.handle_event(key(SDL_SCANCODE_F5)).consumed);
    keys[SDL_SCANCODE_X] = false;
    runtime.update();
    CHECK(!runtime.blocks_input());

    auto bindings = default_bindings();
    HostControls controls(bindings);
    keys[SDL_SCANCODE_RETURN] = true;
    controls.begin(0, ps1_up_bit, false);
    controls.update();
    CHECK(controls.capture().waiting());
    controls.handle_capture_event(key(SDL_SCANCODE_B));
    CHECK(bindings.keys[0][ps1_up_bit] != SDL_SCANCODE_B);
    keys[SDL_SCANCODE_RETURN] = false;
    controls.update();
    CHECK(controls.capture().listening());
    keys[SDL_SCANCODE_B] = true;
    controls.handle_capture_event(key(SDL_SCANCODE_B));
    CHECK(!controls.capture().active() && controls.navigation_blocked());
    CHECK(bindings.keys[0][ps1_up_bit] == SDL_SCANCODE_B);
    controls.update();
    CHECK(controls.navigation_blocked());
    keys[SDL_SCANCODE_B] = false;
    controls.update();
    CHECK(!controls.navigation_blocked());
    int left = bindings.keys[0][ps1_left_bit];
    controls.begin(0, ps1_left_bit, false);
    controls.update();
    controls.assign(SDL_SCANCODE_B);
    CHECK(bindings.keys[0][ps1_left_bit] == SDL_SCANCODE_B && bindings.keys[0][ps1_up_bit] == left);
    controls.begin(0, ps1_up_bit, false);
    controls.update();
    controls.assign(bindings.keys[1][ps1_up_bit]);
    CHECK(controls.capture().active() && bindings.keys[0][ps1_up_bit] == left);
    controls.cancel();
    controls.restore(0);
    CHECK(bindings == default_bindings());

    CHECK(!lom_native_widescreen_enabled() && !settings().store().persisted().display.widescreen);
    lom_native_widescreen_set_enabled(true);
    settings().update();
    CHECK(settings().store().error().empty() && settings().store().persisted().display.widescreen);
    PreferenceStore reload;
    CHECK(reload.load((directory / preferences_filename).string(), Preferences{}).display.widescreen);
    lom_native_widescreen_set_enabled(false);
    settings().update();
    CHECK(!reload.load((directory / preferences_filename).string(), Preferences{}).display.widescreen);

    CHECK(lom_native_audio_category_volume(LOM_AUDIO_MUSIC) == 100 && !lom_native_audio_category_muted(LOM_AUDIO_SFX));
    lom_native_audio_category_set(LOM_AUDIO_MUSIC, 40, false);
    lom_native_audio_category_set(LOM_AUDIO_SFX, 70, true);
    lom_native_audio_category_set(LOM_AUDIO_CATEGORY_COUNT, 10, true);
    lom_native_audio_category_set(LOM_AUDIO_MUSIC, 101, true);
    CHECK(lom_native_audio_category_volume(LOM_AUDIO_MUSIC) == 40 && !lom_native_audio_category_muted(LOM_AUDIO_MUSIC));
    CHECK(lom_native_audio_category_volume(LOM_AUDIO_SFX) == 70 && lom_native_audio_category_muted(LOM_AUDIO_SFX));
    static int sink_music = -1, sink_sfx = -1;
    lom_native_audio_set_category_sink(
        [](int music, int sfx)
        {
            sink_music = music;
            sink_sfx = sfx;
        });
    CHECK(sink_music == 40 && sink_sfx == 0);
    lom_native_audio_category_set(LOM_AUDIO_SFX, 70, false);
    CHECK(sink_music == 40 && sink_sfx == 70);
    lom_native_audio_category_set(LOM_AUDIO_SFX, 70, true);
    lom_native_audio_set_category_sink(nullptr);
    settings().update();
    const auto audio = reload.load((directory / preferences_filename).string(), Preferences{}).audio;
    CHECK(audio.categories[LOM_AUDIO_MUSIC].volume == 40 && !audio.categories[LOM_AUDIO_MUSIC].muted);
    CHECK(audio.categories[LOM_AUDIO_SFX].volume == 70 && audio.categories[LOM_AUDIO_SFX].muted);
    CHECK(audio.master == settings().values().audio.master);
    SDL_Quit();
    std::puts(success_message);
}

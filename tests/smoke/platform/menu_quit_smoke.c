#include "lom_native_menu.h"
#include "lom_native_setjmp.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../support/lom_check.h"

int ResetGraph(int mode);
int VSync(int mode);
int Psyz_QuitRequested(void);
void lom_native_psyz_set_quit_hook(void (*hook)(void));
_Noreturn void Psyz_VideoClockFailure(void);

static const char preferences_name[] = "menu-quit-preferences.ini";

static const Uint32 menu_wait_ms = 500;

static LomJumpBuffer game_loop;

/** FIELD's quit hook: back out of the game loop, the way FIELD does. */
static void field_quit(void)
{
    LOM_LONGJMP(game_loop, 1);
}

/** The shutdown's quit hook: the last VSync mustn't exit before cleanup. */
static void defer_quit(void)
{
}

static int came_back;

/**
 * @brief At exit, check the last VSync of the shutdown returned, rather than
 *        quitting from inside it.
 */
static void check_came_back(void)
{
    if (!came_back)
    {
        fputs("menu-quit: the shutdown's last VSync never came back\n", stderr);
        _Exit(1);
    }
}

/** Queue a key press and release for PSY-Z's next event poll. */
static void queue_key(SDL_Scancode code)
{
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = code;
    event.key.down = true;
    CHECK(SDL_PushEvent(&event));
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    CHECK(SDL_PushEvent(&event));
}

/** The keys that go to Quit game and confirm, pushed from SDL's timer thread
 * once the menu has the game paused, so the quit comes from inside the
 * menu's pause loop as a player's does. */
static Uint32 SDLCALL choose_quit(void* unused, SDL_TimerID timer, Uint32 interval)
{
    (void)unused;
    (void)timer;
    (void)interval;
    queue_key(SDL_SCANCODE_UP);
    queue_key(SDL_SCANCODE_UP);
    queue_key(SDL_SCANCODE_RETURN);
    queue_key(SDL_SCANCODE_RIGHT);
    queue_key(SDL_SCANCODE_RETURN);
    return 0;
}

int main(void)
{
    remove(preferences_name);
    lom_native_menu_enable(preferences_name);
    ResetGraph(0);

    for (int frame = 0; frame < 3; ++frame)
    {
        VSync(0);
    }

    CHECK(!Psyz_QuitRequested());
    lom_native_psyz_set_quit_hook(field_quit);

    if (LOM_SETJMP(game_loop) == 0)
    {
        queue_key(SDL_SCANCODE_F1);
        CHECK(SDL_AddTimer(menu_wait_ms, choose_quit, NULL) != 0);
        VSync(0);
        CHECK(!"the quit hook should have jumped out");
    }

    CHECK(Psyz_QuitRequested());

    lom_native_psyz_set_quit_hook(defer_quit);
    atexit(check_came_back);
    VSync(0);
    came_back = 1;
    remove(preferences_name);
    puts("menu-quit: after quitting from the menu, the shutdown's last VSync comes back");

    fflush(stdout);
    Psyz_VideoClockFailure();
}

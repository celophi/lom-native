#include "lom_native_menu.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_mod_options.h"
#include "lom_native_host_audio.h"
#include "presentation.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "../../support/lom_check.h"

static constexpr Uint32 screen_width = 1280;
static constexpr Uint32 screen_height = 720;
static constexpr Uint32 bytes_per_pixel = 4;
static constexpr char preferences_name[] = "menu-render-preferences.ini";

struct Picture
{
    Uint32 width = 0;
    Uint32 height = 0;
    std::vector<unsigned char> rgb;
};

/** Read a P6 PPM, or an empty picture. */
static Picture read_ppm(const char* path)
{
    Picture picture;
    FILE* file = std::fopen(path, "rb");
    unsigned width = 0, height = 0, maximum = 0;

    if (file && std::fscanf(file, "P6 %u %u %u", &width, &height, &maximum) == 3 && maximum == 255 && std::fgetc(file) != EOF)
    {
        picture.width = width;
        picture.height = height;
        picture.rgb.resize(static_cast<std::size_t>(width) * height * 3);

        if (std::fread(picture.rgb.data(), 1, picture.rgb.size(), file) != picture.rgb.size())
        {
            picture = Picture{};
        }
    }

    if (file)
    {
        std::fclose(file);
    }

    return picture;
}

/** The game's picture at its own size, or sharp squares to show the blur on. */
static Picture game_picture(const Picture& given)
{
    if (given.width)
    {
        return given;
    }

    Picture picture;
    picture.width = 320;
    picture.height = 240;
    picture.rgb.resize(picture.width * picture.height * 3);

    for (Uint32 y = 0; y < picture.height; ++y)
    {
        for (Uint32 x = 0; x < picture.width; ++x)
        {
            const bool light = ((x / 8) + (y / 8)) % 2 == 0;
            unsigned char* out = &picture.rgb[(y * picture.width + x) * 3];
            out[0] = light ? 220 : 40;
            out[1] = light ? 200 : 90;
            out[2] = light ? 120 : 60;
        }
    }

    return picture;
}

/** Copy bytes to a new GPU texture. */
static SDL_GPUTexture* upload_picture(SDL_GPUDevice* device, const Picture& picture)
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = picture.width;
    info.height = picture.height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
    CHECK(texture);

    const Uint32 size = picture.width * picture.height * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo upload_info{SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, size, 0};
    SDL_GPUTransferBuffer* upload = SDL_CreateGPUTransferBuffer(device, &upload_info);
    CHECK(upload);
    auto* mapped = static_cast<unsigned char*>(SDL_MapGPUTransferBuffer(device, upload, false));
    CHECK(mapped);

    for (Uint32 i = 0; i < picture.width * picture.height; ++i)
    {
        std::memcpy(&mapped[i * bytes_per_pixel], &picture.rgb[i * 3], 3);
        mapped[i * bytes_per_pixel + 3] = 255;
    }

    SDL_UnmapGPUTransferBuffer(device, upload);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    CHECK(cmd);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo source{upload, 0, picture.width, picture.height};
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = picture.width;
    region.h = picture.height;
    region.d = 1;
    SDL_UploadToGPUTexture(copy, &source, &region, false);
    SDL_EndGPUCopyPass(copy);
    CHECK(SDL_SubmitGPUCommandBuffer(cmd));
    SDL_ReleaseGPUTransferBuffer(device, upload);
    return texture;
}

/** A left click, as the game passes it on. */
static void click(float x, float y)
{
    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.x = x;
    event.button.y = y;
    lom_native_menu_event(&event);
}

/** Pass a key press to the menu as the game would. */
static void key(SDL_Scancode code)
{
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = code;
    event.key.down = true;
    lom_native_menu_event(&event);
    event.type = SDL_EVENT_KEY_UP;
    event.key.down = false;
    lom_native_menu_event(&event);
}

/** Press and let go of a gamepad button on one controller, as the game would pass it on. */
static void pad_button(SDL_JoystickID pad, SDL_GamepadButton button)
{
    SDL_Event event{};
    event.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    event.gbutton.which = pad;
    event.gbutton.button = static_cast<Uint8>(button);
    event.gbutton.down = true;
    lom_native_menu_event(&event);
    event.type = SDL_EVENT_GAMEPAD_BUTTON_UP;
    event.gbutton.down = false;
    lom_native_menu_event(&event);
}

/** Attach a virtual controller that SDL takes for the real thing by its USB IDs. */
static SDL_JoystickID attach_pad(const char* name, Uint16 vendor, Uint16 product)
{
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.vendor_id = vendor;
    desc.product_id = product;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = name;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    CHECK(id != 0);
    return id;
}

struct Game
{
    SDL_GPUTexture* frame;
    Uint32 width;
    Uint32 height;
    LomPresentation presentation;
};

struct Screen
{
    SDL_GPUTexture* texture;
    Uint32 width;
    Uint32 height;
};

/**
 * @brief Make a texture for the menu to draw on.
 * @param device The GPU.
 * @param width Its width.
 * @param height Its height.
 * @return The texture and its size.
 */
static Screen make_screen(SDL_GPUDevice* device, Uint32 width, Uint32 height)
{
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &info);
    CHECK(texture);
    return Screen{texture, width, height};
}

/** Present the game's frame onto the screen, softened or not, with or without the menu, and read it back. */
static std::vector<unsigned char> frame(SDL_GPUDevice* device, const Screen& screen, Game& game, bool soften, bool menu)
{
    SDL_GPUTexture* target = screen.texture;
    const Uint32 screen_width = screen.width;
    const Uint32 screen_height = screen.height;
    const Uint32 size = screen_width * screen_height * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo download_info{SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD, size, 0};
    SDL_GPUTransferBuffer* download = SDL_CreateGPUTransferBuffer(device, &download_info);
    CHECK(download);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    CHECK(cmd);

    const Uint32 fitted_width = std::min(screen_width, screen_height * 4 / 3);
    const Uint32 fitted_height = std::min(screen_height, screen_width * 3 / 4);
    const SDL_Rect fitted = {static_cast<int>((screen_width - fitted_width) / 2), static_cast<int>((screen_height - fitted_height) / 2),
                             static_cast<int>(fitted_width), static_cast<int>(fitted_height)};
    SDL_GPUBlitRegion source{};
    source.texture = game.frame;
    source.w = game.width;
    source.h = game.height;
    game.presentation.soften = soften;
    lom_present_blit(&game.presentation, device, cmd, source, target, fitted, 4.0f / 3.0f, 1);

    if (menu)
    {
        lom_native_menu_draw(cmd, target, screen_width, screen_height);
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region{};
    region.texture = target;
    region.w = screen_width;
    region.h = screen_height;
    region.d = 1;
    SDL_GPUTextureTransferInfo destination{download, 0, screen_width, screen_height};
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    CHECK(fence && SDL_WaitForGPUFences(device, true, &fence, 1));
    SDL_ReleaseGPUFence(device, fence);

    std::vector<unsigned char> result(size);
    void* mapped = SDL_MapGPUTransferBuffer(device, download, false);
    CHECK(mapped);
    std::memcpy(result.data(), mapped, size);
    SDL_UnmapGPUTransferBuffer(device, download);
    SDL_ReleaseGPUTransferBuffer(device, download);
    return result;
}

/** How sharp a picture is: the squared difference between neighbouring pixels, summed. A blur spreads each step over several pixels, which keeps the plain sum
 * but shrinks this one. */
static unsigned long long edges(const std::vector<unsigned char>& pixels)
{
    unsigned long long total = 0;

    for (Uint32 y = 0; y < screen_height; ++y)
    {
        for (Uint32 x = 1; x < screen_width; ++x)
        {
            const unsigned char* left = &pixels[(y * screen_width + x - 1) * bytes_per_pixel];
            const unsigned char* right = &pixels[(y * screen_width + x) * bytes_per_pixel];

            for (int c = 0; c < 3; ++c)
            {
                const int step = left[c] - right[c];
                total += static_cast<unsigned long long>(step * step);
            }
        }
    }

    return total;
}

/** How many pixels the menu changed from the background. */
static std::size_t changed(const std::vector<unsigned char>& before, const std::vector<unsigned char>& after)
{
    std::size_t count = 0;

    for (std::size_t i = 0; i < before.size(); i += bytes_per_pixel)
    {
        count += std::memcmp(&before[i], &after[i], 3) != 0;
    }

    return count;
}

/**
 * @brief Save a picture of the menu as a PPM.
 * @param path The file.
 * @param pixels The picture, RGBA.
 * @param screen Its size.
 */
static void save(const std::string& path, const std::vector<unsigned char>& pixels, const Screen& screen)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    CHECK(file);
    std::fprintf(file, "P6\n%u %u\n255\n", screen.width, screen.height);

    for (std::size_t i = 0; i < pixels.size(); i += bytes_per_pixel)
    {
        CHECK(std::fwrite(&pixels[i], 1, 3, file) == 3);
    }

    CHECK(std::fclose(file) == 0);
}

int main(int argc, char** argv)
{
    const char* prefix = argc > 1 ? argv[1] : nullptr;
    const Picture given = argc > 2 ? read_ppm(argv[2]) : Picture{};

    SDL_SetHintWithPriority(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT, "", SDL_HINT_OVERRIDE);
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL, false, nullptr);
    CHECK(device);
    std::printf("menu-render: %s backend\n", SDL_GetGPUDeviceDriver(device));
    SDL_Window* window = SDL_CreateWindow("menu-render", screen_width, screen_height, 0);
    CHECK(window);

    std::remove(preferences_name);
    lom_native_menu_enable(preferences_name);
    lom_native_menu_init(window, device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    CHECK(lom_native_menu_gpu_device() == device);

    const Screen texture = make_screen(device, screen_width, screen_height);
    const Picture picture = game_picture(given);
    Game game{upload_picture(device, picture), picture.width, picture.height, {}};
    const std::vector<unsigned char> pixels = frame(device, texture, game, false, false);

    CHECK(!lom_native_menu_open());
    CHECK(changed(pixels, frame(device, texture, game, false, true)) == 0);

    const Picture squares = game_picture(Picture{});
    Game pattern{upload_picture(device, squares), squares.width, squares.height, {}};
    const unsigned long long sharp = edges(frame(device, texture, pattern, false, false));
    const unsigned long long soft = edges(frame(device, texture, pattern, true, false));
    std::printf("menu-render: edges %llu sharp, %llu softened\n", sharp, soft);
    CHECK(soft * 2 < sharp);
    lom_present_release(&pattern.presentation, device);
    SDL_ReleaseGPUTexture(device, pattern.frame);

    struct Step
    {
        const char* name;
        std::vector<SDL_Scancode> keys;
    };

    lom_native_mod_toggles_add("skip-movies", "Skip movies", "Skips every movie, the opening, the ending and the ones in the middle of the story.");
    lom_native_mod_toggles_add("always-run", "Always run", "Run without holding the run button, and hold it to walk.");
    lom_native_mod_toggles_set(1, 0);
    const int option = lom_native_mod_options_add("always-run", "game_over", "After a game over", "What Continue does when the party falls.", 0);
    lom_native_mod_options_add_choice(option, "entrance", "Back to the entrance", "Continue takes you back to where you came in from the world map.");
    lom_native_mod_options_add_choice(option, "no_continue", "No Continue", "");

    const Step steps[] = {
        {"categories", {SDL_SCANCODE_F1}},
        {"display", {SDL_SCANCODE_RETURN}},
        {"audio", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN, SDL_SCANCODE_LEFT}},
        {"controls", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT}},
        {"mods", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN}},
        {"mod-option", {SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN}},
        {"quit", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN}},
    };

    for (const Step& step : steps)
    {
        for (SDL_Scancode code : step.keys)
        {
            key(code);
        }

        CHECK(lom_native_menu_open());
        const std::vector<unsigned char> drawn = frame(device, texture, game, true, true);
        const std::size_t count = changed(pixels, drawn);
        std::printf("menu-render: %s changed %zu pixels\n", step.name, count);
        CHECK(count > screen_width * screen_height / 2);

        if (prefix)
        {
            save(std::string(prefix) + "-" + step.name + ".ppm", drawn, texture);
        }

        if (std::strcmp(step.name, "audio") == 0)
        {
            lom_native_audio_set(0, false);
            const std::vector<unsigned char> silent = frame(device, texture, game, true, true);
            lom_native_audio_set(100, false);
            const std::vector<unsigned char> full = frame(device, texture, game, true, true);
            Uint32 bar_y = 0, bar_left = 0, bar_width = 0;

            for (Uint32 y = 0; y < screen_height; ++y)
            {
                for (Uint32 x = 0, run = 0; x < screen_width; ++x)
                {
                    const std::size_t i = (static_cast<std::size_t>(y) * screen_width + x) * bytes_per_pixel;
                    run = std::memcmp(&silent[i], &full[i], 3) != 0 ? run + 1 : 0;

                    if (run > bar_width)
                    {
                        bar_width = run;
                        bar_left = x + 1 - run;
                        bar_y = y;
                    }
                }
            }

            std::printf("menu-render: volume bar %u px wide at %u,%u\n", bar_width, bar_left, bar_y);
            CHECK(bar_width > 40);
            click(static_cast<float>(bar_left) + bar_width * 0.25f, static_cast<float>(bar_y));
            CHECK(std::abs(lom_native_audio_volume() - 25) <= 2);
            frame(device, texture, game, true, true);
            click(static_cast<float>(bar_left + bar_width) + 4.0f, static_cast<float>(bar_y));
            CHECK(lom_native_audio_volume() == 100);
            frame(device, texture, game, true, true);
            click(static_cast<float>(bar_left) - 4.0f, static_cast<float>(bar_y));
            CHECK(lom_native_audio_volume() == 0);
            lom_native_audio_set(100, false);
        }
    }

    key(SDL_SCANCODE_ESCAPE);
    CHECK(lom_native_menu_open());
    key(SDL_SCANCODE_ESCAPE);
    CHECK(!lom_native_menu_open());

    struct Pad
    {
        const char* name;
        Uint16 vendor;
        Uint16 product;
        SDL_GamepadType type;
    };

    const Pad pads[] = {
        {"playstation", 0x054C, 0x0CE6, SDL_GAMEPAD_TYPE_PS5},
        {"xbox", 0x045E, 0x0B12, SDL_GAMEPAD_TYPE_XBOXONE},
        {"switch", 0x057E, 0x2009, SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO},
    };
    key(SDL_SCANCODE_F1);
    key(SDL_SCANCODE_DOWN);
    key(SDL_SCANCODE_DOWN);

    for (const Pad& pad : pads)
    {
        const SDL_JoystickID id = attach_pad(pad.name, pad.vendor, pad.product);
        CHECK(SDL_GetGamepadTypeForID(id) == pad.type);
        pad_button(id, SDL_GAMEPAD_BUTTON_DPAD_UP);
        pad_button(id, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        pad_button(id, SDL_GAMEPAD_BUTTON_SOUTH);
        const std::vector<unsigned char> drawn = frame(device, texture, game, true, true);
        CHECK(changed(pixels, drawn) > screen_width * screen_height / 2);

        if (prefix)
        {
            save(std::string(prefix) + "-" + pad.name + ".ppm", drawn, texture);
        }

        pad_button(id, SDL_GAMEPAD_BUTTON_EAST);
        SDL_DetachVirtualJoystick(id);
    }

    key(SDL_SCANCODE_F1);
    CHECK(!lom_native_menu_open());

    struct Size
    {
        Uint32 width;
        Uint32 height;
    };

    const Size sizes[] = {{1280, 800}, {1920, 1080}, {3840, 2160}, {3440, 1440}, {1024, 768}, {800, 600}, {640, 480}};
    const Step views[] = {
        {"display", {SDL_SCANCODE_F1, SDL_SCANCODE_RETURN}},
        {"controls", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN, SDL_SCANCODE_DOWN}},
        {"mods", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN}},
        {"mod-option", {SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN}},
        {"quit", {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DOWN, SDL_SCANCODE_RETURN}},
    };

    for (const Step& view : views)
    {
        for (SDL_Scancode code : view.keys)
        {
            key(code);
        }

        CHECK(lom_native_menu_open());

        for (const Size& size : sizes)
        {
            const Screen screen = make_screen(device, size.width, size.height);
            const std::vector<unsigned char> drawn = frame(device, screen, game, true, true);
            const std::vector<unsigned char> plain = frame(device, screen, game, false, false);
            CHECK(changed(plain, drawn) > size.width * size.height / 2);

            if (prefix)
            {
                save(std::string(prefix) + "-" + view.name + "-" + std::to_string(size.width) + "x" + std::to_string(size.height) + ".ppm", drawn, screen);
            }

            SDL_ReleaseGPUTexture(device, screen.texture);
        }
    }

    key(SDL_SCANCODE_ESCAPE);
    key(SDL_SCANCODE_ESCAPE);
    CHECK(!lom_native_menu_open());

    lom_native_menu_destroy();
    lom_present_release(&game.presentation, device);
    SDL_ReleaseGPUTexture(device, game.frame);
    SDL_ReleaseGPUTexture(device, texture.texture);
    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();
    std::remove(preferences_name);
    std::puts("menu-render: the menu draws over the game in each state");
    return 0;
}

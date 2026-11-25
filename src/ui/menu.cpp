#include "menu.h"

#include "../settings/service.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "lom_native_display.h"
#include "menu_fonts.h"
#include "menu_runtime.h"
#include "menu_text.h"

#include <cstddef>
#include <cstring>

namespace lom::settings
{
using namespace menu_style;

namespace
{
constexpr char initialization_error_format[] = "Port menu initialization failed: %s";
constexpr char startup_log_format[] = "Port settings: F1 or controller Guide; %s";
}

void Menu::initialize(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format)
{
    if (!settings().enabled() || context_)
    {
        return;
    }

    window_ = window;
    device_ = device;
    menu_runtime().attach_window(SDL_GetWindowID(window));
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    platform_ready_ = ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo info;
    info.Device = device;
    info.ColorTargetFormat = format;
    gpu_ready_ = platform_ready_ && ImGui_ImplSDLGPU3_Init(&info);

    if (!gpu_ready_)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, initialization_error_format, SDL_GetError());
        shutdown();
        return;
    }

    lom::ui::set_menu_language(nullptr);

    struct Cjk
    {
        const char* language;
        const unsigned char* regular;
        std::size_t regular_size;
        const unsigned char* bold;
        std::size_t bold_size;
    };

    static const Cjk cjk_fonts[] = {
        {"ja", lom_font_notosanscjkjp_regular, lom_font_notosanscjkjp_regular_size, lom_font_notosanscjkjp_bold, lom_font_notosanscjkjp_bold_size},
        {"ko", lom_font_notosanscjkkr_regular, lom_font_notosanscjkkr_regular_size, lom_font_notosanscjkkr_bold, lom_font_notosanscjkkr_bold_size},
        {"zh_CN", lom_font_notosanscjksc_regular, lom_font_notosanscjksc_regular_size, lom_font_notosanscjksc_bold, lom_font_notosanscjksc_bold_size},
        {"zh_TW", lom_font_notosanscjktc_regular, lom_font_notosanscjktc_regular_size, lom_font_notosanscjktc_bold, lom_font_notosanscjktc_bold_size},
    };
    const Cjk* cjk = nullptr;

    for (const Cjk& candidate : cjk_fonts)
    {
        cjk = std::strcmp(candidate.language, lom::ui::menu_language()) == 0 ? &candidate : cjk;
    }

    const auto load = [&](const unsigned char* data, std::size_t size, bool bold)
    {
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        ImFont* font = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), static_cast<int>(size), font_load_size, &config);

        if (font && cjk)
        {
            ImFontConfig merge;
            merge.FontDataOwnedByAtlas = false;
            merge.MergeMode = true;
            io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(bold ? cjk->bold : cjk->regular),
                                           static_cast<int>(bold ? cjk->bold_size : cjk->regular_size), font_load_size, &merge);
        }

        return font;
    };
    body_font_ = load(lom_font_atkinsonhyperlegible_regular, lom_font_atkinsonhyperlegible_regular_size, false);
    bold_font_ = load(lom_font_atkinsonhyperlegible_bold, lom_font_atkinsonhyperlegible_bold_size, true);
    title_font_ = load(lom_font_alegreya_bold, lom_font_alegreya_bold_size, true);
    heading_font_ = load(lom_font_alegreya_mediumitalic, lom_font_alegreya_mediumitalic_size, false);

    const auto display = settings().values().display;
    lom_native_display_set(display.filter, display.resolution, display.fullscreen);
    SDL_Log(startup_log_format, settings().store().path().c_str());
}

void Menu::shutdown()
{
    if (context_)
    {
        lom::settings::settings().flush();

        if (device_)
        {
            SDL_WaitForGPUIdle(device_);
        }

        if (gpu_ready_)
        {
            ImGui_ImplSDLGPU3_Shutdown();
        }

        if (platform_ready_)
        {
            ImGui_ImplSDL3_Shutdown();
        }

        ImGui::DestroyContext(context_);
    }

    lom::settings::menu_runtime().detach_window();
    window_ = nullptr;
    device_ = nullptr;
    context_ = nullptr;
    platform_ready_ = false;
    gpu_ready_ = false;
    title_font_ = heading_font_ = body_font_ = bold_font_ = nullptr;
    held_ = Held{};
}

}

namespace
{
lom::settings::Menu menu;
}

extern "C" void lom_native_menu_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format)
{
    menu.initialize(window, device, format);
}

extern "C" SDL_GPUDevice* lom_native_menu_gpu_device()
{
    return menu.device();
}

extern "C" void lom_native_menu_destroy()
{
    menu.shutdown();
}

extern "C" bool lom_native_menu_event(const SDL_Event* event)
{
    return menu.handle_event(*event);
}

extern "C" void lom_native_menu_draw(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height)
{
    menu.draw(cmd, target, width, height);
}

#ifndef LOM_UI_MENU_H
#define LOM_UI_MENU_H

#include "imgui.h"
#include "menu_backend.h"
#include "menu_model.h"
#include "menu_style.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace lom::settings
{
struct Hit
{
    enum class Kind
    {
        Category,
        Row,
        Meter,
        KeepPlaying,
        Quit,
    };

    ImVec2 min;
    ImVec2 max;
    Kind kind;
    int index;
    float bar_left = 0.0f;
    float bar_right = 0.0f;
};

class Menu
{
  public:
    /**
     * @brief Make the menu, closed, with its model reading and changing the
     *        game's settings.
     */
    Menu() : model_(backend_)
    {
    }

    /**
     * @brief Initialize the menu for an enabled application.
     * @param window Borrowed game window, valid until shutdown.
     * @param device Borrowed GPU device, valid until shutdown.
     * @param format Render target format used for menu drawing.
     */
    void initialize(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format);

    /**
     * @brief Report the device initialize() was given.
     * @return Borrowed device, or null before initialize() and after
     *        shutdown().
     */
    SDL_GPUDevice* device() const
    {
        return device_;
    }

    /**
     * @brief Flush settings and release view resources before device
     *        destruction.
     */
    void shutdown();

    /**
     * @brief Apply the menu's rules to an event and pass the player's input to
     *        the model.
     * @param event Borrowed SDL event to process.
     * @return True if the menu consumed the event; false if the game may handle
     *        it.
     */
    bool handle_event(const SDL_Event& event);

    /**
     * @brief Draw the open menu over the current frame, or the notices when
     *        it's closed, with a quick room change's kept picture
     *        (lom_crossfade_overlay) under them.
     * @param cmd Command buffer recording the frame.
     * @param target Swapchain or offscreen texture to draw into.
     * @param width Target width in pixels.
     * @param height Target height in pixels.
     */
    void draw(SDL_GPUCommandBuffer* cmd, SDL_GPUTexture* target, Uint32 width, Uint32 height);

  private:
    struct Layout
    {
        float scale;
        ImVec2 size;
        float left;
        float right;
        float top;
        float bottom;
        float header_bottom;
        float footer_top;
        float categories_right;
        float rows_left;
    };

    struct Held
    {
        bool active = false;
        MenuInput input = MenuInput::Up;
        Uint64 next_ns = 0;
    };

    void translate(const SDL_Event& event);
    void move_stick(const SDL_GamepadAxisEvent& event);
    void press(MenuInput input, MenuDevice device);
    void hold(MenuInput input);
    void release_hold();
    void repeat_held();
    void click_at(float x, float y);
    void scroll_wheel(const SDL_MouseWheelEvent& event);
    SDL_GamepadType pad_type() const;
    void draw_face_button(SDL_GamepadButton button, ImVec2 center, float radius);
    void draw_dpad(ImVec2 center, float size, bool vertical);

    /**
     * @brief Scale a size in points to pixels for the window's size.
     * @param points The size.
     * @return It in pixels.
     */
    float px(float points) const
    {
        return points * layout_.scale;
    }

    /** A font size, kept readable when the window is small. */
    float text_px(float points) const
    {
        return std::max(px(points), std::min(points, menu_style::minimum_text));
    }

    ImVec2 text_size(ImFont* font, float size, const std::string& text, float wrap = 0.0f) const;
    void text(ImFont* font, float size, ImVec2 position, ImU32 color, const std::string& text, float wrap = 0.0f);

    void draw_backdrop();
    void draw_header();
    void draw_categories();
    void draw_rows();
    float row_height(const MenuRow& row, float left, float right, bool focused) const;
    float draw_row(const MenuRow& row, int index, float y, float left, float right, bool focused);
    void draw_child_guide(const MenuRow& row, float y, float height, float left, float row_left, bool more);
    void draw_value(const MenuRow& row, int index, ImVec2 center_right, bool focused);
    float draw_bindings(const std::vector<MenuRow>& rows, float y, float left, float right);
    void draw_footer();
    float draw_glyph(MenuHint::Control control, float x, float center_y);
    void draw_quit();
    std::vector<std::string> visible_notices(Uint64 now);
    void draw_notices(const std::vector<std::string>& lines);

    GameBackend backend_;
    MenuModel model_;
    SDL_Window* window_ = nullptr;
    SDL_GPUDevice* device_ = nullptr;
    ImGuiContext* context_ = nullptr;
    bool platform_ready_ = false;
    bool gpu_ready_ = false;
    ImFont* title_font_ = nullptr;
    ImFont* heading_font_ = nullptr;
    ImFont* body_font_ = nullptr;
    ImFont* bold_font_ = nullptr;
    ImDrawList* list_ = nullptr;
    Layout layout_{};
    std::vector<Hit> hits_;
    float rows_scroll_ = 0.0f;
    int scroll_category_ = -1;
    int scroll_focus_ = -1;
    Held held_;
    SDL_JoystickID last_pad_ = 0;
    MenuInput stick_x_ = MenuInput::Back;
    MenuInput stick_y_ = MenuInput::Back;
    std::map<unsigned, Uint64> notice_shown_;
};

}

#endif

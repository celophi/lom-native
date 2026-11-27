#ifndef LOM_UI_MENU_BACKEND_H
#define LOM_UI_MENU_BACKEND_H

#include "menu_model.h"

#include <string>

namespace lom::settings
{
class GameBackend final : public MenuBackend
{
  public:
    DisplaySettings display() const override;

    void set_display(const DisplaySettings& value) override;

    std::string display_error() const override;

    AudioLevel audio() const override;

    void set_audio(const AudioLevel& value) override;

    AudioLevel category_audio(int category) const override;

    void set_category_audio(int category, const AudioLevel& value) override;

    bool pause_unfocused() const override;

    void set_pause_unfocused(bool value) override;

    bool remaster_found() const override;

    bool remaster_switch_available(int which) const override;

    bool remaster_switch(int which) const override;

    void set_remaster_switch(int which, bool value) override;

    int text_language() const override;

    int mod_toggle_count() const override;

    std::string mod_toggle_id(int index) const override;

    std::string mod_toggle_name(int index) const override;

    std::string mod_toggle_description(int index) const override;

    bool mod_toggle_on(int index) const override;

    void set_mod_toggle(int index, bool value) override;

    int mod_option_count() const override;

    ModOptionView mod_option(int index) const override;

    void choose_mod_option(int index, int choice) override;

    bool game_seed(std::string& seed) const override;

    void set_text_language(int value) override;

    void resume() override;

    void request_quit() override;

    bool confirming_quit() const override;

    bool confirm_quit() override;

    void cancel_quit() override;

    std::string quit_error() const override;

    std::string binding_name(int port, int button, bool pad) const override;

    void begin_binding(int port, int button, bool pad) override;

    bool binding_active() const override;

    int binding_port() const override;

    int binding_button() const override;

    bool binding_pad() const override;

    void restore_bindings(int port) override;

    std::string binding_message() const override;

    std::string gamepad_name(int port) const override;
};

}

#endif

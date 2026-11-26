#ifndef LOM_NATIVE_QUICK_ROOMS_H
#define LOM_NATIVE_QUICK_ROOMS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Note that the player walked into a trigger region and its script
     *        started: if it changes scene before anything else happens, it's
     *        an ordinary room change.
     */
    void lom_quick_rooms_walked_in(void);

    /**
     * @brief Note something that makes a scene change more than a room
     *        change: a text window, or a script setting its own fade.
     */
    void lom_quick_rooms_not_a_room_change(void);

    /**
     * @brief Shorten the fade out of an ordinary room change, when the
     *        quick_room_changes switch is on. When the old room's last
     *        picture can be kept over the screen (lom_crossfade_hold), the
     *        game's fade is cut to a frame underneath it, and the picture
     *        fades straight into the new room.
     * @param frames The fade's length, as the game has it.
     * @return The length to use.
     */
    int lom_quick_rooms_exit_fade(int frames);

    /**
     * @brief Shorten the countdown that ends a quick room change, so the new
     *        scene loads as soon as the fade out reaches black, not a frame
     *        later.
     * @param frames The countdown, as the game has it.
     * @return The countdown to use.
     */
    int lom_quick_rooms_exit_countdown(int frames);

    /**
     * @brief Check whether a quick room change should leave the scene at
     *        once: when the old room's picture is kept over the screen, the
     *        game's own fade can't be seen, so there's nothing to wait for.
     * @return 1 when it should.
     */
    int lom_quick_rooms_leave_now(void);

    /**
     * @brief Note that the new scene has loaded, so the fade back in after a
     *        quick room change can be shortened too. When the old room's
     *        picture is kept over the screen, the new room is shown at full
     *        brightness at once and the picture starts dissolving into it.
     */
    void lom_quick_rooms_scene_loaded(void);

    /**
     * @brief Shorten the new scene's first fade back in after a quick room
     *        change, when it fades to the normal picture.
     * @param red The fade's target red.
     * @param green Its target green.
     * @param blue Its target blue.
     * @param frames Its length, as the scene's script has it.
     * @return The length to use.
     */
    int lom_quick_rooms_entry_fade(int red, int green, int blue, int frames);

#ifdef __cplusplus
}
#endif
#endif

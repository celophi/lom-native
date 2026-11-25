#ifndef LOM_SETTINGS_DISPLAY_SETTINGS_H
#define LOM_SETTINGS_DISPLAY_SETTINGS_H

namespace lom::settings
{

struct DisplaySettings
{
    int filter = 0;

    int resolution = 0;

    bool fullscreen = false;

    bool widescreen = false;

    bool operator==(const DisplaySettings& other) const
    {
        return filter == other.filter && resolution == other.resolution && fullscreen == other.fullscreen && widescreen == other.widescreen;
    }
};

}
#endif

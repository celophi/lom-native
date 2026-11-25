#include "lom_native_boundary.h"

static LomBoundaryObserver s_observer;

static const char* const s_names[LOM_BOUNDARY_COUNT] = {"files", "sound", "input", "saves", "overlays", "movies", "graphics"};

void lom_native_boundary_event(LomBoundary boundary, unsigned what, unsigned a, unsigned b)
{
    if (s_observer)
    {
        s_observer(boundary, what, a, b);
    }
}

void lom_native_boundary_set_observer(LomBoundaryObserver observer)
{
    s_observer = observer;
}

const char* lom_native_boundary_name(LomBoundary boundary)
{
    return (unsigned)boundary < LOM_BOUNDARY_COUNT ? s_names[boundary] : "unknown";
}

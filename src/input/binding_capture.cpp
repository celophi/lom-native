#include "binding_capture.h"

namespace lom::settings
{

bool BindingCapture::active() const
{
    return phase_ != Phase::Idle;
}

bool BindingCapture::waiting() const
{
    return phase_ == Phase::WaitingForRelease;
}

bool BindingCapture::listening() const
{
    return phase_ == Phase::Listening;
}

}

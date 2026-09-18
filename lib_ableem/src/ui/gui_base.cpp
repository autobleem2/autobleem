#include "ableem/ui/gui_base.h"

namespace ableem {

GuiBase::GuiBase(const std::string &windowTitle, int width, int height)
    // Platform and Renderer have private constructors reachable only via GuiBase/Platform friendship, hence
    // the `new` here instead of make_unique.
    : platform_(new Platform(windowTitle, width, height)),
      renderer_(new Renderer(*platform_)),
      input_(new Input(*platform_)) {
}

GuiBase::~GuiBase() {
    // members are destroyed in reverse declaration order: audio_, input_, renderer_, platform_ - which is
    // exactly the order that keeps every object valid while the ones built on top of it are still alive.
}

void GuiBase::releaseDisplay() {
    renderer_->release();
    platform_->releaseDisplay();
}

void GuiBase::acquireDisplay() {
    platform_->acquireDisplay();
    renderer_->recreate(*platform_);
    input_->reinstallEventFilter();
}

} // namespace ableem

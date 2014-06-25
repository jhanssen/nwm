#include "Commands.h"
#include "WindowManager.h"

Hash<String, std::function<void()> > Commands::sCmds;

static inline Layout::SharedPtr parentOfFocus()
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (!wm)
        return Layout::SharedPtr();
    Client::SharedPtr current = wm->focusedClient();
    if (!current)
        return Layout::SharedPtr();
    const Layout::SharedPtr& layout = current->layout();
    if (!layout)
        return Layout::SharedPtr();
    const Layout::SharedPtr& parent = layout->parent();
    if (!parent)
        return Layout::SharedPtr();
    return parent;
}

void Commands::initBuiltins()
{
    add("layout.toggleOrientation", []() {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            const Layout::Direction dir = parent->direction();
            switch (dir) {
            case Layout::LeftRight:
                parent->setDirection(Layout::TopBottom);
                break;
            case Layout::TopBottom:
                parent->setDirection(Layout::LeftRight);
                break;
            }
            parent->dump();
        });
    add("layout.adjustLeft", []() {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            parent->adjust(-10);
        });
    add("layout.adjustRight", []() {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            parent->adjust(10);
        });
}

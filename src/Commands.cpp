#include "Commands.h"
#include "WindowManager.h"
#include "Workspace.h"

Hash<String, std::function<void(const List<String>&)> > Commands::sCmds;

static inline Layout::SharedPtr parentOfFocus()
{
    WindowManager::SharedPtr wm = WindowManager::instance();
    if (!wm)
        return Layout::SharedPtr();
    Client::SharedPtr current = Workspace::active()->focusedClient();
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
    add("layout.toggleOrientation", [](const List<String>&) {
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
    add("layout.adjust", [](const List<String>& args) {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            const int adjust = args.isEmpty() ? 10 : args[0].toLong();
            parent->adjust(adjust);
        });
    add("layout.adjustLeft", [](const List<String>&) {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            parent->adjust(-10);
        });
    add("layout.adjustRight", [](const List<String>&) {
            Layout::SharedPtr parent = parentOfFocus();
            if (!parent)
                return;
            parent->adjust(10);
        });
    add("workspace.moveTo", [](const List<String>& args) {
            if (args.isEmpty())
                return;
            const int32_t ws = args[0].toLong();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return;
            Workspace::SharedPtr dst = wss[ws];
            Workspace::SharedPtr src = Workspace::active();
            if (dst == src)
                return;
            Client::SharedPtr client = src->focusedClient();
            dst->addClient(client);
        });
    add("workspace.select", [](const List<String>& args) {
            if (args.isEmpty())
                return;
            const int32_t ws = args[0].toLong();
            const List<Workspace::SharedPtr>& wss = WindowManager::instance()->workspaces();
            if (ws < 0 || ws >= wss.size())
                return;
            wss[ws]->activate();
        });
}

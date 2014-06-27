#ifndef LAYOUT_H
#define LAYOUT_H

#include <Rect.h>
#include <rct/SignalSlot.h>
#include <memory>
#include <utility>

class Layout : public std::enable_shared_from_this<Layout>
{
public:
    typedef std::shared_ptr<Layout> SharedPtr;
    typedef std::weak_ptr<Layout> WeakPtr;

    enum Direction { LeftRight, TopBottom };

    Layout(const Rect& rect);
    Layout(const Size& size);
    ~Layout();

    void setRect(const Rect& rect) { mRect = rect; relayout(); }

    SharedPtr add(const Size& size);
    SharedPtr parent() const { return mParent; }
    void dump();

    const Rect& rect() const { return mRect; }
    const Size& requestedSize() const { return mRequested; }
    void adjust(int delta);

    Direction direction() const { return mDirection; }
    void setDirection(const Direction& direction) { mDirection = direction; relayout(); }

    Signal<std::function<void(const Rect&)> >& rectChanged() { return mRectChanged; }

private:
    int children(SharedPtr& first, SharedPtr& second);
    void relayout();
    static void dumpHelper(const Layout::SharedPtr&, int indent);

    bool forEach(const std::function<bool(const SharedPtr& layout)>& func);
    SharedPtr clone() const;

private:
    Size mRequested;
    Rect mRect;
    SharedPtr mParent;
    bool mUsed;

    Direction mDirection;
    std::pair<WeakPtr, WeakPtr> mChildren;

    Signal<std::function<void(const Rect&)> > mRectChanged;
};

#endif

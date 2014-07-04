#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include <Rect.h>
#include <rct/SignalSlot.h>
#include <memory>
#include <utility>

class GridLayout : public std::enable_shared_from_this<GridLayout>
{
public:
    typedef std::shared_ptr<GridLayout> SharedPtr;
    typedef std::weak_ptr<GridLayout> WeakPtr;

    enum Direction { LeftRight, TopBottom };

    GridLayout(const Rect& rect);
    GridLayout(const Size& size);
    ~GridLayout();

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
    static void dumpHelper(const GridLayout::SharedPtr&, int indent);

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

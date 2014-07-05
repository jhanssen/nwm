#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include "Layout.h"

class GridLayout : public Layout
{
public:
    typedef std::shared_ptr<GridLayout> SharedPtr;
    typedef std::weak_ptr<GridLayout> WeakPtr;

    enum { Type = 0 };
    enum Direction { LeftRight, TopBottom };

    GridLayout(const Rect& rect);
    GridLayout(const Size& size);
    ~GridLayout();

    void setRect(const Rect& rect) { mRect = rect; relayout(); }

    virtual Layout::SharedPtr add(const Size& size);

    SharedPtr parent() const { return mParent; }
    virtual void dump();

    const Rect& rect() const { return mRect; }
    const Size& requestedSize() const { return mRequested; }
    void adjust(int delta);

    Direction direction() const { return mDirection; }
    void setDirection(const Direction& direction) { mDirection = direction; relayout(); }

    Signal<std::function<void(const Rect&)> >& rectChanged() { return mRectChanged; }

private:
    virtual void relayout();

    int children(SharedPtr& first, SharedPtr& second);
    static void dumpHelper(const GridLayout::SharedPtr&, int indent);

    bool forEach(const std::function<bool(const SharedPtr& layout)>& func);
    SharedPtr clone() const;

private:
    SharedPtr mParent;
    bool mUsed;

    Direction mDirection;
    std::pair<WeakPtr, WeakPtr> mChildren;
};

#endif

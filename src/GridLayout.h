#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include "Layout.h"

class GridLayout : public Layout
{
public:
    enum { Type = 0 };
    enum Direction { LeftRight, TopBottom };

    GridLayout(const Rect& rect);
    GridLayout(const Size& size);
    ~GridLayout();

    void setRect(const Rect& rect) { mRect = rect; relayout(); }

    virtual Layout *add(const Size& size);

    GridLayout *parent() const { return mParent; }
    virtual void dump();

    const Rect& rect() const { return mRect; }
    const Size& requestedSize() const { return mRequested; }
    void adjust(int delta);

    Direction direction() const { return mDirection; }
    void setDirection(const Direction& direction) { mDirection = direction; relayout(); }

    Signal<std::function<void(const Rect&)> >& rectChanged() { return mRectChanged; }

    void removeChild(GridLayout *child);
private:
    virtual void relayout();

    int children(GridLayout *&first, GridLayout *&second);
    static void dumpHelper(GridLayout *, int indent);

    bool forEach(const std::function<bool(GridLayout *layout)>& func);
    GridLayout *clone() const;

private:
    GridLayout *mParent;
    bool mUsed;

    Direction mDirection;
    std::pair<GridLayout*, GridLayout*> mChildren;
};

#endif

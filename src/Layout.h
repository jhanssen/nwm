#ifndef LAYOUT_H
#define LAYOUT_H

#include "Rect.h"
#include <rct/List.h>
#include <rct/SignalSlot.h>
#include <memory>

class Layout
{
public:
    Layout(unsigned int type, const Rect& rect);
    Layout(unsigned int type, const Size& size);
    virtual ~Layout() { }

    unsigned int type() const { return mType; }

    void setRect(const Rect& rect) { mRect = rect; relayout(); }

    virtual Layout *add(const Size& size) = 0;

    const Rect& rect() const { return mRect; }
    const Size& requestedSize() const { return mRequested; }

    Signal<std::function<void(const Rect&)> >& rectChanged() { return mRectChanged; }

    virtual void dump() { }

protected:
    virtual void relayout() = 0;

protected:
    unsigned int mType;
    Size mRequested;
    Rect mRect;

    Signal<std::function<void(const Rect&)> > mRectChanged;
};

inline Layout::Layout(unsigned int type, const Rect& rect)
    : mType(type)
{
    mRect = rect;
    mRequested = rect.size();
}

inline Layout::Layout(unsigned int type, const Size& size)
    : mType(type)
{
    mRect = Rect({ 0, 0, size.width, size.height });
    mRequested = size;
}

#endif

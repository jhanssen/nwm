#ifndef STACKLAYOUT_H
#define STACKLAYOUT_H

#include "Layout.h"

class StackLayout : public Layout
{
public:
    typedef std::shared_ptr<StackLayout> SharedPtr;
    typedef std::weak_ptr<StackLayout> WeakPtr;

    enum { Type = 1 };

    StackLayout(const Rect& rect);
    StackLayout(const Size& size);

    virtual Layout::SharedPtr add(const Size& size);

private:
    virtual void relayout();

private:
    List<WeakPtr> mChildren;
};

#endif

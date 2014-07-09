#ifndef STACKLAYOUT_H
#define STACKLAYOUT_H

#include "Layout.h"

class StackLayout : public Layout
{
public:
    enum { Type = 1 };

    StackLayout(const Rect& rect);
    StackLayout(const Size& size);
    ~StackLayout();

    virtual Layout *add(const Size& size);
    void removeChild(StackLayout *child);

private:
    virtual void relayout();

private:
    List<StackLayout*> mChildren;
};

#endif

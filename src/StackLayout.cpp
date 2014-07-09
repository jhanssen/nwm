#include "StackLayout.h"

StackLayout::StackLayout(const Rect& rect)
    : Layout(Type, rect)
{
}

StackLayout::StackLayout(const Size& size)
    : Layout(Type, size)
{
}

StackLayout::~StackLayout()
{
    mChildren.deleteAll();
}

Layout *StackLayout::add(const Size& size)
{
    StackLayout *sub = new StackLayout(mRect);
    sub->mRequested = size;
    mChildren.append(sub);
    return sub;
}

void StackLayout::relayout()
{
    for (StackLayout *child : mChildren) {
        assert(child->mChildren.isEmpty());
        child->mRect = mRect;
        child->mRectChanged(mRect);
    }
    mRectChanged(mRect);
}

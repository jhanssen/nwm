#include "StackLayout.h"

StackLayout::StackLayout(const Rect& rect)
    : Layout(Type, rect)
{
}

StackLayout::StackLayout(const Size& size)
    : Layout(Type, size)
{
}

Layout::SharedPtr StackLayout::add(const Size& size)
{
    SharedPtr sub = std::make_shared<StackLayout>(mRect);
    sub->mRequested = size;
    mChildren.append(sub);
    return sub;
}

void StackLayout::relayout()
{
    for (const WeakPtr& weak : mChildren) {
        if (SharedPtr child = weak.lock()) {
            assert(child->mChildren.isEmpty());
            child->mRect = mRect;
            child->mRectChanged(mRect);
        }
    }
    mRectChanged(mRect);
}

#include "Layout.h"
#include <rct/Log.h>
#include <assert.h>

Layout::Layout(const Rect& rect)
    : mRect(rect), mUsed(false)
{
    mRequested = { mRect.width, mRect.height };
    mDirection = (mRect.width > mRect.height) ? LeftRight : TopBottom;
}

Layout::Layout(const Size& size)
    : mRequested(size), mUsed(false)
{
    mRect = { 0, 0, size.width, size.height };
    mDirection = (mRect.width > mRect.height) ? LeftRight : TopBottom;
}

Layout::~Layout()
{
    if (mParent) {
        if (EventLoop::SharedPtr loop = EventLoop::eventLoop()) {
            WeakPtr parent = mParent;
            loop->callLater([parent]() {
                    if (SharedPtr p = parent.lock()) {
                        p->relayout();
                    }
                });
        }
    }
}

int Layout::children(SharedPtr& first, SharedPtr& second)
{
    SharedPtr* ptrs[2] = { &first, &second };
    int n = 0;
    if (SharedPtr ptr = mChildren.first.lock())
        *ptrs[n++] = ptr;
    if (SharedPtr ptr = mChildren.second.lock())
        *ptrs[n++] = ptr;
    return n;
}

void Layout::forEach(const std::function<bool(const SharedPtr& layout)>& func)
{
    if (!func(shared_from_this()))
        return;
    SharedPtr first, second;
    children(first, second);
    if (first) {
        first->forEach(func);
        if (second)
            second->forEach(func);
    }
}

static inline Size calcSize(const Layout::SharedPtr& c1, const Layout::SharedPtr& c2, const Layout::SharedPtr& p)
{
    const unsigned int w2 = (c2 ? c2->requestedSize().width : 0);
    const unsigned int h2 = (c2 ? c2->requestedSize().height : 0);

    if (p->direction() == Layout::TopBottom)
        return Size({ p->rect().width, std::max(c1->requestedSize().height, h2) });
    return Size({ std::max(c1->requestedSize().width, w2), p->rect().height });
}

Layout::SharedPtr Layout::add(const Size& size)
{
    // find either a child with sufficient amount of remaining space or the one
    // with the highest ratio of unused vs. used space
    float curRatio = 1.;
    unsigned int curGeom = 0;
    Layout::SharedPtr curLayout;
    forEach([&curRatio, &curGeom, &curLayout, size](const SharedPtr& layout) -> bool {
            SharedPtr c1, c2;
            const int num = layout->children(c1, c2);
            if (num <= 1) {
                // do we fit in the remaining space?
                Size sz = layout->rect().size();
                if (c1) {
                    // subtract
                    sz.width -= c1->requestedSize().width;
                    sz.height -= c1->requestedSize().height;
                }
                if (sz.width >= size.width && sz.height >= size.height) {
                    // yes, let's use this one
                    curLayout = layout;
                    return false;
                }
            } else {
                assert(num == 2);
                const Size c1Size = c1->requestedSize();
                const Size c2Size = c2->requestedSize();
                const Size pSize = layout->rect().size();

                const unsigned int reqWidth = c1Size.width + c2Size.width;
                const unsigned int reqHeight = c1Size.height + c2Size.height;
                const float ratio = (reqWidth * reqHeight) / static_cast<float>(pSize.width * pSize.height);
                const unsigned int geom = (pSize.width * pSize.height) / static_cast<float>(reqWidth * reqHeight);
                if (ratio <= curRatio && geom > curGeom) {
                    curRatio = ratio;
                    curGeom = geom;
                    curLayout = layout;
                }
            }
            return true;
        });

    assert(curLayout);
    SharedPtr c1, c2;
    curLayout->children(c1, c2);
    SharedPtr created = std::make_shared<Layout>(size);
    created->mUsed = true;
    if (c1) {
        // let's make a new child for each child

        if (c2) {
            // make yet another child for c1 and c2, put us as the second of the current
            SharedPtr ncp = std::make_shared<Layout>(calcSize(c1, c2, curLayout));
            ncp->mParent = curLayout;

            SharedPtr nc1 = std::make_shared<Layout>(c1->requestedSize());
            nc1->mChildren.first = c1;
            nc1->mParent = ncp;
            c1->mParent = nc1;
            ncp->mChildren.first = nc1;

            SharedPtr nc2 = std::make_shared<Layout>(c2->requestedSize());
            nc2->mChildren.first = c2;
            nc2->mParent = ncp;
            c2->mParent = nc2;
            ncp->mChildren.second = nc2;

            curLayout->mChildren.first = ncp;
            curLayout->mChildren.second = created;
        } else {
            curLayout->mChildren.second = created;
        }
    } else {
        assert(!c2);
        if (curLayout->mUsed) {
            // split and move down
            SharedPtr ncp = std::make_shared<Layout>(curLayout->requestedSize());
            SharedPtr parent = curLayout->mParent;
            if (parent) {
                if (parent->mChildren.first.lock() == curLayout)
                    parent->mChildren.first = ncp;
                else if (parent->mChildren.second.lock() == curLayout)
                    parent->mChildren.second = ncp;
            }
            ncp->mParent = parent;
            curLayout->mParent = ncp;
            ncp->mChildren.first = curLayout;
            created->mParent = ncp;
            ncp->mChildren.second = created;
            if (parent) {
                ncp->mRequested = calcSize(parent->mChildren.first.lock(), parent->mChildren.second.lock(), parent);
                ncp->mDirection = (ncp->mRequested.width > ncp->mRequested.height) ? LeftRight : TopBottom;
                parent->relayout();
            } else {
                ncp->relayout();
            }
            return created;
        } else {
            curLayout->mChildren.first = created;
        }
    }
    created->mParent = curLayout;
    curLayout->relayout();
    return created;
}

Layout::SharedPtr Layout::clone() const
{
    return std::make_shared<Layout>(mRequested);
}

void Layout::relayout()
{
    mRectChanged(mRect);

    // layout all children in parent direction
    SharedPtr first, second;
    const int num = children(first, second);
    if (!num)
        return;
    if (num == 1) {
        // make ourselves as big as our parent
        assert(first && !second);
        first->mRect = mRect;
        first->relayout();
        return;
    }
    assert(num == 2);
    const Size& firstSize = first->requestedSize();
    const Size& secondSize = second->requestedSize();
    switch (mDirection) {
    case LeftRight: {
        const unsigned int mid = mRect.width / 2;
        if (firstSize.width <= mid && secondSize.width <= mid) {
            // we're good
            first->mRect = { mRect.x, mRect.y, mid, mRect.height };
            first->relayout();
            second->mRect = { mRect.x + mid, mRect.y, mid, mRect.height };
            second->relayout();
        } else {
            const unsigned int total = firstSize.width + secondSize.width;
            if (total <= mRect.width) {
                const float firstRatio = static_cast<float>(firstSize.width) / mRect.width;
                const float secondRatio = static_cast<float>(secondSize.width) / mRect.width;
                first->mRect = { mRect.x, mRect.y, static_cast<unsigned int>(mRect.width * firstRatio), mRect.height };
                first->relayout();
                second->mRect = { static_cast<unsigned int>(mRect.x + (mRect.width * firstRatio)), mRect.y,
                                  static_cast<unsigned int>(mRect.width * secondRatio), mRect.height };
                second->relayout();
            } else {
                // something's gotta give, can we accomodate one without screwing over the other?
                const float firstRatio = static_cast<float>(firstSize.width) / mRect.width;
                const float secondRatio = static_cast<float>(secondSize.width) / mRect.width;
                bool done = false;
                if (firstRatio > secondRatio) {
                    // see if we can give second the size it wants
                    const int remaining = mRect.width - secondSize.width;
                    if (remaining > 0) {
                        const float ratio = static_cast<float>(remaining) / firstSize.width;
                        if (ratio > 0.7) {
                            // let's go with that
                            first->mRect = { mRect.x, mRect.y, static_cast<const unsigned int>(remaining), mRect.height };
                            first->relayout();
                            second->mRect = { mRect.x + remaining, mRect.y, secondSize.width, mRect.height };
                            second->relayout();
                            done = true;
                        }
                    }
                } else {
                    // see if we can give first the size it wants
                    const int remaining = mRect.width - firstSize.width;
                    if (remaining > 0) {
                        const float ratio = static_cast<float>(remaining) / secondSize.width;
                        if (ratio > 0.7) {
                            // let's go with that
                            first->mRect = { mRect.x, mRect.y, firstSize.width, mRect.height };
                            first->relayout();
                            second->mRect = { mRect.x + remaining, mRect.y, static_cast<const unsigned int>(remaining), mRect.height };
                            second->relayout();
                            done = true;
                        }
                    }
                }
                if (!done) {
                    // fall back to a pure ratio calculation
                    const float firstRatio = static_cast<float>(firstSize.width) / mRect.width;
                    const float secondRatio = static_cast<float>(secondSize.width) / mRect.width;
                    first->mRect = { mRect.x, mRect.y, static_cast<unsigned int>(mRect.width * firstRatio), mRect.height };
                    first->relayout();
                    second->mRect = { static_cast<unsigned int>(mRect.x + (mRect.width * firstRatio)), mRect.y,
                                      static_cast<unsigned int>(mRect.width * secondRatio), mRect.height };
                    second->relayout();
                }
            }
        }
        break; }
    case TopBottom: {
        const unsigned int mid = mRect.height / 2;
        if (firstSize.height <= mid && secondSize.height <= mid) {
            // we're good
            first->mRect = { mRect.x, mRect.y, mRect.width, mid };
            first->relayout();
            second->mRect = { mRect.x, mRect.y + mid, mRect.width, mid };
            second->relayout();
        } else {
            const unsigned int total = firstSize.height + secondSize.height;
            if (total <= mRect.height) {
                const float firstRatio = static_cast<float>(firstSize.height) / mRect.height;
                const float secondRatio = static_cast<float>(secondSize.height) / mRect.height;
                first->mRect = { mRect.x, mRect.y, mRect.width, static_cast<unsigned int>(mRect.height * firstRatio) };
                first->relayout();
                second->mRect = { mRect.x, static_cast<unsigned int>(mRect.y + (mRect.height * firstRatio)),
                                  mRect.width, static_cast<unsigned int>(mRect.height * secondRatio) };
                second->relayout();
            } else {
                // something's gotta give, can we accomodate one without screwing over the other?
                const float firstRatio = static_cast<float>(firstSize.height) / mRect.height;
                const float secondRatio = static_cast<float>(secondSize.height) / mRect.height;
                bool done = false;
                if (firstRatio > secondRatio) {
                    // see if we can give second the size it wants
                    const int remaining = mRect.height - secondSize.height;
                    if (remaining > 0) {
                        const float ratio = static_cast<float>(remaining) / firstSize.height;
                        if (ratio > 0.7) {
                            // let's go with that
                            first->mRect = { mRect.x, mRect.y, mRect.width, static_cast<const unsigned int>(remaining) };
                            first->relayout();
                            second->mRect = { mRect.x, mRect.y + remaining, mRect.height, secondSize.height };
                            second->relayout();
                            done = true;
                        }
                    }
                } else {
                    // see if we can give first the size it wants
                    const int remaining = mRect.height - firstSize.height;
                    if (remaining > 0) {
                        const float ratio = static_cast<float>(remaining) / secondSize.height;
                        if (ratio > 0.7) {
                            // let's go with that
                            first->mRect = { mRect.x, mRect.y, mRect.width, firstSize.height };
                            first->relayout();
                            second->mRect = { mRect.x, mRect.y + remaining, mRect.width, static_cast<const unsigned int>(remaining) };
                            second->relayout();
                            done = true;
                        }
                    }
                }
                if (!done) {
                    // fall back to a pure ratio calculation
                    const float firstRatio = static_cast<float>(firstSize.height) / mRect.height;
                    const float secondRatio = static_cast<float>(secondSize.height) / mRect.height;
                    first->mRect = { mRect.x, mRect.y, mRect.width, static_cast<unsigned int>(mRect.height * firstRatio) };
                    first->relayout();
                    second->mRect = { mRect.x, static_cast<unsigned int>(mRect.y + (mRect.height * firstRatio)),
                                      mRect.width, static_cast<unsigned int>(mRect.height * secondRatio) };
                    second->relayout();
                }
            }
        }
        break; }
    };
}

void Layout::adjust(int delta)
{
    // adjust child layout in parent direction
    SharedPtr first, second;
    const int num = children(first, second);
    if (num <= 1)
        return;
    assert(num == 2);
    switch (mDirection) {
    case LeftRight: {
        first->mRect = { mRect.x, mRect.y,
                         static_cast<const unsigned int>(std::min<int>(std::max<int>(1, first->mRect.width + delta), mRect.width - 1)),
                         mRect.height };
        second->mRect = { mRect.x + first->mRect.width, mRect.y, mRect.width - first->mRect.width, mRect.height };
        break; }
    case TopBottom: {
        first->mRect = { mRect.x, mRect.y, mRect.width,
                         static_cast<const unsigned int>(std::min<int>(std::max<int>(1, first->mRect.height + delta), mRect.height - 1)) };
        second->mRect = { mRect.x, mRect.y + first->mRect.height, mRect.width, mRect.height - first->mRect.height };
        break; }
    }
    first->relayout();
    second->relayout();
}

void Layout::dumpHelper(const SharedPtr& layout, int indent)
{
    char buf[1024];
    const Rect& r = layout->rect();
    const Size& req = layout->requestedSize();
    SharedPtr c1, c2;
    layout->children(c1, c2);
    const float ratio = static_cast<float>(req.width * req.height) / (r.width * r.height);
    snprintf(buf, sizeof(buf), "%*s%u,%u+%u,%u, req %ux%u, ratio %f", indent, " ",
             r.x, r.y, r.width, r.height, req.width, req.height, ratio);
    error() << buf;
    if (c1) {
        dumpHelper(c1, indent + 2);
        if (c2)
            dumpHelper(c2, indent + 2);
    }
}

void Layout::dump()
{
    int indent = 0;
    dumpHelper(shared_from_this(), indent);
}

#include "GridLayout.h"
#include <rct/Log.h>
#include <assert.h>

GridLayout::GridLayout(const Rect& rect)
    : Layout(Type, rect), mUsed(false)
{
    mDirection = (mRect.width > mRect.height) ? LeftRight : TopBottom;
}

GridLayout::GridLayout(const Size& size)
    : Layout(Type, size), mUsed(false)
{
    mDirection = (mRect.width > mRect.height) ? LeftRight : TopBottom;
}

GridLayout::~GridLayout()
{
    if (mParent) {
        mParent->removeChild(this);
    }
}

int GridLayout::children(GridLayout *& first, GridLayout *&second)
{
    first = mChildren.first;
    second = mChildren.second;
    return (first ? 0 : 1) + (second ? 0 : 1);
}

bool GridLayout::forEach(const std::function<bool(GridLayout *layout)>& func)
{
    if (!func(this))
        return false;
    GridLayout *first = 0;
    GridLayout *second = 0;
    children(first, second);
    if (first) {
        if (!first->forEach(func))
            return false;
        if (second) {
            if (!second->forEach(func))
                return false;
        }
    }
    return true;
}

static inline Size calcSize(GridLayout *c1, GridLayout *c2, GridLayout *p)
{
    const unsigned int w2 = (c2 ? c2->requestedSize().width : 0);
    const unsigned int h2 = (c2 ? c2->requestedSize().height : 0);

    if (p->direction() == GridLayout::TopBottom)
        return Size({ p->rect().width, std::max(c1->requestedSize().height, h2) });
    return Size({ std::max(c1->requestedSize().width, w2), p->rect().height });
}

Layout *GridLayout::add(const Size& size)
{
    // find either a child with sufficient amount of remaining space or the one
    // with the highest ratio of unused vs. used space
    float curRatio = 1.;
    unsigned int curGeom = 0;
    GridLayout *curGridLayout = 0;
    forEach([&curRatio, &curGeom, &curGridLayout, size](GridLayout *layout) -> bool {
            error() << "looking at" << layout->rect();
            GridLayout *c1 = 0;
            GridLayout *c2 = 0;
            const int num = layout->children(c1, c2);
            if (num <= 1) {
                // do we fit in the remaining space?
                Size sz = layout->rect().size();
                error() << "considering 1" << sz;
                if (layout->mUsed) {
                    assert(!num);
                    error() << "  removing req" << layout->mRequested;
                    sz.width -= layout->mRequested.width;
                    sz.height -= layout->mRequested.height;
                }
                if (c1) {
                    assert(!layout->mUsed);
                    // subtract
                    error() << "  removing c1" << c1->requestedSize();
                    sz.width -= c1->requestedSize().width;
                    sz.height -= c1->requestedSize().height;
                }
                error() << "  considered 1" << sz << size;
                if (sz.width >= size.width && sz.height >= size.height) {
                    curGridLayout = layout;
                    error() << "    taken";
                    // yes, let's use this one
                    return false;
                } else {
                    unsigned int reqGeom;
                    const Size pSize = layout->rect().size();
                    if (num == 1) {
                        const Size c1Size = c1->requestedSize();
                        reqGeom = (c1Size.width * c1Size.height);
                    } else {
                        assert(!num);
                        reqGeom = 1; // avoid division by zero
                    }
                    const float ratio = reqGeom / static_cast<float>(pSize.width * pSize.height);
                    const unsigned int geom = (pSize.width * pSize.height) / static_cast<float>(reqGeom);
                    if (ratio <= curRatio && geom > curGeom) {
                        error() << "    candidate 1";
                        curRatio = ratio;
                        curGeom = geom;
                        curGridLayout = layout;
                    }
                }
            } else {
                assert(num == 2);
                const Size c1Size = c1->requestedSize();
                const Size c2Size = c2->requestedSize();
                const Size pSize = layout->rect().size();

                const unsigned int reqGeom = (c1Size.width * c1Size.height) + (c2Size.width * c2Size.height);
                const float ratio = reqGeom / static_cast<float>(pSize.width * pSize.height);
                const unsigned int geom = (pSize.width * pSize.height) / static_cast<float>(reqGeom);
                error() << "considering 2" << c1Size << c2Size << pSize << "req" << reqGeom << ratio << geom << "cur" << curRatio << curGeom;
                if (ratio <= curRatio && geom > curGeom) {
                    error() << "candidate 2";
                    curRatio = ratio;
                    curGeom = geom;
                    curGridLayout = layout;
                }
            }
            return true;
        });

    assert(curGridLayout);
    GridLayout *c1 = 0;
    GridLayout *c2 = 0;
    curGridLayout->children(c1, c2);
    GridLayout *created = new GridLayout(size);
    created->mUsed = true;
    if (c1) {
        // let's make a new child for each child

        if (c2) {
            // make yet another child for c1 and c2, put us as the second of the current
            GridLayout *ncp = new GridLayout(calcSize(c1, c2, curGridLayout));
            ncp->mParent = curGridLayout;

            GridLayout *nc1 = new GridLayout(c1->requestedSize());
            nc1->mChildren.first = c1;
            nc1->mParent = ncp;
            c1->mParent = nc1;
            ncp->mChildren.first = nc1;

            GridLayout *nc2 = new GridLayout(c2->requestedSize());
            nc2->mChildren.first = c2;
            nc2->mParent = ncp;
            c2->mParent = nc2;
            ncp->mChildren.second = nc2;

            curGridLayout->mChildren.first = ncp;
            curGridLayout->mChildren.second = created;
        } else {
            curGridLayout->mChildren.second = created;
        }
    } else {
        assert(!c2);
        if (curGridLayout->mUsed) {
            // split and move down
            GridLayout *ncp = new GridLayout(curGridLayout->requestedSize());
            GridLayout *parent = curGridLayout->mParent;
            if (parent) {
                if (parent->mChildren.first == curGridLayout)
                    parent->mChildren.first = ncp;
                else if (parent->mChildren.second == curGridLayout)
                    parent->mChildren.second = ncp;
            }
            ncp->mParent = parent;
            curGridLayout->mParent = ncp;
            ncp->mChildren.first = curGridLayout;
            created->mParent = ncp;
            ncp->mChildren.second = created;
            if (parent) {
                ncp->mRequested = calcSize(parent->mChildren.first, parent->mChildren.second, parent);
                ncp->mDirection = (ncp->mRequested.width > ncp->mRequested.height) ? LeftRight : TopBottom;
                parent->relayout();
            } else {
                ncp->relayout();
            }
            return created;
        } else {
            curGridLayout->mChildren.first = created;
        }
    }
    created->mParent = curGridLayout;
    curGridLayout->relayout();
    return created;
}

GridLayout* GridLayout::clone() const
{
    return new GridLayout(mRequested);
}

void GridLayout::relayout()
{
    mRectChanged(mRect);

    // layout all children in parent direction
    GridLayout *first = 0;
    GridLayout *second = 0;
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
                            second->mRect = { mRect.x + firstSize.width, mRect.y, static_cast<const unsigned int>(remaining), mRect.height };
                            second->relayout();
                            done = true;
                        }
                    }
                }
                if (!done) {
                    // fall back to a pure ratio calculation
                    const float firstRatio = (static_cast<float>(firstSize.width) / mRect.width) / 2.;
                    const float secondRatio = (static_cast<float>(secondSize.width) / mRect.width) / 2.;
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
                            second->mRect = { mRect.x, mRect.y + firstSize.height, mRect.width, static_cast<const unsigned int>(remaining) };
                            second->relayout();
                            done = true;
                        }
                    }
                }
                if (!done) {
                    // fall back to a pure ratio calculation
                    const float firstRatio = (static_cast<float>(firstSize.height) / mRect.height) / 2.;
                    const float secondRatio = (static_cast<float>(secondSize.height) / mRect.height) / 2.;
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

void GridLayout::adjust(int delta)
{
    // adjust child layout in parent direction
    GridLayout *first = 0;
    GridLayout *second = 0;
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

void GridLayout::dumpHelper(GridLayout *layout, int indent)
{
    char buf[1024];
    const Rect& r = layout->rect();
    const Size& req = layout->requestedSize();
    GridLayout *c1, *c2;
    c1 = c2 = 0;
    layout->children(c1, c2);
    const float ratio = static_cast<float>(req.width * req.height) / (r.width * r.height);
    snprintf(buf, sizeof(buf), "%*s%u,%u+%u,%u, req %ux%u, ratio %f dir %s %s", indent, " ",
             r.x, r.y, r.width, r.height, req.width, req.height, ratio,
             (layout->mDirection == LeftRight ? "left-right" : "top-bottom"), (layout->mUsed ? "USED" : ""));
    error() << buf;
    if (c1) {
        dumpHelper(c1, indent + 2);
        if (c2)
            dumpHelper(c2, indent + 2);
    }
}

void GridLayout::dump()
{
    int indent = 0;
    dumpHelper(this, indent);
}

void GridLayout::removeChild(GridLayout *child)
{
    if (child == mChildren.first) {
        mChildren.first = 0;
    } else {
        assert(child == mChildren.second);
        mChildren.second = 0;
    }
    relayout();
}

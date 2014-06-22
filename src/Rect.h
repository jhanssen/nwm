#ifndef RECT_H
#define RECT_H

#include <rct/Log.h>
#include <rct/String.h>

class Size
{
public:
    unsigned int width, height;
};

class Point
{
public:
    unsigned int x, y;
};

class Rect
{
public:
    unsigned int x, y;
    unsigned int width, height;

    Point point() const { return Point({ x, y }); }
    Size size() const { return Size({ width, height }); }
};

inline Log operator<<(Log stream, const Size& size)
{
    stream << "Size(";
    const bool old = stream.setSpacing(false);
    stream << String::format<32>("%ux%u", size.width, size.height) << ")";
    stream.setSpacing(old);
    return stream;
}

inline Log operator<<(Log stream, const Point& point)
{
    stream << "Point(";
    const bool old = stream.setSpacing(false);
    stream << String::format<32>("%u,%u", point.x, point.y) << ")";
    stream.setSpacing(old);
    return stream;
}

inline Log operator<<(Log stream, const Rect& rect)
{
    stream << "Rect(";
    const bool old = stream.setSpacing(false);
    stream << String::format<64>("%u,%u+%ux%u", rect.x, rect.y, rect.width, rect.height) << ")";
    stream.setSpacing(old);
    return stream;
}

#endif

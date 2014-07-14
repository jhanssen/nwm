#ifndef RECT_H
#define RECT_H

#include <rct/Log.h>
#include <rct/String.h>

class Size
{
public:
    Size(int w = 0, int h = 0)
        : width(w), height(h)
    {}
    int width, height;
};

class Point
{
public:
    Point(int xx = 0, int yy = 0)
        : x(xx), y(yy)
    {}
    int x, y;
};

class Rect
{
public:
    Rect(int xx = 0, int yy = 0, int w = 0, int h = 0)
        : x(xx), y(yy), width(w), height(h)
    {}

    int x, y;
    int width, height;

    Point point() const { return Point({ x, y }); }
    Size size() const { return Size({ width, height }); }
    bool isEmpty() const { return !width || !height; }
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

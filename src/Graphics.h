#ifndef GRAPHICS_H
#define GRAPHICS_H

#include "nwm-config.h"
#include "Rect.h"
#include <rct/String.h>
#include <memory>

#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
#include <cairo.h>
#include <pango/pangocairo.h>
#endif

class Client;

class Font
{
public:
    Font() : mSize(0) { }
    Font(const String& font, int size) : mFont(font), mSize(size) { }

    void setFamily(const String& font) { mFont = font; }
    void setPointSize(int size) { mSize = size; }

    String family() const { return mFont; }
    int pointSize() const { return mSize; }

private:
    String mFont;
    int mSize;
};

class Color
{
public:
    Color() : r(0), g(0), b(0), a(255) { }

    uint8_t r, g, b, a;
};

class Graphics
{
public:
    Graphics(Client *client);
    ~Graphics();

    void redraw();

    void setBackgroundColor(const Color& color) { mBackgroundColor = color; }

    void setText(const Rect& rect, const Font& font, const Color& color, const String& string);
    static Size fontMetrics(const Font &font, const String &string, int width = -1);
    void clearText();

private:
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    Size mSize;
    cairo_t* mCairo;
    cairo_surface_t* mSurface;
    PangoLayout* mTextLayout;
    Font mFont;
    Color mBackgroundColor;
    Color mTextColor;
    Rect mTextRect;
#endif
};

inline Log operator<<(Log stream, const Color& color)
{
    stream << "Color(";
    const bool old = stream.setSpacing(false);
    stream << String::format<32>("r:%u, g:%u, b:%u, a:%u", color.r, color.g, color.b, color.a) << ")";
    stream.setSpacing(old);
    return stream;
}

inline Log operator<<(Log stream, const Font& font)
{
    stream << "Font(";
    const bool old = stream.setSpacing(false);
    stream << String::format<32>("%s-%d", font.family().constData(), font.pointSize()) << ")";
    stream.setSpacing(old);
    return stream;
}
#endif

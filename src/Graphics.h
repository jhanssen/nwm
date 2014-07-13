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

class Graphics
{
public:
    Graphics(const std::shared_ptr<Client>& client);
    ~Graphics();

    void redraw();
    void setText(const Rect& rect, const Font& font, const String& string);
    void clearText();

private:
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    cairo_t* mCairo;
    cairo_surface_t* mSurface;
    PangoLayout* mTextLayout;
    Font mFont;
    Rect mTextRect;
#endif
};

#endif

#include "Graphics.h"
#include "Client.h"
#include "WindowManager.h"
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
#include <cairo-xcb.h>
#endif

Graphics::Graphics(const Client::SharedPtr& client)
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    : mCairo(0), mSurface(0), mTextLayout(0)
#endif
{
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    WindowManager* wm = WindowManager::instance();
    const Size& sz = client->size();
    mSurface = cairo_xcb_surface_create(wm->connection(), client->window(), client->visual(), sz.width, sz.height);
    mCairo = cairo_create(mSurface);
#endif
}

Graphics::~Graphics()
{
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    if (mTextLayout)
        g_object_unref(mTextLayout);
    if (mSurface)
        cairo_surface_destroy(mSurface);
    if (mCairo)
        cairo_destroy(mCairo);
#endif
}

void Graphics::redraw()
{
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    if (!mCairo || !mTextLayout)
        return;
    cairo_set_source_rgba(mCairo, mTextColor.r / 255., mTextColor.g / 255., mTextColor.b / 255., mTextColor.a / 255.);
    cairo_translate(mCairo, mTextRect.x, mTextRect.y);
    pango_cairo_update_layout(mCairo, mTextLayout);
    pango_cairo_show_layout(mCairo, mTextLayout);
    cairo_translate(mCairo, -mTextRect.x, -mTextRect.y);
    WindowManager* wm = WindowManager::instance();
    xcb_flush(wm->connection());
#endif
}

void Graphics::setText(const Rect& rect, const Font& font, const Color& color, const String& string)
{
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    assert(mCairo && mSurface);
    mFont = font;
    mTextRect = rect;
    mTextColor = color;
    if (mTextLayout)
        g_object_unref(mTextLayout);
    mTextLayout = pango_cairo_create_layout(mCairo);

    PangoFontDescription* desc = pango_font_description_new();
    pango_font_description_set_family(desc, font.family().constData());
    pango_font_description_set_size(desc, font.pointSize());
    pango_layout_set_font_description(mTextLayout, desc);
    pango_font_description_free(desc);

    pango_layout_set_text(mTextLayout, string.constData(), string.size());
    pango_layout_set_wrap(mTextLayout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_width(mTextLayout, rect.width);
    pango_layout_set_height(mTextLayout, rect.height);
#endif
}

void Graphics::clearText()
{
#if defined(HAVE_CAIRO) && defined(HAVE_PANGO)
    mFont = Font();
    mTextRect = Rect();
    if (mTextLayout) {
        g_object_unref(mTextLayout);
        mTextLayout = 0;
    }
#endif
}

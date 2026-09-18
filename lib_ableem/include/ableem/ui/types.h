#pragma once
// Basic value types shared by every ableem header. No SDL type appears in any public header.

// ABLEEM_API: only meaningful when the library is built as a Windows DLL (ABLEEM_SHARED). On every other
// platform/configuration it expands to nothing.
#if defined(_WIN32) && defined(ABLEEM_SHARED)
#ifdef ABLEEM_BUILDING
#define ABLEEM_API __declspec(dllexport)
#else
#define ABLEEM_API __declspec(dllimport)
#endif
#else
#define ABLEEM_API
#endif

namespace ableem {

//******************
// Color
//******************
struct Color {
    unsigned char r = 255, g = 255, b = 255, a = 255;
    Color() = default;
    Color(unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a = 255) : r(_r), g(_g), b(_b), a(_a) {}
};

//******************
// Point
//******************
struct Point {
    int x = 0, y = 0;
};

//******************
// Size
//******************
struct Size {
    int w = 0, h = 0;
};

//******************
// Rect
//******************
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h) {}
};

//******************
// Align
//******************
enum class Align { Left, Center, Right };

//******************
// BlendMode
//******************
enum class BlendMode { None, Blend, Add, Mod };

} // namespace ableem

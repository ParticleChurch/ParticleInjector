#pragma once
#include <string>

template <class T>
T numericLerp(T a, T b, double f)
{
    if (f < 0.f) f = 0.f;
    else if (f > 1.f) f = 1.f;

    if (a > b)
        return numericLerp<T>(b, a, 1.f - f);
    return (T)((b - a) * f) + a;
}

class Color
{
public:
    unsigned char r, g, b;
    Color(unsigned char r, unsigned char g, unsigned char b);

    Color lerp(const Color& other, float factor);
    std::string toString();
};

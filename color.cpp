#include "color.hpp"

Color::Color(unsigned char r, unsigned char g, unsigned char b)
{
    this->r = r;
    this->g = g;
    this->b = b;
}

Color Color::lerp(const Color& other, float factor)
{
    return Color(
        numericLerp(this->r, other.r, factor),
        numericLerp(this->g, other.g, factor),
        numericLerp(this->b, other.b, factor)
    );
}

std::string Color::toString()
{
    return "(" + std::to_string(this->r) + ", " + std::to_string(this->g) + ", " + std::to_string(this->b) + ")";
}

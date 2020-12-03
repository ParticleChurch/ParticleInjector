#pragma once

#include <chrono>
#include <map>
#include <cmath>

#define TIME_POINT std::chrono::steady_clock::time_point

namespace Animation
{
    enum class Interpolation
    {
        easeInOutQuint = 0,
        linear,
    };

    struct Anim
    {
        int state = 0;
        TIME_POINT changed;

        Anim(int state, TIME_POINT changed)
        {
            this->state = state;
            this->changed = changed;
        }
    };

    extern std::map<const char*, Anim*> trackedAnimations;

    extern TIME_POINT now();
    extern double delta(TIME_POINT after, TIME_POINT before); // after > before
    extern Anim* get(const char* name);
    extern double age(Anim* anim);
    extern double age(const char* name);
    extern void newAnimation(const char* name, int state = 0);
    extern void deleteAnimation(const char* name);
    extern bool changed(const char* name, int newState);
    extern float animate(double timePassed, double animationTime, Interpolation i = Interpolation::easeInOutQuint);
}

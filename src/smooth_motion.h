#pragma once
#include <algorithm>
#include <cmath>

namespace uiMotion {
inline bool advance(float& position, float& velocity, float target, float elapsed, float duration) {
    // Solve the damped spring directly so motion stays consistent across frame rates.
    float timeStep = std::max(0.f, std::min(.1f, elapsed));
    float angularFrequency = 5.f / duration;
    float offset = position - target;
    float initialSlope = velocity + angularFrequency * offset;
    float decay = std::exp(-angularFrequency * timeStep);
    position = target + (offset + initialSlope * timeStep) * decay;
    velocity = (velocity - angularFrequency * initialSlope * timeStep) * decay;
    if (position < 0 || position > 1) {
        position = std::max(0.f, std::min(1.f, position));
        velocity = 0;
    }
    if (std::abs(position - target) < .001f && std::abs(velocity) < .01f) {
        position = target;
        velocity = 0;
        return false;
    }
    return true;
}

struct Toggle {
    float position = 0, velocity = 0, target = 0;
    bool initialized = false;

    void set(bool on, bool animate) {
        target = on ? 1.f : 0.f;
        if (!initialized || !animate) {
            position = target;
            velocity = 0;
            initialized = true;
        }
    }

    bool moving() const {
        return position != target || velocity != 0;
    }

    bool tick(float elapsed) {
        return advance(position, velocity, target, elapsed, .24f);
    }
};

inline bool test() {
    Toggle toggle;
    toggle.set(false, true);
    toggle.set(true, true);
    if (toggle.position != 0 || !toggle.moving())
        return false;
    for (int i = 0; i < 6; ++i)
        toggle.tick(1 / 60.f);
    float middle = toggle.position;
    if (middle <= 0 || middle >= 1)
        return false;
    toggle.set(false, true);
    if (toggle.position != middle)
        return false;
    for (int i = 0; i < 180; ++i)
        toggle.tick(1 / 60.f);
    if (toggle.moving() || toggle.position != 0)
        return false;
    toggle.set(true, false);
    if (toggle.moving() || toggle.position != 1)
        return false;
    float position60Hz = 0, velocity60Hz = 0, position120Hz = 0, velocity120Hz = 0;
    for (int i = 0; i < 6; ++i)
        advance(position60Hz, velocity60Hz, 1, 1 / 60.f, .24f);
    for (int i = 0; i < 12; ++i)
        advance(position120Hz, velocity120Hz, 1, 1 / 120.f, .24f);
    if (std::abs(position60Hz - position120Hz) > .0001f || position60Hz <= 0 || position60Hz >= 1)
        return false;
    float previousPosition = position60Hz;
    advance(position60Hz, velocity60Hz, 0, 0, .24f);
    if (position60Hz != previousPosition)
        return false;
    bool active = true;
    for (int i = 0; i < 180; ++i) {
        active = advance(position60Hz, velocity60Hz, 0, 1 / 60.f, .24f);
        if (position60Hz < 0 || position60Hz > 1 || !std::isfinite(position60Hz) ||
            !std::isfinite(velocity60Hz))
            return false;
    }
    return !active && position60Hz == 0 && velocity60Hz == 0;
}
}

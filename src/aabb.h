#pragma once

#include <stdint.h>

namespace engine::utils {

struct AABB {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;

    // Assuming that w and h are even (for perf)
    constexpr int16_t center_x() const { return x + w / 2; }
    constexpr int16_t center_y() const { return y + h / 2; }

    constexpr bool intersects(const AABB & o) const {
        // x axis.
        int16_t xa = o.x - (x + w); // -ve iff this.right > o.left
        int16_t xb = x - (o.x + o.w); // -ve iff this.left > o.right
        bool xc = (xa ^ xb) >= 0; // -ve iff not overlapping
        // y axis.
        int16_t ya = o.y - (y + h);
        int16_t yb = y - (o.y + o.h);
        bool yc = (ya ^ yb) >= 0;
        return xc & yc;
    }
};

namespace test {

constexpr int check_aabb() {
    // Self intersection.
    {
        AABB a{0, 0, 1, 1};
        if (!a.intersects(a)) return 0;
    }
    // No intersection.
    {
        AABB a{0, 0, 1, 2};
        AABB b{2, 0, 1, 2};
        if (a.intersects(b)) return 1;
        if (b.intersects(a)) return 1;
    }
    {
        AABB a{0, 0, 1, 2};
        AABB b{0, 3, 1, 2};
        if (a.intersects(b)) return 1;
        if (b.intersects(a)) return 1;
    }
    {
        AABB a{0, 0, 1, 2};
        AABB b{2, 3, 1, 2};
        if (a.intersects(b)) return 1;
        if (b.intersects(a)) return 1;
    }
    // Intersection.
    {
        AABB a{0, 0, 5, 10};
        AABB b{4, 0, 5, 10};
        if (!a.intersects(b)) return 2;
        if (!b.intersects(a)) return 2;
    }
    {
        AABB a{0, 0, 5, 10};
        AABB b{0, 9, 5, 10};
        if (!a.intersects(b)) return 2;
        if (!b.intersects(a)) return 2;
    }
    // Edges.
    {
        AABB a{0, 0, 1, 2};
        AABB b{1, 0, 1, 2};
        if (a.intersects(b)) return 3;
        if (b.intersects(a)) return 3;
    }
    {
        AABB a{0, 0, 1, 2};
        AABB b{0, 2, 1, 2};
        if (a.intersects(b)) return 3;
        if (b.intersects(a)) return 3;
    }
    return -1;
}
static_assert(check_aabb() == -1);

} // namespace test

} // namespace engine::utils

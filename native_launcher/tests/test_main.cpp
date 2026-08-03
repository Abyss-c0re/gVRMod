#include "../../tests/test_framework.h"
#include "math3d.hpp"

TEST(launcher_math3d_normalize) {
    Vec3 n = Normalize(V3(3.f, 0.f, 0.f));
    ASSERT_NEAR(n.x, 1.f, 1e-5f);
    ASSERT_NEAR(n.y, 0.f, 1e-5f);
    ASSERT_NEAR(n.z, 0.f, 1e-5f);
}

TEST(launcher_math3d_dot) {
    ASSERT_NEAR(Dot(V3(1, 0, 0), V3(0, 1, 0)), 0.f, 1e-6f);
    ASSERT_NEAR(Dot(V3(2, 0, 0), V3(3, 0, 0)), 6.f, 1e-6f);
}

TEST(launcher_math3d_cross) {
    Vec3 c = Cross(V3(1, 0, 0), V3(0, 1, 0));
    ASSERT_NEAR(c.z, 1.f, 1e-5f);
}

TEST(launcher_desktop_cycle_1_to_4) {
    auto cycle = [](int v, int dir) {
        return 1 + ((v - 1 + dir + 4) % 4);
    };
    ASSERT_EQ(cycle(3, 1), 4); // right → follow
    ASSERT_EQ(cycle(4, 1), 1);
}

int main() {
    return RunAllTests();
}

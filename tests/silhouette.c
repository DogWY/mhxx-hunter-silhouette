#include "../src/silhouette.h"
#include <assert.h>

int main(void) {
    // Column 2..6 is the hunter, rows 1..5. A monster crosses the lower
    // three rows. In the last frame the camera has cleared the monster.
    for (int frame = 0; frame < 5; ++frame) {
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 9; ++x) {
                bool covered = x >= 2 && x <= 6 && y >= 1 && y <= 5;
                bool monster = frame == 2 || (frame == 1 && y >= 3);
                SilhouettePixel p = {
                    .hunter = covered,
                    .left = x > 2, .right = x < 6,
                    .above = y > 1, .below = y < 5,
                    .front_is_large_monster = monster,
                    .front_depth = monster || frame == 3 ? 2 : 4,
                    .hunter_depth = 4,
                };
                bool border = covered && (x == 2 || x == 6 || y == 1 || y == 5);
                assert(silhouette_edge(p, .001f) == (border && monster));
                p.hunter_depth = 1;
                assert(!silhouette_edge(p, .001f));
            }
        }
    }
}

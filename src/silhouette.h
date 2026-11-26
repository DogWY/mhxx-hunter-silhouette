// Inputs share the active viewport. Coverage is the union of the local
// hunter's parts before scene occlusion. Depth increases away from camera.
#ifndef MHGU_SILHOUETTE_H
#define MHGU_SILHOUETTE_H

#include <stdbool.h>

typedef struct {
    bool hunter;
    bool left, right, above, below;
    bool front_is_large_monster;
    float front_depth;
    float hunter_depth;
} SilhouettePixel;

static inline bool silhouette_edge(SilhouettePixel pixel, float depth_bias) {
    return pixel.hunter && pixel.front_is_large_monster &&
           pixel.hunter_depth > pixel.front_depth + depth_bias &&
           !(pixel.left && pixel.right && pixel.above && pixel.below);
}
#endif

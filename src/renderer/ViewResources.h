#pragma once

#include <vector>

#include "core/Types.h"

// Per-view render targets (see the resource-ownership diagram). The handles
// are re-acquired from the RenderTargetPool every frame; targets that are not
// needed this frame (SSAO/Bloom while disabled) stay invalid.
struct ViewResources
{
    FBOHandle fboMainMS;     // multisampled forward target
    FBOHandle fboResolved;   // single-sample resolve destination
    FBOHandle fboSSAO;       // (wired, disabled)
    FBOHandle fboSSAOBlur;   // (wired, disabled)
    FBOHandle fboLit;        // (wired for future HDR)
    std::vector<FBOHandle> fboBloom;  // (wired, disabled)

    int width  = 0;
    int height = 0;
};

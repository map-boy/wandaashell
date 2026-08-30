#pragma once
//
// Helpers shared between the Tier 2 mobile backends. Not part of
// platform/platform.h: nothing in core/ or builtins/ may call these, because
// they have no desktop counterpart.
//
#include <string>

namespace platform {

// Absolute path of the directory holding the app's bundled assets. Empty
// until the bridge sets it, which both bridges do at startup:
//   iOS     - [[NSBundle mainBundle] resourcePath]
//   Android - the directory the app extracted its assets into on first launch
// Only the app knows this path, so it is pushed in rather than discovered.
std::string getBundleAssetDir();
void setBundleAssetDir(const std::string& dir);

} // namespace platform

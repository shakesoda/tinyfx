# TinyFX
Small OpenGL ES2.0+ renderer inspired by [BGFX](https://github.com/bkaradzic/bgfx). Currently master does not work on GLES2, use `gles2` branch instead. This will be fixed in the future.

## Features
- Reorders draw calls to minimize state changes and avoid overdraw (in progress)
- Deals with the dirty details of the graphics API for you
- Bring-your-own-framework style renderer. Doesn't tell you how to architect your program
- Tracks and resets state for you between draws
- Out-of-order submission to views (passes)
- Uniforms separate from shader objects, all shader programs with matching uniforms are updated automatically
- Supports compute shaders (in progress)
- OpenGL ES 3.1+ (ES2 supported in `gles2` branch)
- OpenGL 4.3+ core (as low as 3.1 should work, but isn't regularly tested)
<!-- - Supports stereo rendering for VR -->

## FAQ
### *Who is this for?*
Anyone sick of remembering when you need barriers, implementing a state tracker for the 50th time, integrating bigger deps than your entire codebase or who just wants something less of a pain to use than OpenGL is.

### *Why not use BGFX?*
BGFX is excellent, but we have different priorities and scope.

## Using TinyFX
1. Include `tinyfx.c` in your build
2. Add the location of `tinyfx.h` (and `tinyfx.hpp` if you use the C++ API) to your include paths
3. `#include <tinyfx.h>` or `#include <tinyfx.hpp>` and start using tfx after creating an OpenGL context (GLFW and SDL are good for this!). Remember to call `tfx_set_platform_data` with your target GL version first!

## Examples

Hello Triangle (`examples/01-triangle.c`)
![](https://github.com/shakesoda/tinyfx/raw/master/examples/01-triangle.png)

<!-- Hello C++ (`examples/hello_cpp.cpp) -->
<!-- Transient buffers -->
<!-- Compute -->
<!-- Shadows -->
<!-- ImGui -->
<!-- Skeletal animation? -->

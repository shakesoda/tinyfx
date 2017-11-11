# TinyFX
Small OpenGL ES2+ renderer inspired by [BGFX](https://github.com/bkaradzic/bgfx).

## Features
- Reorders draw calls to minimize state changes and avoid overdraw (in progress)
- Deals with the dirty details of the graphics API for you
- Bring-your-own-framework style renderer. Doesn't tell you how to architect your program
- Tracks and resets state for you between draws
- Out-of-order submission to views (passes)
- Uniforms separate from shader objects, all shader programs with matching uniforms are updated automatically
- Supports compute shaders (in progress)
- OpenGL ES 2.0+
- OpenGL 4.3+ core (as low as 3.1 should work, but is unsupported)
<!-- - Supports stereo rendering for VR -->

## FAQ
### *Who is this for?*
Anyone sick of remembering when you need barriers, implementing a state tracker for the 50th time, integrating bigger deps than your entire codebase or who just wants something less of a pain to use than OpenGL is.

### *What's wrong with BGFX?*
BGFX is great if you need to support multiple backends (I don't) and you care to deal with keeping its dependencies synced. I wanted something (much) smaller which builds quickly on RPi, has a reasonable feature set and doesn't require that you use additional tooling which is a pain for prototyping.

BGFX is faster, more mature and more featureful if you need it. It's also entirely possible to start with TinyFX and switch to BGFX later, as the APIs are (intentionally) very similar.

### *Is this some kind of triangle generator?*
why do I keep getting asked this

## Using TinyFX
1. Include `tinyfx.c` in your build
2. Add the location of `tinyfx.h` (and `tinyfx.hpp` if you use the C++ API) to your include paths
3. Link libGLESv2 (or libepoxy, then define `TFX_USE_EPOXY`)
4. Define a target GL version, if not ES2 (e.g. `TFX_USE_GL=43` for OpenGL 4.3, or `TFX_USE_GLES=31`)
5. `#include <tinyfx.h>` or `#include <tinyfx.hpp>` and start using tfx after creating an OpenGL context (GLFW and SDL are good for this!)

## Examples

Hello Triangle (`tests/tinyfx_test.c`)
![](https://github.com/shakesoda/tinyfx/raw/master/test/triangle.png)

<!-- Hello C++ (`tests/hello_cpp.cpp) -->
<!-- Transient buffers -->
<!-- Compute -->
<!-- Shadows -->
<!-- ImGui -->
<!-- Skeletal animation? -->

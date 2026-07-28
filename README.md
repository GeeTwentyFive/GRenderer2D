Minimalistic simple cross-platform 2D renderer


# Usage

1) Create native window with OpenGL context (e.g. with [GWindower_OpenGL](https://github.com/GeeTwentyFive/GWindower_OpenGL) or GLFW)
2) `GRenderer2D gr2d{WINDOW_WIDTH, WINDOW_HEIGHT};`
3) ...create sprites, instantiate created sprites...
4) `gr2d.DrawFrame();` (+ do a `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)` each frame (lib doesn't do that so that you can compose it with other rendering))


# API

- `.CreateSprite()` - Write sprite to GPU
- `.AddSprite()` - Create instance of sprite created with `.CreateSprite()`
- ##### AT END OF FRAME: `.DrawFrame()`  (returns 0 on success, source line number on error)

#

#### SpriteInstance (created with `.AddSprite()`):
- `.size` - Set/Get size
- `.z_depth` - Set/Get drawing order/depth (for multiple sprites on top of one-another)
- `.position` - Set/Get position
- `.color_RGBA` - Set/Get color shift (in RGBA; 0xRRGGBBAA)
- `.Remove()` - Remove from existence.

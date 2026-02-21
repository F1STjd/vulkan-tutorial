// Compile STB implementation in its own translation unit so third-party
// warnings don't pollute the main build.  This file is compiled with -w.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

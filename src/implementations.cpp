// Compile STB implementation in its own translation unit so third-party
// warnings don't pollute the main build.  This file is compiled with -w.

// #define TINYOBJLOADER_IMPLEMENTATION
// #include <tiny_obj_loader.h>

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

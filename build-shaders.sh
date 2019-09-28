#!/bin/sh

if [ ! -d build ]; then mkdir -p build; fi
echo compile psmain.glsl...
glslc -fshader-stage=frag ./src/shaders/psmain.glsl -o ./build/psmain.spv
echo compile vsmain.glsl...
glslc -fshader-stage=vert ./src/shaders/vsmain.glsl -o ./build/vsmain.spv

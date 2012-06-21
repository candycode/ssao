#!/bin/sh
SHADER_PATH=$HOME/projects/ssao/src/shaders
# set shading to spherical harmonics
SHADING=ao
VSHADER=$SHADER_PATH/ssao_trace_per_frag2_optimal.vert
FSHADER=$SHADER_PATH/ssao_trace_per_frag2_optimal.frag
# numSamples is actually the number of sampling directions
./ssao -vert $VSHADER -manip -frag $FSHADER -mrt -shade $SHADING -dRadius .06 -maxRadius 32 -stepMul 1.0 -maxNumSamples 16 -occFact 1 $1 $2 

#!/bin/sh
SHADER_PATH=$HOME/projects/ssao/src/shaders
./ssao -s -vert $SHADER_PATH/ssao_simple.vert -frag $SHADER_PATH/ssao_simple.frag -hw 9 -step 4 -occFact 7 $1 --options $2

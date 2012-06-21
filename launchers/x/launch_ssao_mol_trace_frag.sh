#!/bin/sh
# numSamples is actually the number of sampling directions
# use e.g. gearbox41_Drexler.pdb
./ssao -viewportUniform osg_Viewport -ssaoRadiusUniform ssaoRadius -dRadius .06 -maxRadius 32 -stepMul 1.0 -maxNumSamples 16 -occFact 1 --options "-method RayTracedPoint -ssao trace_frag" $1

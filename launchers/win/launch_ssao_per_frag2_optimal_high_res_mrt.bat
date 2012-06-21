SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
REM numSamples is actually the number of sampling directions
C:\cmakebuilds\ssao\release\ssao.exe -vert %SHADER_PATH%\ssao_trace_per_frag2_optimal.vert -frag %SHADER_PATH%\ssao_trace_per_frag2_optimal.frag -mrt -dRadius .06 -maxRadius 32 -stepMul 1.0 -maxNumSamples 64 -occFact 1 %1 --options %2

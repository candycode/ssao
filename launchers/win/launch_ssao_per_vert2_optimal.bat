SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
REM numSamples is actually the number of sampling directions
C:\cmakebuilds\ssao\release\ssao.exe -vert %SHADER_PATH%\ssao_trace_per_vert2_optimal.vert -frag %SHADER_PATH%\ssao_trace_per_vert2_optimal.frag -shade ao -dRadius .5 -maxRadius 128 -mrt -stepMul 4.0 -maxNumSamples 16 %1 --options %2

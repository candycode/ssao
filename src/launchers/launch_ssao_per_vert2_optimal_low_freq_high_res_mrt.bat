REM compfiguration for low fewquency case i.e. high step and large radius
SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
REM numSamples is actually the number of sampling directions
C:\cmakebuilds\ssao\release\ssao.exe -vert %SHADER_PATH%\ssao_trace_per_vert2_optimal.vert -frag %SHADER_PATH%\ssao_trace_per_vert2_optimal.frag -dRadius .5 -maxRadius 256 -mrt -stepMul 16.0 -maxNumSamples 72 %1 --options %2

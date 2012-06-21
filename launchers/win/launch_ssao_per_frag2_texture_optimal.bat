SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
REM set shading to spherical harmonics
SET SHADING=ao_lambert
SET VSHADER=%SHADER_PATH%\ssao_trace_per_frag2_optimal.vert
SET FSHADER=%SHADER_PATH%\ssao_trace_per_frag2_optimal.frag
REM numSamples is actually the number of sampling directions
C:\cmakebuilds\ssao\release\ssao.exe -vert %VSHADER% -frag %FSHADER% -textures -shade %SHADING% -dRadius .06 -maxRadius 32 -stepMul 1.0 -maxNumSamples 16 -occFact 1 %1 --options %2

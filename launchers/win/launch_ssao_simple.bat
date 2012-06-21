SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
C:\cmakebuilds\ssao\release\ssao.exe -s -vert %SHADER_PATH%\ssao_simple.vert -frag %SHADER_PATH%\ssao_simple.frag -hw 9 -step 4 -occFact 7 %1 --options %2

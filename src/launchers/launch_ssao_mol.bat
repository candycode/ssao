SET SHADER_PATH=C:\projects\ssao\trunk\src\shaders
C:\cmakebuilds\ssao\release\ssao.exe -s -vert %SHADER_PATH%\ssao_in_vertex_shader.vert -frag %SHADER_PATH%\ssao_in_vertex_shader.frag -hw 9 -step 4 -viewportUniform osg_Viewport -occFact 7 %1 --options %2

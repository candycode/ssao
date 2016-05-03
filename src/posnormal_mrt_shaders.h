static const char POSNORMALS_VERT_MRT[] =
"varying vec3 worldNormal;\n"
"varying vec4 worldPosition;\n"
"void main(void)\n"
"{\n"
"  worldPosition = gl_ModelViewMatrix * gl_Vertex;\n"
"  worldNormal   = gl_NormalMatrix * gl_Normal;\n"
"  //worldNormal   = faceforward( -worldNormal, vec3( 0., 0., 1.0 ), worldNormal );\n"
"  gl_Position = ftransform();\n"
"}\n";

static const char POSNORMALS_FRAG_MRT[] =
"varying vec3 worldNormal;\n"
"varying vec4 worldPosition;\n"
"void main(void)\n"
"{\n"
"  gl_FragData[0] = worldPosition;\n"
"  gl_FragData[1] = vec4( normalize( worldNormal ), 1.0 );\n"
"}\n";

static const char POSNORMALSDEPTH_FRAG_MRT[] =
"varying vec3 worldNormal;\n"
"varying vec4 worldPosition;\n"
"void main(void)\n"
"{\n"
"  gl_FragData[0].xyz = worldPosition.xyz;\n"
"  gl_FragData[1].xyz = normalize( worldNormal );\n"
"  gl_FragData[1].w   = gl_FragCoord.z;\n"
"}\n";

//#define TEXTURE_ENABLED

#extension GL_ARB_texture_rectangle : enable

#ifdef TEXTURE_ENABLED
uniform sampler2D tex;
uniform int textureUnit; // textureUnit < 0 => no texture
#endif

#ifdef MRT_ENABLED
uniform sampler2DRect normals;
uniform sampler2DRect positions;
vec3 normal;
#else
varying vec3 normal;
varying vec3 worldPosition;
#endif
varying vec4 color;
varying float visibility;
void main(void)
{  
#ifdef MRT_ENABLED
 #if defined( AO_FLAT )
   gl_FragColor = color * visibility;
 #elif defined( AO_LAMBERT )
   normal = texture2DRect( normals, gl_FragCoord.xy ).xyz;
   gl_FragColor.rgb = gl_FrontMaterial.diffuse.rgb * dot( normal,  -normalize( texture2DRect( positions, gl_FragCoord.xy ).xyz ) );
   gl_FragColor.rgb *= visibility;
   gl_FragColor.rgb += 0.05 * gl_FrontMaterial.ambient.rgb;
   gl_FragColor.a = gl_FrontMaterial.diffuse.a;
 #else
   gl_FragColor = vec4( visibility, visibility, visibility, 1.0 );
 #endif
#else
 #if defined( AO_FLAT )
   gl_FragColor = color * visibility;
 #elif defined( AO_LAMBERT )
   gl_FragColor.rgb = gl_FrontMaterial.diffuse.rgb * dot( normal,  -normalize( worldPosition.xyz ) );
   gl_FragColor.rgb *= visibility;
   gl_FragColor.rgb += gl_FrontMaterial.ambient.rgb;
   gl_FragColor.a = gl_FrontMaterial.diffuse.a;
 #else
   gl_FragColor = vec4( visibility, visibility, visibility, 1.0 );
 #endif
#endif
#ifdef TEXTURE_ENABLED
  if( textureUnit >= 0 ) gl_FragColor *= texture2D( tex, gl_TexCoord[ textureUnit ].st );
#endif
}


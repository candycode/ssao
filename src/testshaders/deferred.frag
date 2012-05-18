#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;

void main(void)
{
  float j = gl_TexCoord[ 0 ].s;
  float i = gl_TexCoord[ 0 ].t;
  float d = texture2DRect( depthMap, vec2( j, i ) );
  float zx1 = texture2DRect( depthMap, vec2( j - 1., i ) );
  float zx2 = texture2DRect( depthMap, vec2( j + 1., i ) );
  float zy1 = texture2DRect( depthMap, vec2( j, i - 1. ) );
  float zy2 = texture2DRect( depthMap, vec2( j,  i + 1. ) );
  float dzx = zx2 - zx1;
  float dzy = zy2 - zy1;
  if( abs( dzx ) < 0.000001 || abs( dzy ) < 0.000001 ) discard;
  dzx = clamp( dzx, -.01, .01 );
  dzy = clamp( dzy, -.01, .01 );
  vec3 n = normalize( cross( vec3( 2. / 800., 0., dzx ), vec3( 0., 2./600., dzy ) ) );
  gl_FragColor = vec4( n, 1. );
}


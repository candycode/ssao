// computes average kernel width and step multiplier from passed radius

#define TEXTURE_ENABLED
#define USE_OBJECT_RADIUS
//#define MRT_ENABLED

// IN: numSamples, radius, dhwidth (%radius), maxSteps, ssao, depthMap
// OUT: occlusion modified gl_Color, normal, position
#extension GL_ARB_texture_rectangle : enable
#ifdef MRT_ENABLED
uniform sampler2DRect positions;
#endif

#ifdef TEXTURE_ENABLED
uniform int textureUnit; // textureUnit < 0 => no texture
#endif

uniform float radius; // object or scene radius 
uniform float dhwidth; // percentage of radius used as width of convolution kernel (0.1)

uniform int ssao; // enable/disable ssao

varying vec4 color;
#ifndef MRT_ENABLED // in case of multiple render targets normals are available in 'normals' texture
                    // and world space coordinates in 'positions' texture
varying vec3 normal;
varying vec3 worldPosition;
#else
vec3 worldPosition;
#endif

varying float pixelRadius;
varying float R;

uniform vec2 viewport;

float width = viewport.x; 
float height = viewport.y;

//------------------------------------------------------------------------------
vec3 screenSpace( vec3 v )
{
   vec4 p = gl_ProjectionMatrix *  vec4( v, 1.0 );
   p.xyz /= p.w;
   // clamp x,y to [-1,1]
   //clamp( p.xy, -1.0, 1.0 );
   p.xyz *= 0.5;
   p.xyz += 0.5;
   p.x *= width;
   p.y *= height;
   return p.xyz;   
}

//-----------------------------------------------------------------------------
vec3 ssUnproject( vec3 v )
{
#ifdef MRT_ENABLED
  return texture2DRect( positions, v.xy ).xyz;
#else
  vec4 p = vec4( v, 1.0 );
  p.x /= width;
  p.y /= height;
  p.xyz -= 0.5;
  p.xyz *= 2.0;
  p = gl_ProjectionMatrixInverse * p;
  p.xyz /= p.w;
  return p.xyz;
#endif
}

//------------------------------------------------------------------------------
float projectAtPos( vec3 pos, float r )
{
   return distance( screenSpace( pos ), screenSpace( pos + vec3( r, 0., 0 ) ) );
   //return viewport.y < viewport.x ? distance( screenSpace( pos ), screenSpace( pos + vec3( r, 0., 0 ) ) )
   //                               : distance( screenSpace( pos ), screenSpace( pos + vec3( 0., r, 0 ) ) );     
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
  vec4 v = gl_ModelViewMatrix * gl_Vertex;
  worldPosition = v.xyz;
  worldPosition /= v.w;
  if( bool( ssao ) )
  {	
    R = dhwidth * radius;
#ifdef USE_OBJECT_RADIUS    
    pixelRadius = max( 0.0, projectAtPos( worldPosition, R ) );
#else // USE_VIEW_SIZE
    pixelRadius = dhwidth * length( vec2( width, height ) );
    R = length( ssUnproject( vec3( .5 * viewport.x + pixelRadius, .5 * viewport.y, 0. ) ) -
                ssUnproject( vec3( .5 * viewport.x, .5 * viewport.y, 0. ) ) );     
#endif
  }
#ifndef MRT_ENABLED // in case MRT enabled normalized normals are available in texture 'normals'
  normal = gl_NormalMatrix * gl_Normal;
  normal = normalize( faceforward( -normal, normal, vec3( 0., 0., 1. ) ) );
#endif
  color = gl_Color;	
  gl_Position = gl_ProjectionMatrix * v;
#ifdef TEXTURE_ENABLED
  if( textureUnit >= 0 )
  {
    vec4 tc;
    if( textureUnit == 0 )  tc =  gl_MultiTexCoord0;
    else if( textureUnit == 1 ) tc =  gl_MultiTexCoord1;
    else if( textureUnit == 2 ) tc =  gl_MultiTexCoord2;
    else if( textureUnit == 3 ) tc =  gl_MultiTexCoord3;
    else if( textureUnit == 4 ) tc =  gl_MultiTexCoord4;
    else if( textureUnit == 5 ) tc =  gl_MultiTexCoord5;
    else if( textureUnit == 6 ) tc =  gl_MultiTexCoord6;
    else if( textureUnit == 7 ) tc =  gl_MultiTexCoord7;
    gl_TexCoord[ textureUnit ] = gl_TextureMatrix[ textureUnit ] * tc;
  }
#endif
}
  


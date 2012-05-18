// computes average kernel width and step multiplier from passed radius

// IN: numSamples, radius, dhwidth (%radius), maxSteps, ssao, depthMap
// OUT: occlusion modified gl_Color, normal, position
#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
uniform float radius; // object or scene radius 
uniform float dhwidth; // percentage of radius used as width of convolution kernel (0.1)
uniform float numSamples;
uniform float steps;

uniform int ssao;

varying vec4 color;
varying vec3 normal;
varying vec4 worldPosition;
varying float pixelRadius;
varying float R;
varying vec3 screenPosition;

//TEMPORARY
float width = 1024.0; 
float height = 768.0;

//------------------------------------------------------------------------------
vec3 screenSpace( vec4 v )
{
   vec4 p = gl_ProjectionMatrix *  v;
   p.xyz /= p.w;
   p.xyz *= 0.5;
   p.xyz += 0.5;
   p.x *= width;
   p.y *= height;
   return p.xyz;
}

//------------------------------------------------------------------------------
float projectAtPos( vec4 pos, float r )
{
   return distance( screenSpace( pos ), screenSpace( pos + vec4( r * pos.w, 0., 0., 0. ) ) ); 
}


//-----------------------------------------------------------------------------
vec3 ssUnproject( vec3 v )
{
  vec4 p = vec4( v, 1.0 );
  p.x /= width;
  p.y /= height;
  p.xyz -= 0.5;
  p.xyz *= 2.0;
  p = gl_ProjectionMatrixInverse * p;
  p.xyz /= p.w;
  return p.xyz;
}


//------------------------------------------------------------------------------
vec3 xTangentAtScreenPos( vec3 pos )
{
  float z2 = texture2DRect( depthMap, pos.xy + vec2( 1.0, 0. ) ).x;
  float z1 = texture2DRect( depthMap, pos.xy - vec2( 1.0, 0. ) ).x;
  return ssUnproject( vec3( pos.xy + vec2( 1.0, 0. ), z2 ) ) -
         ssUnproject( vec3( pos.xy - vec2( 1.0, 0. ), z1 ) ); 
}

//------------------------------------------------------------------------------
vec3 yTangentAtScreenPos( vec3 pos )
{
  float z2 = texture2DRect( depthMap, pos.xy + vec2( 0., 1.0 ) ).x;
  float z1 = texture2DRect( depthMap, pos.xy - vec2( 0., 1.0 ) ).x;
  return ssUnproject( vec3( pos.xy + vec2( 0.0, 1.0 ), z2 ) ) -
         ssUnproject( vec3( pos.xy - vec2( 0.0, 1.0 ), z1 ) ); 
}

//------------------------------------------------------------------------------
vec3 normalAtScreenPos( vec3 pos )
{
  return cross( xTangentAtScreenPos( pos ), yTangentAtScreenPos( pos ) ); 
}

//------------------------------------------------------------------------------
vec3 ssNormalAtScreenPos( vec3 pos )
{
  float zx2 = texture2DRect( depthMap, pos.xy + vec2( 1.0, 0. ) ).x;
  float zx1 = texture2DRect( depthMap, pos.xy - vec2( 1.0, 0. ) ).x;
  float zy2 = texture2DRect( depthMap, pos.xy + vec2( 0., 1.0 ) ).x;
  float zy1 = texture2DRect( depthMap, pos.xy - vec2( 0., 1.0 ) ).x;
  
  float dzdx = zx2 - zx1;
  float dzdy = zy2 - zy1;
  
  return normalize( vec3( dzdx, dzdy, 1.0 ) ); 
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
  worldPosition = gl_ModelViewMatrix * gl_Vertex;
  if( bool( ssao ) )
  {	
    R = dhwidth * radius;
	pixelRadius = max( 1.0, projectAtPos( worldPosition, R ) );
	normal = normalize( gl_NormalMatrix * gl_Normal );
	color = gl_Color;
	screenPosition = screenSpace( worldPosition );
	//normal = normalAtScreenPos( screenPosition );
  }
  gl_Position = gl_ProjectionMatrix * worldPosition;
  //worldPosition.xyz /= worldPosition.w;  	
}
  


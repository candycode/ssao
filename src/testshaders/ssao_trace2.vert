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
const float eps = 0.000001;

// add blur
float occlusion( vec2 dir, vec3 spos, float maxDist )
{
  vec3 ppos = spos;
  float z = 1.0;
  vec3 p = vec3( ppos.xy, ppos.z );
  float occl = 0.0;
  float yx = abs( dir.x ) < eps ? sign( dir.y ) : dir.y / dir.x;
  float s = abs( dir.x ) < eps ? 0.0 : sign( dir.x );
  float md = maxDist;// / max( width, height );
  float A = ( 1.0 - 0.01 ) / ( md * md );
  //A *= min( float( width ), float( height ) ); // maxDist / max pixels;
  s *= max( 1.0, maxDist / steps );
  vec3 N = normalize( normalAtScreenPos( spos ) );
  int step = 0;
  int occSteps = 0;
  float prev = 0.0;
  vec3 uspos = ssUnproject( spos );
  while( step < int( steps ) )
  {
    p.y += yx;
    p.x += s;
    z = texture2DRect( depthMap, p.xy ).x;
    if( z < p.z )
    {
      float dz = spos.z - z;
      float m = distance( p.xy, spos.xy );
      if( dz / m > prev )      
      {
        vec3 I = ssUnproject( vec3( p.xy, z ) ) - uspos;
        const float B = ( 1.0 - 0.1 ) / ( dhwidth * dhwidth * radius * radius  );
        occl += dot( N, normalize( faceforward( -I, I, N ) ) ) / ( 1.0 + B * dot( I, I ) );
        ++occSteps;
        p.z = z;
        prev = dz / m;
      }      
    }
    ++step;
  }
  return occl / max( 1.0, float( occSteps ) );// * float( occSteps ) / steps;// / max( 1.0, float( steps ) );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
  vec4 v = gl_ModelViewMatrix * gl_Vertex;
  if( bool( ssao ) )
  {	
	float r = max( 1.0, projectAtPos( v, dhwidth * radius ) );
	int hw = int( max( 1.0, numSamples / 8.0 ) ); 	
    // project
	vec3 ppos = screenSpace( v );
	// ppos is [x pixel, y pixel, depth (0..1) ]
	float occ = 0.0;
	for( int i = -hw; i != hw + 1; ++i )
	{
	  for( int j = -hw; j != hw + 1; ++j )
	  {
	     if( i != -hw && j != -hw && i != hw && j != hw ) continue;
	     occ += occlusion( vec2( float( i ), float( j ) ), ppos, r );
	  }
	}
	occ /= ( numSamples );  
	color = gl_Color;
	color.rgb -= occ;
	normal = gl_NormalMatrix * gl_Normal;
  }
  gl_Position = gl_ProjectionMatrix * v;  	
}
  


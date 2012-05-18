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

//------------------------------------------------------------------------------
vec3 xTangentAtScreenPos( vec3 pos )
{
  float dzdx =  0.5 * ( texture2DRect( depthMap, pos.xy + vec2( 1.0, 0. ) ).x -
                texture2DRect( depthMap, pos.xy - vec2( 1.0, 0. ) ).x  );
  return vec3( 1.0, 0.0, dzdx );                                      
}

//------------------------------------------------------------------------------
vec3 yTangentAtScreenPos( vec3 pos )
{
 float dzdy =  0.5 * ( texture2DRect( depthMap, pos.xy + vec2( 0., 1.0 ) ).x -
                texture2DRect( depthMap, pos.xy - vec2( 0., 1.0 ) ).x  ); 
 return vec3( 0.0, 1.0, dzdy );               
}

//------------------------------------------------------------------------------
vec3 normalAtScreenPos( vec3 p )
{
  
  return cross( xTangentAtScreenPos( p ), yTangentAtScreenPos( p ) );  
}

//------------------------------------------------------------------------------
const float eps = 0.000001;

// add blur
float occlusion( vec2 dir, vec3 ppos, float maxDist )
{
  float z = 1.0;
  vec3 p = vec3( ppos.xy, ppos.z );
  float occl = 0.0;
  float yx = abs( dir.x ) < eps ? sign( dir.y ) : dir.y / dir.x;
  float s = abs( dir.x ) < eps ? 0.0 : sign( dir.x );
  float A = ( 1.0 - 0.000001 ) / maxDist;
  A *= max( float( width ), float( height ) ); // maxDist / max pixels;
  s *= max( 1.0, maxDist / steps );
  int step = 0;
  while( step < int( steps ) )
  {
    p.y += yx;
    p.x += s;
    z = texture2DRect( depthMap, p.xy ).x;
    if( z < p.z )
    {
      vec3 dp = ppos - p;
      dp.x /= width; // x in range -1, 1
      dp.y /= height; // y in range -1, 1
      float sd = dot( dp, dp );
      occl += ( 1.0 / steps ) * ( p.z - z ) / (  A * sd ); 
      p.z = z;
    }
    ++step;
  }
  return occl; 
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
	occ /= numSamples;  
	color = gl_Color;
	color.rgb *= 1.0 - occ;
	normal = gl_NormalMatrix * gl_Normal;
  }
  gl_Position = gl_ProjectionMatrix * v;  	
}
  


// IN: color, normal, position, radius, ssao, depthMap, viewport width, viewport height.
// OUT: occlusion modified gl_Color

#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
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
const float eps = 0.0000001;

// add blur
float occlusion( vec2 dir, vec3 spos )
{
  float z = 1.0;
  vec3 p = spos;
  float occl = 0.0;
  float yx = abs( dir.x ) < eps ? sign( dir.y ) : dir.y / dir.x;
  float s = abs( dir.x ) < eps ? 0.0 : sign( dir.x );
  int S = int( clamp( steps, 1.0, min( width, height ) ) );//int( max( 1.0, ( 1.0 - gl_FragCoord.z ) * steps ) );
  s *= max( 1.0, float( pixelRadius ) / float( S ) );
  int step = 0;
  int occSteps = 0;
  float prev = 0.0;
  
  
  /*for( ; step != S && z >= p.z; ++step )
  {
	p.y += yx;
    p.x += s;
    z = texture2DRect( depthMap, p.xy ).x;
  }
  if( z >= p.z ) return 0.0;
  */ 
  const float B = ( 1.0 - 0.01 ) / ( R * R  );
  float dz = 0.;//spos.z - z;
  float m = 1.;//distance( p.xy, spos.xy );
  prev = dz / m; 
  vec3 I;// = ssUnproject( p ) - worldPosition.xyz;
  //occl += dot( normal, normalize( faceforward( -I, I, normal ) ) ) / ( 1.0 );// + B * dot( I, I ) );
  //++occSteps;
 
  step = 0;
  float oy = p.y;
  while( step < S && distance( p.xy, spos.xy ) < pixelRadius )
  {
    p.x += s;
    p.y += abs( dir.x ) < eps ? sign( dir.y ) : yx * s;
    /*if( abs( dir.y ) > eps && abs( oy - p.y ) < 0.5 ) 
    {
      ++step;
      continue;
    } 
    oy = p.y;*/
    
    z = texture2DRect( depthMap, p.xy ).x;
    /*z += texture2DRect( depthMap, p.xy + vec2( 0.5, 0.0 ) ).x;
    z += texture2DRect( depthMap, p.xy - vec2( 0.5, 0.0 ) ).x;
    z += texture2DRect( depthMap, p.xy + vec2( 0.0, 0.5 ) ).x;
    z += texture2DRect( depthMap, p.xy - vec2( 0.0, 0.5 ) ).x;
    z += texture2DRect( depthMap, p.xy + vec2( 0.5, 0.5 ) ).x;
    z += texture2DRect( depthMap, p.xy - vec2( 0.5, 0.5 ) ).x;
    z += texture2DRect( depthMap, p.xy + vec2( -0.5, 0.5 ) ).x;
    z += texture2DRect( depthMap, p.xy + vec2( 0.5, -0.5 ) ).x;
    z /= 9.0;*/
    if( z < p.z )
    {
      dz = spos.z - z;
      m = distance( p.xy, spos.xy );
      if( dz / m > prev )      
      {
        p.z = z;
        prev = dz / m;
        I = ssUnproject( p ) - worldPosition.xyz;
        occl += dot( normal, normalize( faceforward( -I, I, normal ) ) ) / ( 1.0 + B * dot( I, I ) );
        ++occSteps;
      }      
    }
    ++step;
  }
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
  if( bool( ssao ) )
  {	
    if( !gl_FrontFacing ) discard;
    /*vec4 v = gl_FragCoord;
    v.xyzw /= v.w;
    v.xyz -= 0.5;
    v.xyz *= 2.0;
    worldPosition = gl_ProjectionMatrixInverse * v;
    worldPosition.xyzw /= worldPosition.w;*/
    worldPosition.xyzw /= worldPosition.w;
	int hw = int( max( 1.0, numSamples / 8.0 ) ); 	
    // project
	//vec3 ppos = screenSpace( worldPosition );
	// ppos is [x pixel, y pixel, depth (0..1) ]
	float occ = 0.0;
	for( int i = -hw; i != hw + 1; ++i )
	{
	  for( int j = -hw; j != hw + 1; ++j )
	  {
	     if( i != -hw && j != -hw && i != hw && j != hw ) continue;
	     occ += occlusion( vec2( float( i ), float( j ) ), screenSpace( worldPosition ) );
	  }
	}
	occ /= ( numSamples );
	gl_FragColor = color;  
	gl_FragColor.rgb -= smoothstep( 0.0, 1.0, occ );
  }  	
}
  


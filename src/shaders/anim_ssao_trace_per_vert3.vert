// IN: color, normal, position, radius, ssao, depthMap, viewport width, viewport height.
// OUT: occlusion modified gl_Color

#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
uniform float numSamples;
uniform float steps;
uniform float hwMax; //max pixels
uniform float dhwidth;
uniform float radius;
uniform int ssao;
uniform float dstep;

varying vec4 color;
varying vec3 normal;
vec4 worldPosition;
float pixelRadius;
float R;
vec3 screenPosition;
vec3 viewdir;
uniform vec2 viewport;

float width = viewport.x;
float height = viewport.y;



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
float projectAtNearPos( vec4 pos, in float r )
{
   //vec4 pos = p;
   //pos.z = gl_DepthRange.near * pos.w;
   vec2 p1 = screenSpace( pos ).xy;
   vec2 p2 = screenSpace( pos + vec4( r * pos.w, 0., 0., 0. ) ).xy;			
   return distance( p1, p2 ); 
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
  int S = int( clamp( steps, 1.0, min( width, height ) ) );
  int step = 0;
  int occSteps = 0;
  float prev = 0.0;
  float dz = 0.;
  float m = 1.;
  prev = dz / m; 
  vec3 I;
  float PR = clamp( pixelRadius, 1.0, hwMax );
  const float PRP = PR *  R * pixelRadius;
  const float B = ( 1.0 - 0.3 ) / ( PRP * PRP );
   
  int maxSteps = abs( dir.x ) < eps || abs( dir.y ) < eps ? int( PR ) :
                 int( PR * inversesqrt( 1.0 + yx * yx ) );  
  
  if( maxSteps == 0 ) return 0.0;
  
  float ds = s * PR * max( dstep, 1. / PR );
  float dy = abs( dir.x ) < eps ? sign( dir.y ) * ds : yx * ds;
  float xstep = 0.;
  float ystep = 0.;
  while( step != maxSteps )
  {
    ++step;
    xstep += ds;
    ystep += dy;
    if( abs( xstep ) < 1.0 && abs( ystep ) < 1.0 ) continue;
    p.x += xstep;
    p.y += ystep;
    z = texture2DRect( depthMap, p.xy ).x;
    xstep = 0.;
    ystep = 0.;
    if( z < p.z  )
    {
      dz = spos.z - z;
      dz *= dz;
      m = dot( p.xy - spos.xy, p.xy - spos.xy );
      if( dz / m >= prev )      
      {
        p.z = z;
        prev = dz / m;
        I = ssUnproject( p ) - worldPosition.xyz;
		//if( dot( normal, faceforward( -I, I, normal ) ) <= eps ) continue;
        float ka = dot( viewdir, /*normalize( I ) );//*/ normalize( faceforward( -I, I, viewdir ) ) );
        //if( ka > 0. )
        {
          occl += ka / ( 1.0 + B * dot( I, I ) );
          ++occSteps;
        }
      }    
    }  
  }
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#if 0
uniform int osg_FrameNumber;
vec4 NextPosition()
{
  vec4 v;
  float r = sqrt( gl_Vertex.x * gl_Vertex.x + gl_Vertex.z * gl_Vertex.z );
  //float M = sin( 6.28 * ( gl_Vertex.x + gl_Vertex.y ) * osg_FrameNumber / 960.0 );
  v.x =  gl_Vertex.x + gl_Vertex.y * sin( gl_Vertex.y * ( 1.  / radius ) *  osg_FrameNumber / 60.0 );
  v.z =  gl_Vertex.z + gl_Vertex.y * cos( gl_Vertex.y * ( 1.  / radius ) *  osg_FrameNumber / 60.0 );
  v.y =  gl_Vertex.y + gl_Vertex.z * cos( gl_Vertex.z * ( 1.  / radius ) *  osg_FrameNumber / 60.0 );
  v.w =  gl_Vertex.w;
  return v;
}
#endif
 
void main()
{
#if 0
  worldPosition = gl_ModelViewMatrix * NextPosition();
#else
  worldPosition = gl_ModelViewMatrix * gl_Vertex;
#endif  
  if( bool( ssao ) )
  {	
    normal = gl_NormalMatrix * gl_Normal;
    R = dhwidth * radius;
	pixelRadius = max( 1.0, projectAtPos( worldPosition, R ) );
	int hw = int( max( 1.0, numSamples / 8.0 ) ); 	
    // project
	vec3 ppos = screenSpace( worldPosition );
	screenPosition = ppos;
	viewdir = vec3( 0., 0., 1. ); //normalize( normalAtScreenPos( ppos ) );
	//normal = normalize( normalAtScreenPos( ppos ) );
	//normal = faceforward( -normal, normal, vec3( 0., 0., 1. ) ); 
	// ppos is [x pixel, y pixel, depth (0..1) ]
	float occ = 0.0;
	for( int i = -hw; i != hw + 1; ++i )
	{
	  for( int j = -hw; j != hw + 1; ++j )
	  {
	     if( i != -hw && j != -hw && i != hw && j != hw ) continue;
	     occ += occlusion( vec2( float( i ), float( j ) ), ppos );
	  }
	}
	occ /= ( numSamples );  
	color = gl_Color;
	color.rgb *= 1.0 - smoothstep( 0.0, 1.0, occ );	
  }
  gl_Position = gl_ProjectionMatrix * worldPosition;  	
}
  


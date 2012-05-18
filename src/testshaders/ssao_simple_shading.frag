#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
uniform int ssao;
uniform int shade;
// d sampling step in pixels
uniform float samplingStep;
// s half number of samples in each direction; total samples = ( 2 * s + 1 ) * ( 2 * s + 1 )
uniform float halfSamples;

varying vec3 N;
varying vec4 color;


float wf( float d )
{
  float P = 2.0 * halfSamples + 1.0;
  P *= P;
  P = 1. / P;
  float B = 0.9;
  float H = halfSamples * samplingStep;
  float a = ( P - B ) / ( 2.0 * H * H * B );
  return P / ( a * d * d + 1.0 );
}

// returns occlusion at pixel x, y
float ComputeWeightedOcclusion( int x, int y, float z )
{
    float zt = 0.0;
	// attempt to correlate the radius and step with disatance from viewer; it really
	// depends on the object topology
	int step = int( samplingStep ); //int( max( 1.0, samplingStep * ( 1.0 - .3 * z ) ) );
	int hs = int( halfSamples ); //int( max( 1.0, float( halfSamples ) * ( 1.0 - .5 * z ) ) );
	float pi = 0.0;
	float pj = 0.0;
	for( int i = -hs; i != hs + 1; ++i )
	{
	  pi = float( x + step * i );
	  for( int j = -hs; j != hs + 1; ++j )
	  {
	    pj = float( y + step * j );
	    //if( i == 0 && j == 0 ) continue;
	    float zz = texture2DRect( depthMap, vec2( pi, pj ) ).x;
	    //if( zz < MAX_Z )
	    {
	       	zt += wf( distance( vec2( pi, pj ), vec2( float( x ), float( y ) ) ) ) * zz;
		}
	  }
	}
	//If z > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments.
	//The distance between the fragment and the average of its neighbors is considered an occlusion value to be
	//subtracted from or multiplied by the pixel luminance/color.  
	return 1.0 - sqrt( clamp( ( z - zt ), 0.0, 1.0 ) );
}


// returns occlusion at pixel x, y
float ComputeOcclusion( vec4 p )
{
    float zt = 0.0;
	// attempt to correlate the radius and step with disatance from viewer; it really
	// depends on the object topology
	float step = samplingStep; //max( 1.0, samplingStep * ( 1.0 - p.z ) );
	int hs = int( halfSamples ); //int( max( 1.0, float( halfSamples ) * ( 1.0 - .5 * p.z ) ) );
	vec2 pij = vec2( 0., 0. );
	for( int i = -hs; i != hs + 1; ++i )
	{
	  pij.x = p.x + step * float( i );
	  for( int j = -hs; j != hs + 1; ++j )
	  {
	    pij.y = p.y + step * float( j );
	    float zz = texture2DRect( depthMap, pij ).x;
	   	zt += zz;
	  }
	}
	float P = 2.0 * halfSamples + 1.0;
	P *= P;
    P = 1. / P;
	//If z > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments.
	//The distance between the fragment and the average of its neighbors is considered an occlusion value to be
	//subtracted from or multiplied by the pixel luminance/color.  
	return smoothstep( 0., 1., sqrt( clamp( 2.0 * ( p.z - zt * P ), 0.0, 1.0 ) ) );
}



vec3 ld1 = vec3( 0.577350269, 0.577350269, 0.577350269 );
vec3 ld2 = vec3( -0.577350269, -0.577350269, 0.577350269 );
vec3 ld3 = vec3( -0.577350269, 0.577350269, 0.577350269 );
vec3 ld4 = vec3( 0.577350269, -0.577350269, 0.577350269 );
float kd = 1.0;

vec4 ComputeColor()
{
	return vec4( color.rgb * 0.5 * ( max( 0.0, dot( N, ld1 ) )
								   + max( 0.0, dot( N, ld2 ) )
								   + max( 0.0, dot( N, ld3 ) )
								   + max( 0.0, dot( N, ld4 ) ) ), color.a );
}

void main(void)
{
  if( bool( shade ) )
  {
    gl_FragColor = ComputeColor();
  }
  clamp( gl_FragColor, 0., 1.0 ); 
  if( bool( shade ) && bool( ssao ) ) 
  {
	gl_FragColor.rgb -= ComputeOcclusion( gl_FragCoord );
  }
}


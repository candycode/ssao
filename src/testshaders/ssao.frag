#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;

// d sampling step in pixels
uniform float samplingStep;
// s half number of samples in each direction; total samples = ( 2 * s + 1 ) * ( 2 * s + 1 )
uniform float halfSamples;

// returns average depth at pixel x, y
float ComputeAverageDepth( int x, int y, float z )
{

	
    int occ = 0;
	float zt = 0.0;
	// attempt to correlate the radius and step with disatance from viewer; it really
	// depends on the object topology
	int step = int( samplingStep ); //int( max( 1.0, samplingStep * ( 1.0 - .3 * z ) ) );
	int hs = int( halfSamples ); //int( max( 1.0, float( halfSamples ) * ( 1.0 - .5 * z ) ) );
	for( int i = -hs; i != hs + 1; ++i )
	{
	  for( int j = -hs; j != hs + 1; ++j )
	  {
	    //if( i == 0 && j == 0 ) continue;
	    float zz = texture2DRect( depthMap, vec2( float( x + step * i ), float( y + step * j ) ) ).x;
	    //if( zz < MAX_Z )
	    {
			zt += zz;
			++occ;
		}
	  }
	}
	float r = zt / float( occ ); 
	return r;
}


float GC = 1.0 / 273.0;

float gaussian[] = { 1.,  4.,  7.,  4., 1.,
                           4., 16., 26., 16., 4.,
                           7., 26., 41., 26., 7.,
                           4., 16., 26., 16., 4.,
                           1.,  4.,  7.,  4., 1. };
                  
// returns average depth at pixel x, y
float ComputeGaussianAverageDepth( int x, int y, float z )
{
    float zt = 0.0;
	int step = 1;
	int hs = 2;
	for( int i = -hs; i != hs; ++i )
	{
	  for( int j = -hs; j != hs; ++j )
	  {
	    //if( i == 0 && j == 0 ) continue;
	    float zz = texture2DRect( depthMap, vec2( float( x + step * i ), float( y + step * j ) ) ).x;
	    //if( zz < MAX_Z )
	    {
			zt += zz * gaussian[ 5 * ( j + hs ) + ( i + hs ) ] * GC;
		}
	  }
	}
	return zt;
}


void main(void)
{
  float l = gl_FragCoord.z - ComputeAverageDepth( int( gl_FragCoord.x ), int( gl_FragCoord.y ), gl_FragCoord.z );
  //If > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments.
  //The distance between the fragment and the average of its neighbors is considered an occlusion value to be
  //subtracted from or multiplied by the pixel luminance/color.  
  l = 1.0 - sqrt( clamp( l, 0.0, 1.0 ) );
  gl_FragColor = vec4( l, l, l, 1. );
}


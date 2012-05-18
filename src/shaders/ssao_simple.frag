#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;

// d sampling step in pixels
uniform float samplingStep;
// s half number of samples in each direction; total samples = ( 2 * s + 1 ) * ( 2 * s + 1 )
uniform float halfSamples;

varying vec4 color;

// returns occlusion at pixel x, y
float ComputeOcclusion( int x, int y, float z )
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
	//If z > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments (pixels).
	//The distance between the fragment and the average of its neighbors is considered an occlusion value to be
	//subtracted from or multiplied by the pixel luminance/color.  
	return 1.0 - sqrt( clamp( z - zt / float( occ ), 0.0, 1.0 ) );
}

void main(void)
{
  float l = ComputeOcclusion( int( gl_FragCoord.x ), int( gl_FragCoord.y ), gl_FragCoord.z );
  gl_FragColor = vec4( color.rgb * smoothstep( -0.02, 1., l ), color.a );
}


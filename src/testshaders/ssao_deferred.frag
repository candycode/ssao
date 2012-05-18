#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
uniform sampler2DRect colorMap;

varying vec2 pos;

const float MAX_Z = 0.8;

//TEMPORARY - REPLACE WITH UNIFORMS
// d sampling step in pixels
const int samplingStep = 8;
// s half number of samples in each direction; total samples = ( 2 * s + 1 ) * ( 2 * s + 1 )
const int halfSamples = 4;

// returns average depth at pixel x, y
float ComputeAverageDepth( int x, int y, float z )
{
	
    int occ = 0;
	float zt = 0.0;
	// attempt to correlate the radius and step with disatance from viewer; it really
	// depends on the object topology
	int step = int( max( 1.0, float( samplingStep ) * ( 1.0 - .3 * z ) ) );
	int hs = halfSamples; //int( max( 1.0, float( halfSamples ) * ( 1.0 - .5 * z ) ) );
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

void main(void)
{
  //float l = texture2DRect( depthMap, pos ).x - ComputeAverageDepth( int( gl_FragCoord.x ), int( gl_FragCoord.y ), gl_FragCoord.z );
  //If > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments.
  //The distance between the fragment and the average of its neighbors is considered an occlusion value to be
  //subtracted from or multiplied by the pixel luminance/color.  
  //l = 1.0 - sqrt( clamp( l, 0.0, 1.0 ) );
  //vec4 color = texture2DRect( colorMap, pos );
  vec4 color = vec4( pos, 0., 1. );
  color.r /= 1024.0;
  color.g /= 768.0;
  gl_FragColor = texture2DRect( depthMap, pos );
}


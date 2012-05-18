#extension GL_ARB_texture_rectangle : enable
uniform sampler2DRect depthMap;
varying float samplingStep;
varying float halfWidth;

varying vec3 normal;
varying vec4 color;

uniform int ssao;

// returns average depth at pixel x, y
float ComputeAverageDepth( float x, float y )
{

	
	int occ = 0;
	float zt = 0.0;
	// attempt to correlate the radius and step with disatance from viewer; it really
	// depends on the object topology
	float step = samplingStep;
	int hs = int( halfWidth );
	for( int i = -hs; i != hs + 1; ++i )
	{
	  for( int j = -hs; j != hs + 1; ++j )
	  {
	    if( i == 0 && j == 0 ) continue;
	    float zz = texture2DRect( depthMap, vec2( x + step * float( i ), y + step * float( j ) ) ).x;
	    //if( zz < MAX_Z )
	    {
			zt += zz;
			++occ;			
		}
	  }
	}
	//occ = ( 2. * float( hs ) + 1.0 ) * ( 2. * float( hs ) + 1.0 );
	return zt / float( occ ); 
}

void main()
{
  float l = bool( ssao ) ? gl_FragCoord.z - ComputeAverageDepth( gl_FragCoord.x, gl_FragCoord.y ) : 0.0;
  //If > 0 it means the fragment is behind (further away from the viewpoint) the neighboring fragments.
  //The distance between the fragment and the average of its neighbors is considered an occlusion value to be
  //subtracted from or multiplied by the pixel luminance/color.  
  l = smoothstep( 0., 1. , 1.0 - sqrt( clamp( l, 0.0, 1.0 ) ) );
  //l *= l; // WARNING remove for smooth surfaces without holes
  
  //float kn = 1.0;
  float kn = dot( vec3( 0., 0., 1.0 ), normal );
  l *= kn;
  gl_FragColor = vec4( color.r * l, color.g * l, color.b * l, color.a );
    
  //gl_FragColor = vec4( l, l, l, 1. );
}


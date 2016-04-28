// IN: color, normal, position, radius, ssao, depthMap, viewport width, viewport height.
// OUT: occlusion modified gl_Color

//#define TEXTURE_ENABLED

#extension GL_ARB_texture_rectangle : enable
#ifdef MRT_ENABLED
uniform sampler2DRect positions;
uniform sampler2DRect normals; // note that in this case it is better to store the
                               // z coordinate into the positions texture since
                               // the normals texture is not used
vec4 worldPosition;
vec3 normal;
#else
uniform sampler2DRect depthMap;
varying vec3 normal;
varying vec4 worldPosition;
#endif
varying float visibility;

#ifdef TEXTURE_ENABLED
varying vec4 texCoord;
uniform int textureUnit; // textureUnit < 0 => no texture
#endif

uniform float numSamples;
uniform float steps;
uniform float hwMax; //max pixels
uniform float dhwidth;
uniform float radius;
uniform int ssao;
uniform float dstep;
uniform float occlusionFactor;

varying vec4 color;

float pixelRadius;
float R;
vec3 screenPosition;

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

//-----------------------------------------------------------------------------
vec3 ssUnproject( vec3 v )
{
#ifdef MRT_ENABLED
  return texture2DRect( positions, v.xy ).xyz;
#else
  vec4 p = vec4( v, 1.0 );
  p.x /= width;
  p.y /= height;
  p.xyz -= 0.5;
  p.xyz *= 2.0;
  p = gl_ProjectionMatrixInverse * p;
  p.xyz /= p.w;
  return p.xyz;
#endif
}

//------------------------------------------------------------------------------
uniform float minCosAngle;// = 0.2;//mininum angle used for angle occlusion computation
// adjusted pixel radius
float PR = 1.0;
// adjusted world space radius
float PRP = 1.0;
// attenuation coefficient
float B = 0.0;


void ComputeRadiusAndOcclusionAttenuationCoeff()
{
  // clamp pixel radius to [1.0, max radius]  
  PR = clamp( pixelRadius, 1.0, hwMax );
  // set coefficient for occlusion attenuation function 1 / ( 1 + B * dist^2 )
  // Rw : Rs = NRw : NRs => NRw = Rw * NRs / Rs 
  PRP = R * PR / pixelRadius; // clamp world space radius
  // the coefficient is set such that the occlusion at maximum distance
  // is ~ 1/2 of the peak value
  B = ( 1.0 - 0.1 ) / ( PRP * PRP );
}

//------------------------------------------------------------------------------
// occlusion function for horizontal (y=y0) lines
float hocclusion( float ds )
{
 
  // follow line y = y0 in screen space along negative or positive direction according to ds
  // where:
  //    x0 = screenPosition.x
  //    y0 = screenPosition.y  
  // each point visible from current fragment (i.e. with z < current z)
  // will contribute to the occlusion factor
  int occSteps = 0;  // number of occlusion rays 
  vec3 p = screenPosition.xyz;
  float occl = 0.0; // occlusion
  float z = 1.0; // z in depth map
  float dz = 0.; //
  float dist = 1.; // distance between current point and shaded point 
  float prev = 0.; // previous angular coefficient 
  vec3 I; // vector from point in depth map to shaded point
  int upperI = int( PR / abs( ds ) );
  for( int i = 0; i != upperI; ++i )
  {
      p.x += ds;
#ifdef MRT_ENABLED
    z = texture2DRect( normals, p.xy ).w;
#else
    z = texture2DRect( depthMap, p.xy ).x;
#endif   
    // get depth value from depth map; if point is closer to viewepoint than
    // previous point then compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
    //if( z < p.z  )
    {
      
      dz = screenPosition.z - z;
      dist = distance( p.xy, screenPosition.xy );
      float angCoeff = dz / dist;
      if(  angCoeff > prev )      
      {
        p.z = z;
        prev = angCoeff;
        // compute vector from point in world coordinates to point whose
        // occlusion is being computed
#ifdef MRT_ENABLED // when Multiple Render Targets is enabled the world position
                   // of each pixel is available in 'positions' texture
        I = texture2DRect( positions, p.xy ).xyz - worldPosition.xyz;
#else
        I = ssUnproject( p ) - worldPosition.xyz;
#endif
        // ADD AO contribution: function of angle between normal or viewpoint and distance
        float k = dot( normal, normalize( I ) );
        // if occluded compute occlusion and increment number of occlusions
        if( k > minCosAngle )
        {
          occl += ( k / ( 1. + B * dot( I, I ) ) );
          ++occSteps;
        }
      }      
    }
  }
  // return average occlusion along ray: divide by number of occlusions found 
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
// occlusion function for vertical (x=x0) lines
float vocclusion( float ds )
{
 
  // follow line x = x0 in screen scpace along negative or positive direction according to ds
  // where:
  //    x0 = screenPosition.x
  //    y0 = screenPosition.y  
  // each point visible from current fragment (i.e. with z < current z)
  // will contribute to the occlusion factor
  int occSteps = 0;  // number of occlusion rays 
  vec3 p = screenPosition.xyz;
  float occl = 0.0; // occlusion
  float z = 1.0; // z in depth map
  float dz = 0.; //
  float dist = 1.; // distance between current point and shaded point 
  float prev = 0.; // previous angular coefficient 
  vec3 I; // vector from point in depth map to shaded point
  int upperI = int( PR / abs( ds ) );
  for( int i = 0; i != upperI; ++i )
  {
    p.y += ds; 
#ifdef MRT_ENABLED
    z = texture2DRect( normals, p.xy ).w;
#else
    z = texture2DRect( depthMap, p.xy ).x;
#endif    // get depth value from depth map; if point is closer to viewepoint than
    // previous point then compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
   // if( z < p.z  )
    {
      dz = screenPosition.z - z;
      dist = distance( p.xy, screenPosition.xy );
      float angCoeff = dz / dist;
      if(  angCoeff > prev )      
      {
        p.z = z;
        prev = angCoeff;

        // compute vector from point in world coordinates to point whose
        // occlusion is being computed
#ifdef MRT_ENABLED // when Multiple Render Targets is enabled the world position
                   // of each pixel is available in 'positions' texture
        I = texture2DRect( positions, p.xy ).xyz - worldPosition.xyz;
#else
        I = ssUnproject( p ) - worldPosition.xyz;
#endif
        // ADD AO contribution: function of angle between normal or viewpoint and distance
        float k = dot( normal, normalize( I ) );
        // if occluded compute occlusion and increment number of occlusions
        if( k > minCosAngle )
        {
          occl +=  ( k / ( 1. + B * dot( I, I ) ) );
          ++occSteps;
        }
      }      
    }
  }
  // return average occlusion along ray: divide by number of occlusions found 
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
// occlusion function for non degenerate (i.e. lines not paralles to x or y axis) lines
float occlusion( vec2 dir )
{
  // compute angular coefficient and steps
  float m = dir.y / dir.x;
  
  // follow line y = m * ( x - x0 ) + y0 in screen space
  // where:
  //    m = dir.y / dir.x
  //    x0 = screenPosition.x
  //    y0 = screenPosition.y  
  // each point visible from current fragment (i.e. with z < current z)
  // will contribute to the occlusion factor
  int occSteps = 0;  // number of occlusion rays 
  vec3 p = screenPosition.xyz;
  // compute number of i (x) steps
  // size of radius = sqrt( (num x steps)^2 + (ang. coeff. * num x steps)^2 )
  int upperI = int( PR * inversesqrt( 1.0 + m * m ) / abs( dstep ) );
  float occl = 0.0; // occlusion
  float z = 1.0; // z in depth map
  float dz = 0.; //
  float dist = 1.; // distance between current point and shaded point 
  float prev = 0.; // previous angular coefficient 
  vec3 I; // vector from point in depth map to shaded point
  float ds = sign( dir.x ) * dstep;
  for( int i = 0; i != upperI; ++i )
  {
    p.x += ds;
    p.y += ds * m;
#ifdef MRT_ENABLED
    z = texture2DRect( normals, p.xy ).w;
#else
    z = texture2DRect( depthMap, p.xy ).x;
#endif    // get depth value from depth map; if point is closer to viewepoint than
    // previous point then compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
   // if( z < p.z  )
    {
      dz = screenPosition.z - z;
      dist = distance( p.xy, screenPosition.xy );
      float angCoeff = dz / dist;
      if(  angCoeff > prev )      
      {
        p.z = z;
        prev = angCoeff;

        // compute vector from point in world coordinates to point whose
        // occlusion is being computed
#ifdef MRT_ENABLED // when Multiple Render Targets is enabled the world position
                   // of each pixel is available in 'positions' texture
        I = texture2DRect( positions, p.xy ).xyz - worldPosition.xyz;
#else
        I = ssUnproject( p ) - worldPosition.xyz;
#endif
        // ADD AO contribution: function of angle between normal or viewpoint and distance
        float k = dot( normal, normalize( I ) );
        // if occluded compute occlusion and increment number of occlusions
        if( k > minCosAngle )
        {
          occl += ( k / ( 1. + B * dot( I, I ) ) );
          ++occSteps;
        }
      }      
    }
  } 
  // return average occlusion along ray: divide by number of occlusions found 
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
float ComputeOcclusion()
{
    // ppos is [x pixel, y pixel, depth (0..1) ]
	// numSamples is an upper limit fr the number of directions == number of rayso
    // if the 2D (i,j) sample points are laid out on the edges of a square
    // and numSamples is the total number of points then the number of samples on one edge
    // is numSamples / 4;
    // the i and j indices are assumed to be in the range:
    //  [-(numSamples / 4) / 2, +(numSamples / 4) / 2] == [ -numSamples/8,+numSamples/8 ]
    int hw = int( max( 1.0, numSamples / 8.0 ) ); 	
    // ppos is [x pixel, y pixel, depth (0..1) ]
    float occ = 0.0;
    int i = -hw;
    int j = -hw;
    // vertical edges, j = 0 excluded
    for( ; j != 0; ++j ) occ += occlusion( vec2( float( i ), float( j ) ) );
    for( j = 1; j != hw + 1; ++j ) occ += occlusion( vec2( float( i ), float( j ) ) );
    i = hw;
    for( ; j != hw + 1; ++j ) occ += occlusion( vec2( float( i ), float( j ) ) );
    for( j = 1; j != hw + 1; ++j ) occ += occlusion( vec2( float( i ), float( j ) ) );
    // horizontal edges, i = 0 excluded
    j = -hw;
    for( i = -hw + 1; i != 0; ++i ) occ += occlusion( vec2( float( i ), float( j ) ) );
    for( i = 1; i != hw; ++i ) occ += occlusion( vec2( float( i ), float( j ) ) );
    j = hw;
    for( i = -hw + 1; i != 0; ++i ) occ += occlusion( vec2( float( i ), float( j ) ) );
    for( i = 1; i != hw; ++i ) occ += occlusion( vec2( float( i ), float( j ) ) ); 
    // i = 0 and j = 0
    occ += hocclusion( -dstep );
    occ += hocclusion( dstep );
    occ += vocclusion( -dstep );
    occ += vocclusion( dstep );
    occ /= max( 1.0, float( 8 * hw - 2 ) );
    return occ;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
  worldPosition = gl_ModelViewMatrix * gl_Vertex;
  color = gl_Color;
  normal = normalize( gl_NormalMatrix * gl_Normal );
  visibility = 1.0;
  if( bool( ssao ) )
  {	   
    R = dhwidth * radius;
	pixelRadius = max( 1.0, projectAtPos( worldPosition, R ) );
	// project
    screenPosition = screenSpace( worldPosition );
	ComputeRadiusAndOcclusionAttenuationCoeff();
	
	visibility = 1.0 - smoothstep( 0.0, 1.0, ComputeOcclusion() * occlusionFactor );	
  }
  gl_Position = gl_ProjectionMatrix * worldPosition;
#ifdef TEXTURE_ENABLED
  if( textureUnit >= 0 )
  {
    vec4 tc;
    if( textureUnit == 0 )  tc =  gl_MultiTexCoord0;
    else if( textureUnit == 1 ) tc =  gl_MultiTexCoord1;
    else if( textureUnit == 2 ) tc =  gl_MultiTexCoord2;
    else if( textureUnit == 3 ) tc =  gl_MultiTexCoord3;
    else if( textureUnit == 4 ) tc =  gl_MultiTexCoord4;
    else if( textureUnit == 5 ) tc =  gl_MultiTexCoord5;
    else if( textureUnit == 6 ) tc =  gl_MultiTexCoord6;
    else if( textureUnit == 7 ) tc =  gl_MultiTexCoord7;
    gl_TexCoord[ textureUnit ] = gl_TextureMatrix[ textureUnit ] * tc;
  }
#endif
}
  


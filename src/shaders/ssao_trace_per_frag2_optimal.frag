#define TEXTURE_ENABLED

// IN: color, normal, position, radius, ssao, depthMap, viewport width, viewport height.
// OUT: occlusion modified gl_Color
//#define MRT_ENABLED
#extension GL_ARB_texture_rectangle : enable
#ifdef MRT_ENABLED
uniform sampler2DRect positions;
uniform sampler2DRect normals;
#else
uniform sampler2DRect depthMap;
#endif
uniform float numSamples; //number of rays
uniform float hwMax; //max pixels
uniform int ssao; //enable/disable ssao
uniform float dstep; //step multiplier 
uniform float occlusionFactor; // occlusion multiplier

#ifdef TEXTURE_ENABLED
uniform sampler2D tex;
uniform int textureUnit; // textureUnit < 0 => no texture
uniform int textureEnabled;
#endif

varying vec4 color; // vertex color

#ifdef MRT_ENABLED // in case of multiple render targets normals are available in 'normals' texture
                   // and world space positions in 'positions' texture
vec3 normal;
vec3 worldPosition;
#else
varying vec3 normal;
varying vec3 worldPosition; 
#endif


varying float R; // radius in world coordinates
varying float pixelRadius; //radius in screen coordinates (=pixels)

uniform vec2 viewport;

float width = viewport.x;
float height = viewport.y;

vec3 screenPosition;

//-----------------------------------------------------------------------------
#ifndef MRT_ENABLED
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
#endif

//------------------------------------------------------------------------------
//cosine of mininum angle used for angle occlusion computation (~30 deg. best)
uniform float minCosAngle; // = 0.2; // ~78 deg. from normal, ~22 deg from tangent plane
// adjusted pixel radius
float PR = 1.0;
// adjusted world space radius
float PRP = 1.0;
// attenuation coefficient
float B = 0.0;

// TODO: move in vertex shader; it will be faster only for small models (<1M vertices)
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
  vec3 p = gl_FragCoord.xyz;
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
    // compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
    dz = screenPosition.z - z;
    dist = distance( p.xy, screenPosition.xy );
    float angCoeff = dz / dist;
    if( angCoeff > prev )      
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
      // ADD AO contribution: function of angle between normal and ray
      float k = dot( normal, normalize( I ) );
      // if occluded compute occlusion and increment number of occlusion
      // contributions
      if( k > minCosAngle )
      {
        occl += ( k / ( 1. + B * dot( I, I ) ) );
        ++occSteps;
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
  vec3 p = gl_FragCoord.xyz;
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
#endif    
    // compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
    dz = screenPosition.z - z;
    dist = distance( p.xy, screenPosition.xy );
    float angCoeff = dz / dist;
    if( angCoeff > prev )      
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
      // ADD AO contribution: function of angle between normal and ray
      float k = dot( normal, normalize( I ) );
      // if occluded compute occlusion and increment number of occlusion
      // contributions
      if( k > minCosAngle )
      {
        occl += ( k / ( 1. + B * dot( I, I ) ) );
        ++occSteps;
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
  vec3 p = gl_FragCoord.xyz;
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
#endif   
    // compute angular coefficient: if angular coefficient
    // is greater than last computed coefficient it means the point is 
    // visible from screenPosition and therefore occludes it 
    dz = screenPosition.z - z;
    dist = distance( p.xy, screenPosition.xy );
    float angCoeff = dz / dist;
    if( angCoeff > prev )      
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
      // ADD AO contribution: function of angle between normal and ray
      float k = dot( normal, normalize( I ) );
      // if occluded compute occlusion and increment number of occlusion
      // contributions
      if( k > minCosAngle )
      {
        occl += ( k / ( 1. + B * dot( I, I ) ) );
        ++occSteps;
      }
    }      
  }

  // return average occlusion along ray: divide by number of occlusions found 
  return occl / max( 1.0, float( occSteps ) );
}

//------------------------------------------------------------------------------
float ComputeOcclusion()
{
    // numSamples is an upper limit fr the number of directions == number of rays
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
    // divide occlusion by the number of shot rays
    occ /= max( 1.0, float( 8 * hw - 2 ) );
    return occ;
}

//------------------------------------------------------------------------------
// Shade with spherical harmonics
#define VINE_STREET_KITCHEN
vec3 SphHarmShade()
{
  const float C1 = 0.429043;
  const float C2 = 0.511664;
  const float C3 = 0.743125;
  const float C4 = 0.886227;
  const float C5 = 0.247708;

#if defined( ST_PETER_BASILICA )
  // Constants for St. Peter's Basilica lighting OK - scaling 1.8
  const float scaling = 1.7;
  const vec3 L00  = vec3( 0.3623915,  0.2624130,  0.2326261);
  const vec3 L1m1 = vec3( 0.1759130,  0.1436267,  0.1260569);
  const vec3 L10  = vec3(-0.0247311, -0.0101253, -0.0010745);
  const vec3 L11  = vec3( 0.0346500,  0.0223184,  0.0101350);
  const vec3 L2m2 = vec3( 0.0198140,  0.0144073,  0.0043987);
  const vec3 L2m1 = vec3(-0.0469596, -0.0254485, -0.0117786);
  const vec3 L20  = vec3(-0.0898667, -0.0760911, -0.0740964);
  const vec3 L21  = vec3( 0.0050194,  0.0038841,  0.0001374);
  const vec3 L22  = vec3(-0.0818750, -0.0321501,  0.0033399);
#elif defined( GALILEOS_TOMB )
  // Constants for Galileo's tomb lighting - GOOD, scaling ~ 1.6 - 1.8
  const float scaling = 1.7;
  const vec3 L00  = vec3( 1.0351604,  0.7603549,  0.7074635);
  const vec3 L1m1 = vec3( 0.4442150,  0.3430402,  0.3403777);
  const vec3 L10  = vec3(-0.2247797, -0.1828517, -0.1705181);
  const vec3 L11  = vec3( 0.7110400,  0.5423169,  0.5587956);
  const vec3 L2m2 = vec3( 0.6430452,  0.4971454,  0.5156357);
  const vec3 L2m1 = vec3(-0.1150112, -0.0936603, -0.0839287);
  const vec3 L20  = vec3(-0.3742487, -0.2755962, -0.2875017);
  const vec3 L21  = vec3(-0.1694954, -0.1343096, -0.1335315);
  const vec3 L22  = vec3( 0.5515260,  0.4222179,  0.4162488);
#elif defined( VINE_STREET_KITCHEN )
  // Constants for Vine Street kitchen lighting - BEST, scaling < 1
  const float scaling = 0.8;
  const vec3 L00  = vec3( 0.6396604,  0.6740969,  0.7286833);
  const vec3 L1m1 = vec3( 0.2828940,  0.3159227,  0.3313502);
  const vec3 L10  = vec3( 0.4200835,  0.5994586,  0.7748295);
  const vec3 L11  = vec3(-0.0474917, -0.0372616, -0.0199377);
  const vec3 L2m2 = vec3(-0.0984616, -0.0765437, -0.0509038);
  const vec3 L2m1 = vec3( 0.2496256,  0.3935312,  0.5333141);
  const vec3 L20  = vec3( 0.3813504,  0.5424832,  0.7141644);
  const vec3 L21  = vec3( 0.0583734,  0.0066377, -0.0234326);
  const vec3 L22  = vec3(-0.0325933, -0.0239167, -0.0330796);
#elif defined( CAMPUS_SUNSET ) 
  // Constants for Campus Sunset lighting OK, scaling ~ 1
  const float scaling = 1.0;
  const vec3 L00  = vec3( 0.7870665,  0.9379944,  0.9799986);
  const vec3 L1m1 = vec3( 0.4376419,  0.5579443,  0.7024107);
  const vec3 L10  = vec3(-0.1020717, -0.1824865, -0.2749662);
  const vec3 L11  = vec3( 0.4543814,  0.3750162,  0.1968642);
  const vec3 L2m2 = vec3( 0.1841687,  0.1396696,  0.0491580);
  const vec3 L2m1 = vec3(-0.1417495, -0.2186370, -0.3132702);
  const vec3 L20  = vec3(-0.3890121, -0.4033574, -0.3639718);
  const vec3 L21  = vec3( 0.0872238,  0.0744587,  0.0353051);
  const vec3 L22  = vec3( 0.6662600,  0.6706794,  0.5246173);
#elif defined( FUNSTON_BEACH_SUNSET )
  const float scaling = 1.8;
  // Constants for Funston Beach Sunset lighting OK, scaling ~ 1.8
  const vec3 L00  = vec3( 0.6841148,  0.6929004,  0.7069543);
  const vec3 L1m1 = vec3( 0.3173355,  0.3694407,  0.4406839);
  const vec3 L10  = vec3(-0.1747193, -0.1737154, -0.1657420);
  const vec3 L11  = vec3(-0.4496467, -0.4155184, -0.3416573);
  const vec3 L2m2 = vec3(-0.1690202, -0.1703022, -0.1525870);
  const vec3 L2m1 = vec3(-0.0837808, -0.0940454, -0.1027518);
  const vec3 L20  = vec3(-0.0319670, -0.0214051, -0.0147691);
  const vec3 L21  = vec3( 0.1641816,  0.1377558,  0.1010403);
  const vec3 L22  = vec3( 0.3697189,  0.3097930,  0.2029923);
#endif
  
  vec3 tnorm  = normal;
  
  vec3 DiffuseColor = C1 * L22 * (tnorm.x * tnorm.x - tnorm.y * tnorm.y) +
                      C3 * L20 * tnorm.z * tnorm.z +
                      C4 * L00 -
                      C5 * L20 +
                      2.0 * C1 * L2m2 * tnorm.x * tnorm.y +
                      2.0 * C1 * L21  * tnorm.x * tnorm.z +
                      2.0 * C1 * L2m1 * tnorm.y * tnorm.z +
                      2.0 * C2 * L11  * tnorm.x +
                      2.0 * C2 * L1m1 * tnorm.y +
                      2.0 * C2 * L10  * tnorm.z;

  DiffuseColor *= scaling * vec3( gl_FrontMaterial.diffuse );

  return /*clamp( vec3( 0., 0., 0. ), vec3( 1., 1., 1. ),*/ DiffuseColor;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void main()
{
    ComputeRadiusAndOcclusionAttenuationCoeff();
#ifdef MRT_ENABLED
    normal = texture2DRect( normals, gl_FragCoord.xy ).xyz;
    worldPosition = texture2DRect( positions, gl_FragCoord.xy ).xyz;
#endif
// screen space occlusion is computed by accumulating the occlusion obtained
// by intersecting a number of rays with the surrounding geometry;
// the ray starting point is the current fragment and the directions are computed by
// iterating over coordinates along the edges of a square matrix

    // (-i_max,j_max)   (-i_max+1,j_max) ...  (i_max,j_max)
    // (-i_max,j_max-1)         .             (i_max,j_max-1) 
    //       .                  .                   .  
    //       .                  .                   .
    //       .                  .                   .
    // (-i_max,-j_max)  (-i_max+1,-j_max) ... (i_max,-j_max)

    // (i,j) indices are then transformed into angular coefficients assigned to rays 
#if defined( AO_LAMBERT )  // lambert shading
  gl_FragColor.rgb = gl_FrontMaterial.diffuse.rgb * dot( normal, -normalize( worldPosition ) );
  gl_FragColor.a = gl_FrontMaterial.diffuse.a;
#elif defined( AO_FLAT ) // color only
  gl_FragColor = gl_FrontMaterial.diffuse;
#elif defined( AO_SPHERICAL_HARMONICS )
  gl_FragColor.rgb = SphHarmShade();
  gl_FragColor.a =  gl_FrontMaterial.diffuse.a;
#else // ambient occlusion only
  gl_FragColor = vec4(1.0);
#endif
  if( ssao > 0 )
  {
    // set screen position for further usage in ambient occlusion computation
    screenPosition = gl_FragCoord.xyz;
    // multiply the color intensity by 1 - occlusion
    gl_FragColor.rgb *= 1.0 - smoothstep( 0.0, 1.0, ComputeOcclusion() * occlusionFactor );   
  }

#ifdef TEXTURE_ENABLED
  if( textureUnit >= 0 && bool( textureEnabled ) ) gl_FragColor *= texture2D( tex, gl_TexCoord[ textureUnit ].st );
#endif
  
#if defined( AO_SPHERICAL_HARMONICS )
  gl_FragColor += 0.1 * gl_FrontMaterial.ambient;    
#endif
  
}
  


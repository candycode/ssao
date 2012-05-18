// computes average kernel width and step multiplier from passed radius

uniform float radius; // object or scene radius 
uniform float dstep; // percentage of radius to use as convolution step multiplier (0.02)
uniform float dhwidth; // percentage of radius used as width of convolution kernel (0.1)
uniform float stepMax; // max allowed step size multiplier (2 - 10)
uniform float hwMax; // max allowe kernel window half size in pixels (8)
uniform float hwMin; // min allowed kernel window half size in pixels (1) 
uniform float stepMin; // min allowed step multiplier (1)
uniform vec2 viewport; //viewport    

uniform int ssao;

varying float samplingStep;
varying float halfWidth;

varying vec4 color;
varying vec3 normal;

float width = viewport.x; 
float height = viewport.y;

void main()
{
    color = gl_Color;
    normal = gl_NormalMatrix * gl_Normal;
    
    //////
    vec4 wpos = gl_ModelViewMatrix * gl_Vertex;
    vec4 projpos = gl_ProjectionMatrix * wpos;  
    gl_Position = projpos;
    if( bool( ssao ) )
    {
		vec4 wrad = wpos + vec4( clamp( dhwidth, 1.0, 128.0) * radius * wpos.w, 0., 0., 0. );
		vec4 projrad = gl_ProjectionMatrix * wrad;
		projpos.xyz /= projpos.w;
		projrad.xyz /= projrad.w;
		projpos.xyz *= 0.5;
		projrad.xyz *= 0.5;
		projpos.xyz += 0.5;
		projrad.xyz += 0.5; 
		projpos.x *= width;
		projpos.y *= height;
		projrad.x *= width;
		projrad.y *= height;
   		float dhw = distance( vec3( projpos.xy, 0.0 ), vec3( projrad.xy, 0.0 ) );
   		halfWidth = clamp( dhw, hwMin, hwMax );
   		samplingStep = clamp( dstep * halfWidth, stepMin, stepMax );
    /////
    }
    
	/*vec4 c = gl_ModelViewProjectionMatrix * vec4( 0., 0., -radius, 1. );
	c.xyz /= c.w;
	c.xy = 0.5 * c.xy + vec2( 0.5, 0.5 );
	c.xy *= vec2( width, height );
	
	 
	vec4 step   = gl_ModelViewProjectionMatrix * vec4( radius * dstep, 0., -radius, 1. );
	vec4 hwidth = gl_ModelViewProjectionMatrix * vec4( radius * dhwidth, 0., -radius, 1. );  
	step.xyz /= step.w;
    hwidth.xyz /= hwidth.w;
    step.xy = step.xy + vec2( 1.0, 1.0 );
    hwidth.xy = hwidth.xy + vec2( 1.0, 1.0 );
    step.xy   *= 0.5;
    hwidth.xy *= 0.5;
  	step.xy *= vec2( width, height );
  	hwidth.xy *= vec2( width, height ); 
  	
  	float ds  = distance( vec3( step.xy, 1.0 ), vec3( c.xy, 1.0 ) );
  	//float dhw = distance( vec3( hwidth.xy, 1.0 ), vec3( c.xy, 1.0 ) );
  	
  	//samplingStep = clamp( ds, stepMin, stepMax );
  	//halfWidth = clamp( dhw, hwMin, hwMax );  
    gl_Position = ftransform();*/
}


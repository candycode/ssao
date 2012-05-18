uniform float radius;
uniform float dstep;
uniform float dhwidth;
uniform float stepMax;
uniform float hwMax;

varying float samplingStep;
varying float halfWidth;

varying vec4 color;
varying vec3 normal;

//TEMPORARY
float width = 1024.0;
float height = 768.0;

void main()
{
    color = gl_Color;
    normal = gl_NormalMatrix * gl_Normal;

	vec4 c = gl_ModelViewProjectionMatrix * vec4( 0., 0., -radius, 1. );
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
  	float dhw = distance( vec3( hwidth.xy, 1.0 ), vec3( c.xy, 1.0 ) );
  	
  	samplingStep = clamp( ds, 1.0, stepMax );
  	halfWidth = clamp( dhw, 1.0, hwMax );
  
    gl_Position = ftransform();
    
}


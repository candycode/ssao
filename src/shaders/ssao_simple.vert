varying vec4 color;
varying vec3 N;

void main()
{
    color = gl_Color;
    N = normalize( gl_NormalMatrix * gl_Normal );
    gl_Position = ftransform();
}


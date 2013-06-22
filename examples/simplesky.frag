#version 130

uniform float time;

in vec3 ray;

void main(void){
    vec3 r = normalize(ray);
    const vec3 a = vec3( 0.8, 0.8, 0.9 );
    const vec3 b = vec3( 0.4, 0.4, 1.0 );
    float t = sqrt(abs(r.y));
    gl_FragColor = vec4( mix(a,b,t), 1. );
}

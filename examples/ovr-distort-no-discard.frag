#version 130

uniform sampler2D texSrc;
uniform vec2 lensCenter;
uniform vec2 screenCenter;
uniform vec2 scale;
uniform vec2 scaleIn;
uniform vec4 distortK;

in vec2 v_uv;

// Shader with just lens distortion correction.
void main(void){
    // Scales input texture coordinates for distortion.
    // ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
    // larger due to aspect ratio.
    vec2  theta = (v_uv - lensCenter) * scaleIn; // Scales to [-1, 1]
    float r2 = theta.x * theta.x + theta.y * theta.y;
    float distort = distortK.x + (distortK.y + (distortK.z + distortK.w * r2) * r2) * r2;
    vec2  uv = lensCenter + theta * (distort * scale);

    /*
    vec2 bound = vec2(0.25,0.5);
    if(any(lessThan(uv, screenCenter-bound))) discard;
    if(any(greaterThan(uv, screenCenter+bound))) discard;
    */

    gl_FragColor = texture2D( texSrc, uv );
}

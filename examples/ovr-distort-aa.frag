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

    vec2 bound = vec2(0.25,0.5);
    if(any(lessThan(uv, screenCenter-bound))) discard;
    if(any(greaterThan(uv, screenCenter+bound))) discard;

    // Antialias
    float dscale = 0.5;
    float friends = 0.5;
    vec2 d = dscale * (dFdx(uv) + dFdy(uv));
    vec4 box = vec4(uv-d, uv+d);
    vec4 c = texture2D( texSrc, box.xy )
           + texture2D( texSrc, box.zw )
           + texture2D( texSrc, box.xw )
           + texture2D( texSrc, box.zy );

    gl_FragColor = (friends * c + texture2D( texSrc, uv )) / (1. + 4.*friends);

    //gl_FragColor = texture2D( texSrc, uv );
}

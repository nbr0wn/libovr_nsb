#version 130

uniform sampler2D texSrc;
uniform vec2 lensCenter;
uniform vec2 screenCenter;
uniform vec2 scale;
uniform vec2 scaleIn;
uniform vec4 distortK;
uniform vec4 chromaK;

in vec2 v_uv;

// Shader with just lens distortion correction.
void main(){
    // Scales input texture coordinates for distortion.
    // ScaleIn maps texture coordinates to Scales to ([-1, 1]), although top/bottom will be
    // larger due to aspect ratio.
    vec2  theta = (v_uv - lensCenter) * scaleIn; // Scales to [-1, 1]
    float r2 = theta.x * theta.x + theta.y * theta.y;
    float distort = distortK.x + (distortK.y + (distortK.z + distortK.w * r2) * r2) * r2;
    vec2  theta1 = theta * (distort * scale);

    // Detect whether blue texture coordinates are out of range since these will scaled out the furthest.\n"
    vec2 uvBlue = lensCenter + theta1 * (chromaK.z + chromaK.w * r2);

    vec2 bound = vec2(0.25,0.5);
    if(any(lessThan(uvBlue, screenCenter-bound))) discard;
    if(any(greaterThan(uvBlue, screenCenter+bound))) discard;

    // Now do blue texture lookup.
    float blue = texture2D( texSrc, uvBlue ).b;

    // Do green lookup (no scaling).
    vec2 uvGreen = lensCenter + theta1;
    //float green = texture2D( texSrc, uvGreen ).g;

    // Antialias
    float dscale = 0.5;
    float friends = 0.5;
    vec2 d = dscale * (dFdx(uvGreen) + dFdy(uvGreen));
    vec4 box = vec4(uvGreen-d, uvGreen+d);
    float c = texture2D( texSrc, box.xy ).g
            + texture2D( texSrc, box.zw ).g
            + texture2D( texSrc, box.xw ).g
            + texture2D( texSrc, box.zy ).g;
    float green = (friends * c + texture2D( texSrc, uvGreen ).g) / (1. + 4.*friends);


    // Do red scale and lookup.
    vec2 uvRed = lensCenter + theta1 * (chromaK.x + chromaK.y * r2);
    float red = texture2D( texSrc, uvRed ).r;

    gl_FragColor = vec4( red, green, blue, 1 );
}


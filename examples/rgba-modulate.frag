// Very basic rgba-texture modulated by vertex rgba -- no lighting

uniform sampler2D rgbaMap;

varying vec4 v_rgba;
varying vec2 v_uv;

void main(void){
  vec4 c = texture2D( rgbaMap, v_uv );
  gl_FragColor = vec4( c * v_rgba );
}


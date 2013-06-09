attribute vec4 point;
attribute vec2 uv01;

uniform mat4 view;
uniform mat4 texm;

varying vec2 v_uv;

void main(void){
  v_uv = (texm * vec4(uv01,0.,1.)).st;
  gl_Position = view * point;
}

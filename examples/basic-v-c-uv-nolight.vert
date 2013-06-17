#version 130

/* Basic vertex shader.
 * Transforms point, and interpolates rgba and uv.
 * No lighting!
 * Good for trails/tracers/effects -- self-luminant, varying rgba/uv.
 */

uniform mat4 modelViewProjMtx;

in vec4 point;
in vec4 rgba;
in vec2 uv01;

out vec4 v_rgba;
out vec2 v_uv;

void main(void){
  v_rgba = rgba;
  v_uv = uv01;
  gl_Position = modelViewProjMtx * point;
}

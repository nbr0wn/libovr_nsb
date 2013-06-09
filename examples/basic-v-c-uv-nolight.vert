/* Basic vertex shader.
 * Transforms point, and interpolates rgba and uv.
 * No lighting!
 * Good for trails/tracers/effects -- self-luminant, varying rgba/uv.
 */

uniform mat4 modelViewProjMtx;

// Input
attribute vec4 point;
attribute vec4 rgba;
attribute vec2 uv01;

// Output
varying vec4 v_rgba;
varying vec2 v_uv;

void main(void){
  v_rgba = rgba;
  v_uv = uv01;
  gl_Position = modelViewProjMtx * point;
}

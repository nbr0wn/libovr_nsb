#version 120

/* Given lights in viewspace, eye in worldspace...
 * This shader outputs vectors oriented in viewspace, with
 * position relative to the surface point -- useful for lighting.
 */

uniform mat4 modelViewProjMtx; // per model
uniform mat4 modelViewMtx; // per model
uniform vec3 lightPos;     // in viewspace; set per scene (or per obj, prioritized)
uniform vec3 lightPos2;    // as above
uniform vec3 eyePos;       // per scene

// Input
attribute vec4 point;
attribute vec3 normal;
attribute vec2 uv01;

// Output
varying vec3 v_n;
varying vec2 v_uv;
varying vec3 v_eye;
varying vec3 v_light1;
varying vec3 v_light2;

void main(void){
  // view-space orientation, with origin at point
  vec3 p = (modelViewMtx * point).xyz;
  v_light1 = lightPos - p;
  v_light2 = lightPos2 - p;
  v_eye = -p;
  v_n = mat3(modelViewMtx) * normal;
  v_uv = uv01;
  gl_Position = modelViewProjMtx * point;
}


#version 120

uniform float time;

in vec3 ray;

mat3 m = mat3( 0.00,  0.80,  0.60,
              -0.80,  0.36, -0.48,
              -0.60, -0.48,  0.64 );

float hash( float n ){
    return fract(sin(n)*43758.5453);
}

float noise( in vec3 x ){
    vec3 p = floor(x);
    vec3 f = fract(x);

    f = f*f*(3.0-2.0*f);

    float n = p.x + p.y*57.0 + 113.0*p.z;

    float res = mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
                        mix( hash(n+ 57.0), hash(n+ 58.0),f.x),f.y),
                    mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                        mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
    return res;
}

float fbm( vec3 p ){
    float f;
    f  = 0.50*noise( p ); p = m*p*2.02;
    f += 0.25*noise( p ); p = m*p*2.03;
    f += 0.15*noise( p ); p = m*p*2.01;
    f += 0.08*noise( p );
    return f;
}

vec4 map( in vec3 p ){
	float d = 2.2 - 2.8 * fbm( p*1.3 - vec3(0.07,0.03,0.0)*time );
	d*= sqrt(noise( p + vec3(0.02,0.01,0.03)*time ));
	d = clamp( d, 0.0, 1.0 );
	vec4 res = vec4( d );
	res.xyz = mix( vec3(0.3,0.3,0.5), vec3(1.,1.,1.), d );
	return res;
}

void main(void){
  const vec3 horizoncolor = vec3( 0.9, 0.8, 0.8 );
  const vec3 suncolor = vec3( 2.0, 1.8, 1.4 );
  const vec3 skycolor = vec3( 0.6, 0.6, 1.0 );
  const vec3 sun = vec3( 0.707,0.,-0.707 );

  vec3 r = normalize(ray);
  float d = 0.5 * (dot( r, sun ) + 1.);
  vec3 sunlight = mix(skycolor,suncolor,pow(d,8.));
  vec3 hazecolor = horizoncolor * sunlight;

  vec3 col;
  const float limit = 0.08;
  if( r.y > limit ){
    float haze = ((1.-r.y)/(1.-limit));
    vec3 p = ray / ray.y;
    col = mix( map(p).rgb, hazecolor, haze );
  }else{
    col = hazecolor;
  }
  gl_FragColor = vec4( col, 1.0 );
}



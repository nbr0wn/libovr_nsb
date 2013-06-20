#ifndef _GLSTEREO_H_
#define _GLSTEREO_H_

#include <libovr_nsb/OVR.h>
#include "gltools.h"


typedef void (*render_fn)( mat4_t, mat4_t, void* );

enum DistortKind {
    DISTORT_CUSTOM,
    DISTORT_RADIAL,
    DISTORT_RADIAL_CHROMA
};

typedef struct {
   Device *dev;       // HMD device
   View backbuffer;   // Texture-target for rendering which will be distorted
   View framebuffer;  // Displayed framebuffer
   double proj[16];   // projection matrix
   enum DistortKind distort;
   GLuint shader;     // post-process shader (distortion)
} GLStereo;


GLStereo *glStereoCreate( Device *dev, int rendw, int rendh, enum DistortKind distort );
GLStereo *glStereoCreateCustom( Device *dev, int rendw, int rendh, GLuint distortShader );
void      glStereoRender( GLStereo *sr, double pos[3], render_fn render );
void      glStereoDestroy( GLStereo *sr );

void      glStereoSetDistort( GLStereo *sr, enum DistortKind distort );
void      glStereoSetDistortCustom( GLStereo *sr, GLuint shader );


#endif

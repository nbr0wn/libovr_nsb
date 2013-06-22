#ifndef _GLTOOLS_H_
#define _GLTOOLS_H_

#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/freeglut.h>

// COUNTED_ARRAY and BYTE_ARRAY are useful macros to pass statically sized
// arrays to functions which have a count followed by the array pointer.

#define COUNTED_ARRAY(a) (sizeof(a) / sizeof(a[0])), (a)
#define BYTE_ARRAY(a) (sizeof(a)), (a)


/* ===== Shaders ======================================= */

/* Usage:

   // declare program sources -- an array of ShaderSource
   ShaderSource shdrSimple[] = {
       {GL_VERTEX_SHADER,   "basic-v-uv-nolight.vert"},
       {GL_FRAGMENT_SHADER, "rgba-modulate.frag"}
   };

   // load a program
   Gluint prog = shaderProgram( COUNTED_ARRAY(shdrSimple) );

   // set current program for rendering
   glUseProgram( prog );

   // when it's no longer needed (shutdown)
   glDeleteProgram( prog );
*/

typedef struct {
    GLuint  type;  // GL_VERTEX_SHADER; TESS_CONTROL, TESS_EVALUATION, GEOMETRY, FRAGMENT
    char   *fname; // file-name to load shader from
} ShaderSource;

GLuint shaderCompile( GLuint type, char *name, char *source );
GLuint shaderLoadAndCompile( ShaderSource *s );
GLuint shaderLink( int shaders, GLuint shader[] );
GLuint shaderProgram( int shaders, ShaderSource src[] );



/* ===== Vertices ====================================== */

/* Usage:

   // New vertex format
   VDesc desc =
    { {"point",  3},
      {"normal", 3},
      {"uv01",   2} };
   VFormat v3n3t2 = vtxNewFormat( prog, COUNTED_ARRAY(desc) );

   // uploading vertex data
   GLuint vbo = newBufObj( GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW );

   // prep for vertex format (must match current shader too... hmm)
   vtxEnable( &v3n3t2 );

   // using vbo
   glBindBuffer( GL_ARRAY_BUFFER, vbo );
*/

#define MAX_ATTRIBS_PER_VERTEX 8

typedef struct { const char *attrName; int elems; } VDesc;

typedef struct { GLint idx; GLuint elems; GLuint offs; } VAttrib;

typedef struct {
    int attribs;
    GLuint stride;
    VAttrib attr[MAX_ATTRIBS_PER_VERTEX];
} VFormat;

VFormat vtxNewFormat( GLuint prog, int count, VDesc desc[] );
void    vtxEnable( VFormat *fmt );
void    vtxDisable( VFormat *fmt );



/* ===== Textures ====================================== */

#define TEX_BYTE1  GL_RED8, GL_RED, GL_UNSIGNED_BYTE
#define TEX_BYTE2  GL_RG8, GL_RG, GL_UNSIGNED_BYTE
#define TEX_BYTE3  GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE
#define TEX_BYTE4  GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE
#define TEX_SHORT1 GL_RED16, GL_RED, GL_UNSIGNED_SHORT
#define TEX_SHORT2 GL_RG16, GL_RG, GL_UNSIGNED_SHORT
#define TEX_SHORT3 GL_RGB16, GL_RGB, GL_UNSIGNED_SHORT
#define TEX_SHORT4 GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT
#define TEX_FLOAT1 GL_RED32F, GL_RED, GL_FLOAT
#define TEX_FLOAT2 GL_RG32F, GL_RG, GL_FLOAT
#define TEX_FLOAT3 GL_RGB32F, GL_RGB, GL_FLOAT
#define TEX_FLOAT4 GL_RGBA32F, GL_RGBA, GL_FLOAT

#define TEX_MIRROR GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT
#define TEX_REPEAT GL_REPEAT, GL_REPEAT
#define TEX_CLAMP  GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE

GLuint texCreate( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data );
GLuint texCreateMip( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data );
GLuint texCreateTarget( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT );
void   texBind( GLint n, GLuint uniformLoc, GLuint tex );
void   texUnbind( GLint n );



/* ===== Viewport / Framebuffer ======================== */

typedef struct {
    int x0;
    int y0;
    int wid;
    int hgt;
    int offscreen; // if true, the following three point to the offscreen buffer
    GLuint rgba;
    GLuint depth;
    GLuint fbo;
} View;

void viewSetFrameBuffer( View *v, GLsizei w, GLsizei h );
void viewSetOffscreenBuffer( View *v, GLint filter, GLsizei w, GLsizei h );
void viewFreeOffscreenBuffer( View *v );



/* ===== Misc ========================================== */

GLuint newBufObj( GLenum target, GLsizei size, const void *data, GLenum kind );

GLuint renderbufferNew( GLenum component, GLsizei w, GLsizei h );

GLuint fboNew( GLuint texture );
int    fboBind( GLuint fbo );
void   fboUnbind( void );
GLuint fboCreate( GLuint texture );
GLuint fboCreateWithDepth( GLuint texture, GLuint depth );

#endif

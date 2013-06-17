#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/freeglut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#include <libovr_nsb/OVR.h>

// TODO
//  |render setup (transforms, view, uniforms, etc)
//  |texture params
//  |backbuffer
//  |stereo
//  |distortion
//  |add 'discard' to distort shaders
//  /add movement -- probably best to leave this for a demo using SDL
//  -create shapes
//    -fractal terrain
//  -aspect has a problem -- a 2/3 ratio is required where it should be 1/2
//  -set Distortion struct from HMD info
//  -fix resize
//  -rather than GetAttribLocation, use BindAttribLocation (do before linking)
//  -full implementation of stereo.c
//  -breakout GL stuff into gltools.c?
//  -properly clean-up resources (buffers, textures, mallocs)
//  -fix any remaining FIXME's and TODO's; check XXX's.


void idleFunc( );
void displayFunc( );
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );
void initGL( );
void initialize( );


/* ---------------------------------------------------------------- */


#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#define COUNTED_ARRAY(a) (NELEMS(a)), (a)
#define BYTE_ARRAY(a) (sizeof(a)), (a)

#define PRINT_GL_ERROR() printOGLError(__FILE__, __LINE__)

void printOGLError( char *file, int line ){
   GLenum err = glGetError();
   if( err != GL_NO_ERROR )
      printf( "glError in file %s, line %d: %s (0x%08x)\n", file, line, gluErrorString(err), err );
}


void oglMatrix( mat4_t m, float *f ){
   int i;
   for( i=0;i<16;i++ ) f[i] = m[i];
}

void printMatrix( float *m ){
   int i;
   for( i=0; i<4; i++ ){
      float *n = m+i*4;
      printf( "%6.4f %6.4f %6.4f %6.4f\n", n[0], n[1], n[2], n[3] );
   }
}

void showUniforms( GLuint prog ){
   int total = -1;
   glGetProgramiv( prog, GL_ACTIVE_UNIFORMS, &total ); 
   //printf( "Uniforms for program %d:\n" );
   int i;
   char name[128];
   for( i=0; i<total; i++ ){
      int nameLen, elems;
      GLenum type = GL_ZERO;
      glGetActiveUniform( prog, (GLuint)i, sizeof(name)-1, &nameLen, &elems, &type, name );
      if( type != GL_ZERO ){
         name[nameLen] = 0;
         GLuint location = glGetUniformLocation( prog, name );
         printf(" location %2d: '%s' type=0x%04x[%d]\n", location, name, type, elems );
      }
   }
}


// used for shader loading
char *loadText( char *fname ){
   char *text = NULL;
   FILE *fp = fopen( fname, "r" );
   if( fp ){
      fseek( fp, 0, SEEK_END );
      int size = ftell( fp );
      fseek( fp, 0, SEEK_SET );
      text = malloc( size+1 );
      fread( text, 1, size, fp );
      fclose( fp );
      text[size] = 0;  // null-terminate string
   }else{
      printf( "Error opening '%s'.\n", fname );
   }
   return text;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// GL Shader Load, Compile, Link
/////////////////////////////////////////////////////////////////////////////////////////////

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


GLuint shaderCompile( GLuint type, char *name, int n, char *source[] ){
   /* Create shader and compile it */
   GLuint shader = glCreateShader( type );
   glShaderSource( shader, n, source, NULL );
   glCompileShader( shader );

   /* Report error and return zero if compile failed */
   GLint compiled;
   glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
   if( !compiled ){
      GLint length;
      glGetShaderiv( shader, GL_INFO_LOG_LENGTH, &length );
      GLchar *log = malloc(length);
      glGetShaderInfoLog( shader, length, &length, log );
      printf( "Failed to compile shader '%s'...\n%s\n", name, log );
      free( log );
      glDeleteShader( shader );
      shader = 0;
   }
   return shader;
}

GLuint shaderLoadAndCompile( ShaderSource *s ){
   GLuint shader = 0;
   GLchar *text = loadText( s->fname );
   if( text ){
      shader = shaderCompile( s->type, s->fname, 1, &text );
      free( text );
   }
   return shader;
}

GLuint shaderProgram( int shaders, ShaderSource src[] ) {
   int i;
   GLuint program = glCreateProgram();

   for( i=0; i<shaders; i++ ){
      GLuint shader = shaderLoadAndCompile( &src[i] );
      glAttachShader( program, shader );
      glDeleteShader( shader );  // NOTE only flags shader for deletion when unreferenced
   }

   glLinkProgram( program );

   /* Report error and return zero if link failed */
   GLint linked;
   glGetProgramiv( program, GL_LINK_STATUS, &linked );
   if( !linked ){
      GLint length;
      glGetProgramiv( program, GL_INFO_LOG_LENGTH, &length );
      GLchar *log = malloc(length);
      glGetProgramInfoLog( program, length, &length, log );
      printf( "Failed to link program...\n%s\n", log );
      free( log );
      glDeleteProgram( program );
      program = 0;
   }

   return program;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// GL Buffer Object
/////////////////////////////////////////////////////////////////////////////////////////////

GLuint newBufObj( GLenum target, GLsizei size, const void *data, GLenum kind ){
   GLuint id;
   glGenBuffers( 1, &id );
   glBindBuffer( target, id );
   glBufferData( target, size, data, kind );
   glBindBuffer( target, 0 );
   return id;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// GL Vertex Attributes
/////////////////////////////////////////////////////////////////////////////////////////////

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


VFormat vtxNewFormat( GLuint prog, int count, VDesc desc[] ){
   VFormat fmt;

   int i;
   int stride = 0;
   for( i=0; i<count; i++ ){
      int elems = desc[i].elems;
      GLint idx = glGetAttribLocation( prog, desc[i].attrName );
      if( idx < 0 ) printf("vtxNewFormat (warning): Attribute '%s' wasn't found in shader program.\n", desc[i].attrName );
      fmt.attr[i].idx = idx;
      fmt.attr[i].elems = elems;
      fmt.attr[i].offs = stride;
      stride+= elems*4;
   }

   fmt.attribs = count;
   fmt.stride = stride;
   return fmt;
}

void vtxEnable( VFormat *fmt ){
   int i;
   VAttrib *a = fmt->attr;
   for( i=0; i<fmt->attribs; i++ ){
      glEnableVertexAttribArray( a[i].idx );
      glVertexAttribPointer( a[i].idx, a[i].elems, GL_FLOAT, GL_FALSE, fmt->stride, a[i].offs );
   }
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Textures, Framebuffers, and Renderbuffers
/////////////////////////////////////////////////////////////////////////////////////////////

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

// internal function
GLuint texNew( GLint filter, GLint wrapS, GLint wrapT ){
   GLuint idx;
   glGenTextures( 1, &idx );
   glBindTexture( GL_TEXTURE_2D, idx );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
   return idx;
}

GLuint texCreate( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data ){
   GLuint idx = texNew( filter, wrapS, wrapT );
   glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
   glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, data );
   glBindTexture( GL_TEXTURE_2D, 0 );
   return idx;
}

// exactly like texLoad, but no data... useful for generating a render target
GLuint texCreateTarget( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT ){
   GLuint idx = texNew( filter, wrapS, wrapT );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
   glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, 0 );
   glBindTexture( GL_TEXTURE_2D, 0 );
   return idx;
}

//delete: glDeleteTextures( n, array );

//(* Binds texture unit 'n' to use 'tex_id' as source, and feeding
// * the current shader via 'uniform_location'. *)
void texBind( GLint n, GLuint uniformLoc, GLuint tex ){
   glActiveTexture( GL_TEXTURE0+n );
   glUniform1i( uniformLoc, n );
   glBindTexture( GL_TEXTURE_2D, tex );
}

void texUnbind( GLint n ){
   glActiveTexture( GL_TEXTURE0+n );
   glBindTexture( GL_TEXTURE_2D, 0 );
}


// RenderBuffer
GLuint rbNew( GLenum component, GLsizei w, GLsizei h ){
   GLuint rb;
   glGenRenderbuffers( 1, &rb );
   glBindRenderbuffer( GL_RENDERBUFFER, rb );
   glRenderbufferStorage( GL_RENDERBUFFER, component, w, h );
   glBindRenderbuffer( GL_RENDERBUFFER, 0 );
   return rb;
}

//delete: glDeleteRenderbuffers( n, array );

// FrameBuffer Object
GLuint fboNew( GLuint texture ){
  GLuint fbo;
  glGenFramebuffers( 1, &fbo );
  glBindFramebuffer( GL_FRAMEBUFFER, fbo );
  glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0 );
  return fbo;
}

// returns true if fbo is good
int fboBind( GLuint fbo ){
  glBindFramebuffer( GL_FRAMEBUFFER, fbo );
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void fboUnbind( void ){ glBindFramebuffer( GL_FRAMEBUFFER, 0 ); }

GLuint fboCreate( GLuint texture ){
  GLuint fbo = fboNew( texture );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );
  return fbo;
}

GLuint fboCreateWithDepth( GLuint texture, GLuint depth ){
  GLuint fbo = fboNew( texture );
  glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );
  return fbo;
}

//delete: glDeleteFramebuffers( n, array );


/////////////////////////////////////////////////////////////////////////////////////////////
// Renderable
/////////////////////////////////////////////////////////////////////////////////////////////

// A basic "renderable" type referring to vertices and one texture
typedef struct {
   VFormat *fmt;   // vertex format
   GLuint   prog;  // shader program to use for render
   GLuint   vao;   // vertex array object -- a convenient handle used to bind up all attribs
   GLuint   vbo;   // vertex buffer -- handle to GL buffer of vertex data
   GLuint   ibo;   // index buffer
   GLuint   tex;   // texture (zero if none)
   GLuint   u_tex; // texture uniform index -- for binding to shader
   GLenum   mode;  // GL_TRIANGLES, GL_TRIANGLE_STRIP, etc
   GLsizei  count; // number of vertex indices describing this shape
   GLenum   type;  // data format of indices (eg. GL_UNSIGNED_SHORT)
} Renderable;

// Simple Renderable instance list, with matrix.
typedef struct RList_s RList;
struct RList_s {
   double      mtx[16]; // mat4_t
   Renderable *rend;
   RList      *next;
};


// Bind all attributes to the vertex array object (VAO)
void renderBindVao( Renderable *r ){
   glGenVertexArrays( 1, &r->vao );
   glBindVertexArray( r->vao );
   glBindBuffer( GL_ARRAY_BUFFER, r->vbo );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, r->ibo );
   vtxEnable( r->fmt );
   glBindVertexArray(0);
}

void renderableAddTex( Renderable *r, char *uname, GLuint tex ){
   r->tex = tex;
   r->u_tex = glGetUniformLocation( r->prog, uname );
}

void renderableStream( Renderable *r, int verts, float *vdata, int indices, GLushort *idata ){
   glBindBuffer( GL_ARRAY_BUFFER, r->vbo );
   glBufferSubData( GL_ARRAY_BUFFER, 0, verts * r->fmt->stride, vdata );
   glBindBuffer( GL_ARRAY_BUFFER, 0 );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, r->ibo );
   glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, indices*sizeof(GLushort), idata );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
   r->count = indices;
}

// Assumes GL_TRIANGLES, u16 indices, and GL_STATIC_DRAW, since that's typical
Renderable renderableCreate( GLuint prog, VFormat *fmt, GLsizei vbytes, GLfloat *vdata, GLsizei ibytes, GLushort *idata, GLenum usage ){
   Renderable r = {
      fmt,
      prog,
      0,
      newBufObj( GL_ARRAY_BUFFER, vbytes, vdata, usage ),
      newBufObj( GL_ELEMENT_ARRAY_BUFFER, ibytes, idata, usage ),
      0,
      0,
      GL_TRIANGLES,
      ibytes>>1,
      GL_UNSIGNED_SHORT
   };
   renderBindVao( &r );
   return r;
}

void renderObj( Renderable *r ){
   if( r->tex ) texBind( 0, r->u_tex, r->tex );
   glBindVertexArray( r->vao );
   glDrawElements( r->mode, r->count, r->type, 0 );
   if( r->tex ) texUnbind( 0 );
}


/////////////////////////////////////////////////////////////////////////////////////////////
// View (viewport) and offscreen buffer
/////////////////////////////////////////////////////////////////////////////////////////////

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

View frameBuffer( GLsizei w, GLsizei h ){
   return (View){ 0, 0, w, h, 0, 0, 0, 0 };
}

View offscreenBuffer( GLint filter, GLsizei w, GLsizei h ){
   GLuint rgba = texCreateTarget( TEX_BYTE4, w, h, filter, TEX_CLAMP );
   //GLuint rgba = texCreateTarget( filter, GL_MIRRORED_REPEAT, GL_MIRRORED_REPEAT, TEX_BYTE4, w, h );
   GLuint depth = rbNew( GL_DEPTH_COMPONENT, w, h );
   GLuint fbo = fboCreateWithDepth( rgba, depth );
   return (View){ 0, 0, w, h, 1, rgba, depth, fbo };
}

void freeOffscreenBuffer( View *v ){
   glDeleteTextures( 1, &v->rgba );
   glDeleteRenderbuffers( 1, &v->depth );
   glDeleteFramebuffers( 1, &v->fbo );
}



/* ----------------------------------------------------------------------- */


/////////////////////////////////////////////////////////////////////////////////////////////
// Global State
/////////////////////////////////////////////////////////////////////////////////////////////


// width and height of the window
GLsizei g_width = 1280;
GLsizei g_height = 800;


// Oculus Rift Device
Device *dev;


View *g_backbuffer = NULL;   // Texture-target for rendering which will be distorted
View *g_framebuffer = NULL;  // Displayed framebuffer

RList *g_renderList = NULL;

double g_proj[16];   // projection matrix
double g_head[3] = { 0., 0., 0. }; // world-pos of base of head

// Inverse reference quaternion
// - this is the inverse of a reference orientation
// - intially set with a reference orientation of 90deg pitch which seems to
//   correspond to the Rift's natural orientation
double g_qref[4] = { -0.7071, 0., 0., 0.7071 };



// Shaders

// Specified by an array of pairs: shader type, filename.
// Multiple of the same type are fine.
//
// I was going to avoid re-compiling common shaders by caching compiled ID,
// but dealing with potentially deleted shaders complicates things for a
// simple demo... so duplicates will just be recompiled.
ShaderSource shdrNoLight[] = {
    {GL_VERTEX_SHADER,   "basic-v-c-uv-nolight.vert"},
    {GL_FRAGMENT_SHADER, "rgba-modulate.frag"}
};

ShaderSource shdrDistort[] = {
    {GL_VERTEX_SHADER,   "ovr-post.vert"},
    {GL_FRAGMENT_SHADER, "ovr-distort.frag"}
};

ShaderSource shdrDistortChroma[] = {
    {GL_VERTEX_SHADER,   "ovr-post.vert"},
    {GL_FRAGMENT_SHADER, "ovr-distortchroma.frag"}
};


// A Square

float squareVerts[] =
{  -1.0,  1.0,  0.0,  1.0, 1.0, 1.0, 1.0,  0., 0.,
    1.0,  1.0,  0.0,  1.0, 0.9, 0.8, 1.0,  1., 0.,
   -1.0, -1.0,  0.0,  0.7, 0.6, 1.0, 1.0,  0., 1.,
    1.0, -1.0,  0.0,  0.4, 0.2, 0.1, 1.0,  1., 1. };

GLushort squareIdx[] = { 0, 2, 1,  1, 2, 3 };



/////////////////////////////////////////////////////////////////////////////////////////////
// GL Init
/////////////////////////////////////////////////////////////////////////////////////////////
void initGL()
{
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    glDepthRange( 0.0f, 1.0f );
    glClearDepth( 1.0f );
    glDepthFunc( GL_LESS );
    glEnable( GL_DEPTH_TEST );

    glFrontFace( GL_CCW );
    /* FIXME -- no culling while building up some basic rendering
    glCullFace( GL_BACK );
    glEnable( GL_CULL_FACE );
    */
    glDisable( GL_CULL_FACE );

    glBlendEquation( GL_FUNC_ADD );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_BLEND );
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Handle window resizes
/////////////////////////////////////////////////////////////////////////////////////////////
void reshapeFunc( GLsizei w, GLsizei h )
{
   // FIXME -- need to redefine g_framebuffer and probably a few other things
    g_width = w; g_height = h;
    glViewport( 0, 0, w, h );
#if 0
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    gluLookAt( eye[0], eye[1], eye[2],
               0.0f, 0.0f, 0.0f, 
               0.0, 1.0, 0.0 );
#endif
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 27:  // Why the hell doesn't glut have GLUT_KEY_ESC?
        case 'q':
            exit( 0 );
            break;
        case ' ':
            if(dev){
               double qm[16];
               float m[16];
               printf("Q Matrix:\n");
               quat_toMat4( dev->Q, qm );
               oglMatrix( qm, m );
               printMatrix( m );
               printf("q: x= %6.3f y= %6.3f z= %6.3f w= %6.3f\n", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] );
               // set this rotation as reference
               // -NOTE that this generally results in off-kilter "ground
               //  plane" which can be nauseating -- should probably apply
               //  gravity sense from linear accelerometer.
               quat_set( dev->Q, g_qref );
               quat_conjugate( g_qref, NULL );
               quat_normalize( g_qref, NULL );
            }
            break;
        case 'w':
            break;
        case 'a':
            break;
        case 's':
            break;
        case 'd':
            break;
    }

    glutPostRedisplay( );
}



/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void mouseFunc( int button, int state, int x, int y )
{
    glutPostRedisplay( );
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}



/* ================ Imports from stereo.c =================== */

// TODO remove this once we use stereo.c

typedef struct {
    float K[4];
    float XCenterOffset, YCenterOffset;
    float Scale;
    float ChromaticAberration[4]; // Additional per-channel scaling is applied after distortion:
                                  // Index [0] - Red channel constant coefficient.
                                  // Index [1] - Red channel r^2 coefficient.
                                  // Index [2] - Blue channel constant coefficient.
                                  // Index [3] - Blue channel r^2 coefficient.
} Distortion;


Distortion g_defaultDistort = {
   { 1.0f, 0.22f, 0.24f, 0.0f },
   0., 0.,
   0.3,
   { 0.996f, -0.004f, 1.014f, 0.0f }
};

float distortionFn( Distortion *d, float r ){        
    float *k = d->K;
    float r2 = r * r;
    return r * (k[0] + (k[1] + (k[2] + k[3] * r2) * r2) * r2);
}

float stereoProjectionOffset( SensorDisplayInfo *hmd )
{
    // Post-projection viewport coordinates range from (-1.0, 1.0), with the
    // center of the left viewport falling at (1/4) of horizontal screen size.
    // We need to shift this projection center to match with the lens center;
    // note that we don't use the IPD here due to collimated light property of the lens.
    // We compute this shift in physical units (meters) to
    // correct for different screen sizes and then rescale to viewport coordinates.    
    float viewCenter         = hmd->HScreenSize * 0.25f;
    float eyeProjectionShift = viewCenter - hmd->LensSeparation*0.5f;
    return 4.0f * eyeProjectionShift / hmd->HScreenSize;
}

void distortionUpdateOffsetAndScale( Distortion *distort, SensorDisplayInfo *hmd, View *v, float x, float y )
{
    // Distortion center shift is stored separately, since it isn't affected
    // by the eye distance.
    float lensOffset        = hmd->LensSeparation * 0.5f;
    float lensShift         = hmd->HScreenSize * 0.25f - lensOffset;
    float lensViewportShift = 4.0f * lensShift / hmd->HScreenSize;
    distort->XCenterOffset  = lensViewportShift;

    // Compute distortion scale from DistortionFitX & DistortionFitY.
    // Fit value of 0.0 means "no fit".
    if ((fabs(x) < 0.0001f) &&  (fabs(y) < 0.0001f))
        distort->Scale = 1.0f;
    else{
        // Convert fit value to distortion-centered coordinates before fit radius
        // calculation.
        float stereoAspect = 0.5f * (float)(v->wid) / (float)(v->hgt);
        float dx           = x - distort->XCenterOffset;
        float dy           = y / stereoAspect;
        float fitRadius    = sqrt(dx * dx + dy * dy);
        distort->Scale     = distortionFn( distort, fitRadius ) / fitRadius;
    }
}

void projectionMatrix( View *vp, SensorDisplayInfo *hmd, Distortion *d, int stereo, mat4_t m ){
    double zNear = 0.01;
    double zFar  = 500.;

    double aspect = (double)(vp->wid) / (double)(vp->hgt);
    if( stereo ) aspect*= 0.65; // FIXME -- this should be 0.5... what's up?

    double fovy = 80.;
    if( stereo ){
       double percievedHalfRTDistance = 0.5 * hmd->VScreenSize * d->Scale;    
       fovy = 2. * atan( percievedHalfRTDistance / hmd->EyeToScreenDistance[0] /* FIXME... */ );
       // -- the above eyeToScreenDistance is per eye; we should ideally
       //    generate a separate projection matrix per eye to account for this
       fovy*= 180./M_PI;
    }

    mat4_perspective( fovy, aspect, zNear, zFar, m );
}

/* ========================================================== */


void defaultSensorDisplayInfo( SensorDisplayInfo *s ){
    memset( s, 0, sizeof(SensorDisplayInfo) );
    s->DistortionType = Base_Distortion;    
    s->HResolution = 1280;
    s->VResolution = 800;
    s->HScreenSize = 0.14976f;
    s->VScreenSize = s->HScreenSize * (float)(s->VResolution) / (float)(s->HResolution);
    s->VCenter = 0.5f;
    s->LensSeparation = 0.0635f;
    s->EyeToScreenDistance[0] = 0.041f;
    s->EyeToScreenDistance[1] = 0.041f;
    s->DistortionK[0] = 1.0f;
    s->DistortionK[1] = 0.22f;
    s->DistortionK[2] = 0.24f;
    s->DistortionK[3] = 0.0f;
}

// Useful when a Rift is not attached
SensorDisplayInfo defaultDisplayInfo;
SensorDisplayInfo *getDisplayInfo( Device *d ){
   if( d )
      return &d->sensorInfo;
   else
      return &defaultDisplayInfo;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Rendering
/////////////////////////////////////////////////////////////////////////////////////////////

void render( RList *r, mat4_t view, mat4_t proj ){
   double dMtx[16];
   float  fMtx[16];
   GLint u_mvp;
   GLint u_mv;
   GLuint prog = 0;

   while( r ){
      if( r->rend->prog != prog ){
         prog = r->rend->prog;
         glUseProgram( prog );

         // Uniforms must be set up anytime program changes...
         u_mvp = glGetUniformLocation( prog, "modelViewProjMtx" );
         u_mv  = glGetUniformLocation( prog, "modelViewMtx" );
      }

      mat4_multiply( view, r->mtx, dMtx );
      oglMatrix( dMtx, fMtx );
      glUniformMatrix4fv( u_mv, 1, GL_FALSE, fMtx );

      mat4_multiply( proj, dMtx, dMtx );
      oglMatrix( dMtx, fMtx );
      glUniformMatrix4fv( u_mvp, 1, GL_FALSE, fMtx );

      renderObj( r->rend );

      r = r->next;
   }
   glBindVertexArray( 0 ); // unbind any currently bound VAO

   glUseProgram(0);
}

void mapDistortion( GLuint src, Distortion *d, View *v ){
   static GLint u_texSrc, u_lensCenter, u_screenCenter, u_scale, u_scaleIn, u_distortK, u_chromaK;
   static GLint u_viewm, u_texm;
   static GLuint distortShader = 0;
   static GLuint vao = 0;
   static GLuint vbo = 0;
   static VDesc desc[] = { {"point", 3}, {"uv01", 2} };
   static VFormat v3t2; // vertex format only used for screen-quad
   static float screenVerts[] =
   {  0.0,  0.0,  0.0,   0., 0.,
      1.0,  0.0,  0.0,   1., 0.,
      0.0,  1.0,  0.0,   0., 1.,
      1.0,  1.0,  0.0,   1., 1. };

   // One-time setup, which could be better done as a distortion-mapping state
   if( !distortShader ){
      distortShader = shaderProgram( COUNTED_ARRAY(shdrDistortChroma) );
      v3t2 = vtxNewFormat( distortShader, COUNTED_ARRAY(desc) );
      vbo = newBufObj( GL_ARRAY_BUFFER, BYTE_ARRAY(screenVerts), GL_STATIC_DRAW );

      // distortion shader uniform locations
      u_texSrc = glGetUniformLocation( distortShader, "texSrc" );
      u_lensCenter = glGetUniformLocation( distortShader, "lensCenter" );
      u_screenCenter = glGetUniformLocation( distortShader, "screenCenter" );
      u_scale = glGetUniformLocation( distortShader, "scale" );
      u_scaleIn = glGetUniformLocation( distortShader, "scaleIn" );
      u_distortK = glGetUniformLocation( distortShader, "distortK" );
      u_chromaK = glGetUniformLocation( distortShader, "chromaK" );

      // for the vertex shader
      u_viewm = glGetUniformLocation( distortShader, "view" );
      u_texm  = glGetUniformLocation( distortShader, "texm" );

      // it seems VAOs aren now REQUIRED to render anything!? (OpenGL4.2 core?)
      // -- at least, no rendering seems to work without a VAO bound
      // -- so set one up for the screen quad...
      glGenVertexArrays( 1, &vao );
      glBindVertexArray( vao );
      glBindBuffer( GL_ARRAY_BUFFER, vbo );
      vtxEnable( &v3t2 );
      glBindVertexArray( 0 );

      //showUniforms( distortShader );
   }


   glUseProgram( distortShader );

   float w = (float)(v->wid) / (float)g_width;
   float h = (float)(v->hgt) / (float)g_height;
   float x = (float)(v->x0) / (float)g_width;
   float y = (float)(v->y0) / (float)g_height;
   float as = (float)(v->wid) / (float)(v->hgt);
   float scaleFactor = 1.0f / d->Scale;


   // Set distortion shader parameters
   // We are using 1/4 of DistortionCenter offset value here, since it is
   // relative to [-1,1] range that gets mapped to [0, 0.5].

   /* XXX Why don't these work!?
   glUniform2f( u_lensCenter, x+(w+d->XCenterOffset*0.5f)*0.5f, y+h*0.5f );
   glUniform2f( u_screenCenter, x+w*0.5f, y+h*0.5f );
   glUniform2f( u_scale, w*0.5f*scaleFactor, h*0.5f*scaleFactor );
   glUniform2f( u_scaleIn, 2.0f/w, 2.0f/(h*as) );
   glUniform4f( u_distortK, d->K[0], d->K[1], d->K[2], d->K[3] );
   */

   // glUniform*f functions weren't working -- uniforms would not be set;
   // reading back values would show garbage. *fv versions seem to work...
   float a[4];
   a[0] = x+(w+d->XCenterOffset*0.5f)*0.5f;
   a[1] = y+h*0.5f;
   glUniform2fv( u_lensCenter, 1, a );
   a[0] = x+w*0.5f;
   a[1] = y+h*0.5f;
   glUniform2fv( u_screenCenter, 1, a );
   a[0] = w*0.5f*scaleFactor;
   a[1] = h*0.5f*scaleFactor;
   glUniform2fv( u_scale, 1, a );
   a[0] = 2.0f/w;
   a[1] = 2.0f/(h*as);
   glUniform2fv( u_scaleIn, 1, a );
   glUniform4fv( u_distortK, 1, d->K );
   glUniform4fv( u_chromaK, 1, d->ChromaticAberration );


   float texm[16] = {
      w,  0., 0., 0.,
      0., h,  0., 0.,
      0., 0., 0., 0.,
      x,  y,  0., 1. };
   glUniformMatrix4fv( u_texm, 1, GL_FALSE, texm );

   float viewm[16] = {  // pre-transposed compared to OVR demos
      2., 0., 0., 0.,
      0., 2., 0., 0.,
      0., 0., 0., 0.,
     -1.,-1., 0., 1. };
   glUniformMatrix4fv( u_viewm, 1, GL_FALSE, viewm );


   // Render 

   glViewport( v->x0, v->y0, v->wid, v->hgt );

   // (distorted) copy to the framebuffer ignores depth and blending
   glDepthMask( GL_FALSE );
   glDisable( GL_BLEND );

   texBind( 0, u_texSrc, src );

   glBindVertexArray( vao );
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
   glBindVertexArray( 0 );

   texUnbind( 0 );

   glEnable( GL_BLEND );
   glDepthMask( GL_TRUE );

}

// generates the view matrix (inverse) from a worldspace vec+quat
void viewOfPosOrient( vec3_t p, quat_t q, mat4_t dst ){
   double qinv[4];
   double neg[3];
   vec3_negate( p, neg );
   quat_conjugate( q, qinv );
   quat_multiplyVec3( qinv, neg, NULL );
   mat4_fromRotationTranslation( qinv, neg, dst );
}

void displayFunc( )
{
   double eye[16];
   View *bb = g_backbuffer;
   View *fb = g_framebuffer;

   assert(fb); // we at least require the framebuffer to be defined

   // Orientation, from HMD sensor
   double orient[4];
   quat_set( dev->Q, orient );
   quat_multiply( orient, g_qref, NULL ); // apply inverse-reference orientation

   if( bb && bb->offscreen && fboBind(bb->fbo) ){
      /* Offscreen stereo render with distortion */

      // clear the color and depth buffers
      glClearColor( 0.02, 0.05, 0.1, 0.0 );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      double proj[16];

      double ipd = 0.064; // inter-pupillary distance
      double leftEye[3] = { -0.5*ipd, 0.15, 0.1 };
      double rightEye[3] = { 0.5*ipd, 0.15, 0.1 };

      double offs = stereoProjectionOffset( getDisplayInfo(dev) );
      int halfwid = bb->wid >> 1;


      /*** Left Eye ***/

      glViewport( 0.0f, 0.0f, halfwid, bb->hgt );

      // offset projection matrix for left lens
      mat4_identity( proj ); proj[12] = offs;
      mat4_multiply( proj, g_proj, NULL );

      // build view matrix for left eye
      quat_multiplyVec3( orient, leftEye, NULL );
      vec3_add( leftEye, g_head, NULL );
      viewOfPosOrient( leftEye, orient, eye );

      render( g_renderList, eye, proj );


      /*** Right Eye ***/

      glViewport( halfwid, 0.0f, halfwid, bb->hgt );

      // offset projection matrix for right lens
      mat4_identity( proj ); proj[12] = -offs;
      mat4_multiply( proj, g_proj, NULL );

      // build view matrix for right eye
      quat_multiplyVec3( orient, rightEye, NULL );
      vec3_add( rightEye, g_head, NULL );
      viewOfPosOrient( rightEye, orient, eye );

      render( g_renderList, eye, proj );

      fboUnbind(); // return to framebuffer


      /* distort from backbuffer to framebuffer */

      View v; memset( &v, 0, sizeof(View) );
      Distortion distort = g_defaultDistort;

      v.wid = fb->wid/2.; v.hgt = fb->hgt;
      mapDistortion( bb->rgba, &distort, &v );

      v.x0 = v.wid;
      distort.XCenterOffset = -distort.XCenterOffset;
      mapDistortion( bb->rgba, &distort, &v );

   }else{

      /* Direct framebuffer render with no stereo or distortion */
      glClearColor( 0.02, 0.05, 0.1, 0.0 );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      glViewport( 0.0f, 0.0f, fb->wid, fb->hgt );
      viewOfPosOrient( g_head, orient, eye );
      render( g_renderList, eye, g_proj );

   }

   glFlush( );
   glutSwapBuffers( );
}


/////////////////////////////////////////////////////////////////////////////////////
// Continuous sample/update thread code
// select() chosen for portability
/////////////////////////////////////////////////////////////////////////////////////
void *threadFunc( void *data )
{
    Device *localDev = (Device *)data;
    if( !openRift(0,localDev) )
    {
        printf("Could not locate Rift\n");
        printf("Be sure you have read/write permission to the proper /dev/hidrawX device\n");
        return 0;
    }

    printf("Device Info:\n");
    printf("\tName:      %s\n", localDev->name);
    printf("\tNroduct:   %s\n", localDev->product);
    printf("\tVendorID:  0x%04hx\n", localDev->vendorId);
    printf("\tProductID: 0x%04hx\n", localDev->productId);

    printf("ESC or q to quit\n\n");

    while( localDev->runSampleThread )
    {
        // Try to sample the device for 1ms
        waitSampleDevice(localDev, 1000);

        // Send a keepalive - this is too often.  Need to only send on keepalive interval
        sendSensorKeepAlive(localDev);
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Tracker thread function
/////////////////////////////////////////////////////////////////////////////////////////////
void runSensorUpdateThread( Device *dev )
{
    pthread_t f1_thread; 
    dev->runSampleThread = TRUE;
    pthread_create(&f1_thread,NULL,threadFunc,dev);
}

void initialize()
{
    initGL();

    // projection
    double zNear = 0.01;
    double zFar = 500.0;
    double aspect = (double)g_width / (double)g_height;
    double fovy = 90./aspect; // FIXME: base on Rift display info
    mat4_perspective( fovy, aspect, zNear, zFar, g_proj );

    defaultSensorDisplayInfo( &defaultDisplayInfo );
}



//  procedural textures...

GLuint proceduralTex( int w, int h, void (*fn)(double,double,double,unsigned char*) );
double perlin( double x, double y, double z );

/* Bias values 'x' in 0-1 range using a gamma function. 0 <= b <= 1
 *  b = 0.5 is identity;
 *  b > 0.5 biases upward;
 *  b < 0.5 biases downward;
 */
double bias( double x, double b ){
   if( x <= 0. ) return 0.;
   return pow( x, log(b) / log(0.5) );
}

void fn1( double x, double y, double z, unsigned char *d ){
   double s = 40.;
   double a = 0.6 * perlin(x*s,y*s,z*s);
   s*= 2.13;
   a+= 0.1 * perlin(x*s,y*s,z*s);
   a = 0.9 - fabs(a); //bias( fabs(a), 0.5 );
   double b = 0.8 + 0.2 * perlin(x*3.,y*2.,z+7.2);
   int n = (int)(a*b*255.99);
   d[0] = n;
   d[1] = n;
   d[2] = (int)(a*255.99);
   d[3] = 255;
}

void fn2( double x, double y, double z, unsigned char *d ){
   double s = 30.;
   double r = s*1.23;
   double t = s*0.125;
   double b = 0.1 * perlin(x*t,y*t,(z+1.3)*t);
   t*= 5.3;
   double c = 0.8 + 0.3 * perlin(x*t,y*t,(z+4.2)*t);
   double a = c * (1. - fabs( perlin((x+b)*s,(y+b)*s,z*s) ) ) *
              (1. - fabs( perlin((x+0.2)*r,(y+0.9)*r,(z+2.7)*r) ) );
   int n = a > 1.? 255 : (int)(a*255.99);
   d[0] = n;
   d[1] = n;
   d[2] = n;
   d[3] = 255;
}

//-----------------------------------------------------------------------------
// Name: main( )
// Desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    dev = (Device *)malloc(sizeof(Device));
    runSensorUpdateThread(dev);

    glutInitContextVersion( 4, 2 );
    glutInitContextFlags( GLUT_CORE_PROFILE | GLUT_DEBUG );
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowSize( g_width, g_height );
    glutInitWindowPosition( 0, 0 );
    glutCreateWindow( argv[0] );

    // Set up our callbacks
    glutIdleFunc( idleFunc );
    glutDisplayFunc( displayFunc );
    glutReshapeFunc( reshapeFunc );
    glutKeyboardFunc( keyboardFunc );
    glutMouseFunc( mouseFunc );

    // Local init
    initialize();


    /* XXX The following are "local variables" which must persist...
     * these functions can be made to allocate from the heap instead, with
     * requirement for corresponding free(). Then this lump could be moved to
     * initialize(). */

    // Compile a shader program
    // -vertex format and rendering are dependent on this
    // -note, it's possible to use different shaders with model/verts if the
    //  use of uniform variables is consistent
    GLuint shader = shaderProgram( COUNTED_ARRAY(shdrNoLight) );

    showUniforms( shader );

    // New vertex format
    VDesc desc[] =
    {  {"point", 3},
       {"rgba",  4},
       {"uv01",  2}  };
    VFormat v3c4t2 = vtxNewFormat( shader, COUNTED_ARRAY(desc) );


    // Objects to render
    Renderable square = renderableCreate( shader, &v3c4t2, BYTE_ARRAY(squareVerts), BYTE_ARRAY(squareIdx), GL_STATIC_DRAW );
    Renderable floor = square;

    // Create a texture
    GLuint tex1 = proceduralTex( 512, 512, fn1 );
    GLuint tex2 = proceduralTex( 512, 512, fn2 );
    renderableAddTex( &square, "rgbaMap", tex2 );
    renderableAddTex( &floor, "rgbaMap", tex1 );


    RList r0,r1,r2,r3,r4;

    double ang = M_PI * 0.5;
    double trans[] = { 0., 0., 4. };

    r0.rend = &square;
    mat4_identity( r0.mtx );
    mat4_rotateY( r0.mtx, 0., NULL );
    mat4_translate( r0.mtx, trans, NULL );
    r0.next = NULL;

    r1.rend = &square;
    mat4_identity( r1.mtx );
    mat4_rotateY( r1.mtx, ang, NULL );
    mat4_translate( r1.mtx, trans, NULL );
    r1.next = &r0;

    r2.rend = &square;
    mat4_identity( r2.mtx );
    mat4_rotateY( r2.mtx, ang*2., NULL );
    mat4_translate( r2.mtx, trans, NULL );
    r2.next = &r1;

    r3.rend = &square;
    mat4_identity( r3.mtx );
    mat4_rotateY( r3.mtx, ang*3., NULL );
    mat4_translate( r3.mtx, trans, NULL );
    r3.next = &r2;

    r4.rend = &floor;
    mat4_identity( r4.mtx );
    mat4_rotateZ( r4.mtx, 2.*ang, NULL );
    mat4_rotateX( r4.mtx, ang, NULL );
    double trans3[] = { 0., -3., -1. };
    mat4_translate( r4.mtx, trans3, NULL );
    r4.next = &r3;

    g_renderList = &r4;


    // Describe viewport/buffers to render into

    SensorDisplayInfo *displayInfo = getDisplayInfo( dev );

    View framebuffer = frameBuffer( displayInfo->HResolution, displayInfo->VResolution );
    g_framebuffer = &framebuffer;

    int stereo = 0;
#define STEREO
#ifdef STEREO
    stereo = 1;
    View backbuffer = offscreenBuffer( GL_LINEAR, 1600, 1000 );
    g_backbuffer = &backbuffer;
#endif

    // distortionFitXY... coordinates of a point to 'fit' to the distorted
    // display -- so, -1,0 meaning to keep the leftmost point in-view
    float x = -1.0f;
    float y =  0.0f;
    distortionUpdateOffsetAndScale( &g_defaultDistort, displayInfo, &framebuffer, x, y );

    projectionMatrix( g_framebuffer, displayInfo, &g_defaultDistort, stereo, g_proj );


    // Go Glut
    glutMainLoop();

    // FIXME mainloop isn't exited -- we should add sane cleanup (a lot more
    // than the offscreen-buffer...)
    if( g_backbuffer )
       freeOffscreenBuffer( g_backbuffer );

    return 0;
}



/* ----------------- Procedural Texturing ------------------------- */

GLuint proceduralTex( int w, int h, void (*fn)(double,double,double,unsigned char*) ){
   GLuint tex = 0;
   unsigned char *texdata = malloc( w*h*4 );
   unsigned char *row = texdata;
   double step = 1. / (float)w;
   double x,y;
   int i, j;
   for( j=0, y=0.; j<h; j++, y+=step ){
      for( i=0, x=0.; i<w; i++, x+=step ){
         fn( x, y, 0., row+i*4 );
      }
      row+= w * 4;
   }
   tex = texCreate( TEX_BYTE4, w, h, GL_LINEAR, TEX_CLAMP, texdata );
   free( texdata );
   return tex;
}

static int pmt[] = {
   151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
   151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static double fade(double t){ return t * t * t * (t * (t * 6. - 15.) + 10.); }
static double lerp(double t, double a, double b){ return a + t * (b - a); }
static double grad(int hash, double x, double y, double z){
   int h = hash & 15;
   double u = h<8 ? x : y,
          v = h<4 ? y : h==12||h==14 ? x : z;
   return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

double perlin( double x, double y, double z ){
   double ix = floor(x), iy = floor(y), iz = floor(z);
   x -= ix; y -= iy; z -= iz;
   int X = (int)ix & 0xff, Y = (int)iy & 0xff, Z = (int)iz & 0xff;
   double u = fade(x), v = fade(y), w = fade(z);
   int A = pmt[X  ]+Y, AA = pmt[A]+Z, AB = pmt[A+1]+Z,
       B = pmt[X+1]+Y, BA = pmt[B]+Z, BB = pmt[B+1]+Z;

   return lerp(w, lerp(v, lerp(u, grad(pmt[AA  ], x  , y  , z   ),
                                  grad(pmt[BA  ], x-1, y  , z   )),
                          lerp(u, grad(pmt[AB  ], x  , y-1, z   ),
                                  grad(pmt[BB  ], x-1, y-1, z   ))),
                  lerp(v, lerp(u, grad(pmt[AA+1], x  , y  , z-1 ),
                                  grad(pmt[BA+1], x-1, y  , z-1 )),
                          lerp(u, grad(pmt[AB+1], x  , y-1, z-1 ),
                                  grad(pmt[BB+1], x-1, y-1, z-1 ))));
}


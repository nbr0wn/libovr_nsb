#define _GNU_SOURCE  // for sincos

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#include <libovr_nsb/OVR.h>

// to USE_SDL, linking must include -lSDL rather than -lglut
//#define USE_SDL
#ifdef USE_SDL
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL/SDL.h>
#else
#include <GL/freeglut.h>
#endif

// Define TERRAIN for a mountainous heightfield scene; otherwise
// the scene is simply a few squares
#define TERRAIN

// Define STEREO for stereo+distortion; otherwise undistorted single view
#define STEREO

// Layout of this file:
//
//   Miscellaneous util-functions
//
//   Modern OpenGL abstractions:
//     Shader Load, Compile, Link
//     Buffer Object
//     Vertex Attributes
//     Textures, Framebuffers, and Renderbuffers
//     View (viewport) and offscreen buffer
//
//   Model structures
//
//   --------------------------
//
//   Global State
//
//   Stereo-lens functions translated from OVR SDK
//
//   Rendering
//
//   Main
//
//   --------------------------
//
//   Procedural Texturing
//
//   Tesselation
//
//   Terrain Generation
//


// ------------------------------------
// TODO
//  |render setup (transforms, view, uniforms, etc)
//  |texture params
//  |backbuffer
//  |stereo
//  |distortion
//  |add 'discard' to distort shaders
//  |aspect has a problem -- a 2/3 ratio is required where it should be 1/2
//  |limit vertex recomputation
//  /add movement -- probably best to leave this for a demo using SDL
//  -create shapes
//    |fractal terrain
//    |change head-height with underlying terrain
//    -skydome
//  -lighting
//  -OpenAL audio
//
//  -set Distortion struct from HMD info
//  -fix resize
//  -rather than GetAttribLocation, use BindAttribLocation (do before linking)
//  -full implementation of stereo.c
//  -breakout GL stuff into gltools.c?
//  -properly clean-up resources (buffers, textures, mallocs)
//  -fix any remaining FIXME's and TODO's; check XXX's.
//  -make code easier to read


void displayFunc( );
#ifndef USE_SDL
void idleFunc( );
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );
#endif

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

// convert to OpenGL float matrix, from gl-matrix.c double
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
   printf( "Uniforms for program %d:\n" );
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

// Note that "disable" is not required as long as enable is captured as part
// of the VAO state.
void vtxEnable( VFormat *fmt ){
   int i;
   VAttrib *a = fmt->attr;
   for( i=0; i<fmt->attribs; i++ ){
      glEnableVertexAttribArray( a[i].idx );
      glVertexAttribPointer( a[i].idx, a[i].elems, GL_FLOAT, GL_FALSE, fmt->stride, a[i].offs );
   }
}

void vtxDisable( VFormat *fmt ){
   int i;
   VAttrib *a = fmt->attr;
   for( i=0; i<fmt->attribs; i++ )
      glDisableVertexAttribArray( a[i].idx );
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
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
   glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
   glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, data );
   glBindTexture( GL_TEXTURE_2D, 0 );
   return idx;
}

GLuint texCreateMip( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data ){
   GLuint idx = texNew( filter, wrapS, wrapT );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
   glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
   glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, data );
   glGenerateMipmap( GL_TEXTURE_2D );
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
   glBindBuffer( GL_ARRAY_BUFFER, r->vbo );

	// set ibo and vertex attribs to be captured by VAO
   glGenVertexArrays( 1, &r->vao );
   glBindVertexArray( r->vao );
   vtxEnable( r->fmt );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, r->ibo );
   glBindVertexArray(0);

	// reset state
	vtxDisable( r->fmt );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
   glBindBuffer( GL_ARRAY_BUFFER, 0 );
}

void renderableAddTex( Renderable *r, char *uname, GLuint tex ){
   r->tex = tex;
   r->u_tex = glGetUniformLocation( r->prog, uname );
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
	glBindVertexArray( 0 );
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
   GLuint depth = rbNew( GL_DEPTH_COMPONENT, w, h );
   GLuint fbo = fboCreateWithDepth( rgba, depth );
   return (View){ 0, 0, w, h, 1, rgba, depth, fbo };
}

void freeOffscreenBuffer( View *v ){
   glDeleteTextures( 1, &v->rgba );
   glDeleteRenderbuffers( 1, &v->depth );
   glDeleteFramebuffers( 1, &v->fbo );
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Model structures, for representing deformable heightfield (or more)
/////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_ADJ 7

typedef struct {
   float p[3];  // current x,y,z
   float hgt;   // target height (y)
   float vel;   // current velocity
   unsigned short adj[MAX_ADJ];  // indices of adjacent faces
   unsigned short faces;         // number of adjacent faces
} Vert;

typedef struct {
   float n[3];
   unsigned short a,b,c;
   unsigned short reserved;
} Face;

typedef struct {
   GLfloat*  vdata;
   GLushort* idata;
   Vert *v;
   Face *f;
   int verts;
   int faces;
   Renderable r;
   int updateIndices;
} Model;

// Terrain functions which are defined waaay below...
Model *genTerrain( GLuint prog, VFormat *fmt, int maxTess, float scale );
void   modelUpdate( Model *m, float dt );
float  heightAt( Model *m, float x, float z, int *phint );


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

Model *g_terrain = NULL;



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


// These are target values; not current values
double g_targetHeading = 0.;
double g_lftfoot = 0.;
double g_rgtfoot = 0.;
// TODO left/right foot stepping isn't really utilized yet

double g_heading = 0.;

// heading is turned into xz-direction
void vecOfHeading( double heading, double step, double *dir ){
   double s,c; sincos( heading, &s, &c );
   dir[0] = -s * step;
   dir[1] = 0.;
   dir[2] = -c * step;
}

void quatOfHeading( double heading, double *q ){
   double s,c; sincos( heading*0.5, &s, &c );
   q[0] = 0.;
   q[1] = s;
   q[2] = 0.;
   q[4] = c;
}

double angleDiff( double a, double b ){
   double x = a - b;
   if( x < M_PI && x >= -M_PI) return x;
   double twoPi = M_PI + M_PI;
   double y = x + M_PI;
   double n = floor(y / twoPi);
   return y - n * twoPi - M_PI;
}

#ifdef USE_SDL
int processEvents( void ){
   SDL_Event event;

   while(SDL_PollEvent(&event)){
      switch(event.type){
         case SDL_KEYDOWN:
            switch(event.key.keysym.sym){
               case SDLK_ESCAPE:
                  return 0;
               default: break;
            }
            break;
         case SDL_MOUSEMOTION:
            //gCursX = event.motion.x;
            //gCursY = event.motion.y;
            break;
         case SDL_MOUSEBUTTONDOWN:
         case SDL_MOUSEBUTTONUP:
            // SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT
            // SDL_BUTTON_WHEELUP, SDL_BUTTON_WHEELDOWN
            break;
         case SDL_VIDEORESIZE:
            //setDisplay(rendState,event.resize.w,event.resize.h);
            //redraw = 1;
            break;
         case SDL_QUIT:
            return 0;
         default: break;
		}
	}
	return 1;
}

#else

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
        case '.':
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

        case 'a': // left foot forward
            g_lftfoot += 1.f;
            break;
        case 's': // right foot forward
            g_rgtfoot += 1.f;
            break;
        case 'z': // left foot back
            g_lftfoot -= 0.5f;
            break;
        case 'x': // right foot back
            g_rgtfoot -= 0.5f;
            break;
        case ' ': // turn to facing
            if( dev ){
               double orient[4];
               double facing[] = {0., 0., -1.};
               quat_set( dev->Q, orient );
               quat_multiply( orient, g_qref, NULL ); // apply inverse-reference orientation
               quat_multiplyVec3( orient, facing, NULL );
               g_targetHeading = atan2f( -facing[0], -facing[2] );
            }
            break;

    }

    glutPostRedisplay( );
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

#endif


/////////////////////////////////////////////////////////////////////////////////////////////
// Stereo-lens functions translated from OVR SDK
/////////////////////////////////////////////////////////////////////////////////////////////

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
    if( stereo ) aspect*= 0.5;

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
      distortShader = shaderProgram( COUNTED_ARRAY(shdrDistort) );
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
		// XXX This might have been because I had leaky state (vtxEnable, perhaps)
      glBindBuffer( GL_ARRAY_BUFFER, vbo );
      glGenVertexArrays( 1, &vao );
      glBindVertexArray( vao );
      vtxEnable( &v3t2 );
      glBindVertexArray( 0 );
      vtxDisable( &v3t2 );
      glBindBuffer( GL_ARRAY_BUFFER, 0 );

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
   a[1] = h*0.5f*scaleFactor*as;
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
   glDisable( GL_DEPTH_TEST );
   glDisable( GL_BLEND );

   texBind( 0, u_texSrc, src );

   glBindVertexArray( vao );
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
   glBindVertexArray( 0 );

   texUnbind( 0 );

   glEnable( GL_BLEND );
   glEnable( GL_DEPTH_TEST );

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

int localVIdx = 0;

// turn left/right footsteps into g_head results
void move( void ){
   // g_heading must accommodate changes in g_targetHeading
   double da = angleDiff( g_targetHeading, g_heading );
   double da_step = da < 0.05 ? (da > -0.05 ? da : -0.05) : 0.05;
   g_heading+= da_step;

   // target position is between feet in direction of "g_heading"
   double avg = 0.5 * (g_lftfoot + g_rgtfoot);

   double step = avg < 0.05 ? (avg > -0.05 ? avg : -0.05) : 0.05;

   g_lftfoot-= step;
   g_rgtfoot-= step;
   double v[3];
   vecOfHeading( g_heading, step, v );
   vec3_add( g_head, v, NULL );

   g_head[1] = 1.8f + heightAt( g_terrain, g_head[0], g_head[2], &localVIdx );
}

void displayFunc( )
{
   double eye[16];
   View *bb = g_backbuffer;
   View *fb = g_framebuffer;

   assert(fb); // we at least require the framebuffer to be defined

#ifdef TERRAIN
   modelUpdate( g_terrain, 1.f/60.f );

   //g_head[1] = 1.8f + heightAt( g_terrain, g_head[0], g_head[2], &localVIdx );
   move();
#endif

   // Orientation, from HMD sensor
   double orient[4] = { 0., 0., 0., 1. };
	if( dev ){
		quat_set( dev->Q, orient );
		quat_multiply( orient, g_qref, NULL ); // apply inverse-reference orientation
	}

   /*
   double qheading[4];
   quatOfHeading( -g_heading, qheading );
   quat_multiply( orient, qheading, NULL );
   */

   if( bb && bb->offscreen && fboBind(bb->fbo) ){
      /* Offscreen stereo render with distortion */

      // clear the color and depth buffers
      glClearColor( 0.45, 0.5, 0.8, 0.0 );
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
      glClearColor( 0.45, 0.5, 0.8, 0.0 );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      glViewport( 0.0f, 0.0f, fb->wid, fb->hgt );
      viewOfPosOrient( g_head, orient, eye );
      render( g_renderList, eye, g_proj );

   }

   glFlush( );

#ifdef USE_SDL
	SDL_GL_SwapBuffers();
#else
   glutSwapBuffers( );
#endif
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


// Procedural textures...

GLuint proceduralTex( int w, int h, void (*fn)(double,double,double,unsigned char*) );
double perlin( double x, double y, double z );

void fn1( double x, double y, double z, unsigned char *d ){
   double s = 40.;
   double a = 0.6 * perlin(x*s,y*s,z*s);
   s*= 2.13;
   a+= 0.1 * perlin(x*s,y*s,z*s);
   a = 0.9 - fabs(a);
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

#ifdef USE_SDL
    if( SDL_Init(SDL_INIT_VIDEO) < 0 ){
        fprintf(stderr,"Failed SDL init: %s\n",SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);

    SDL_Surface *screen = SDL_SetVideoMode( g_width, g_height, 0, SDL_OPENGL|SDL_DOUBLEBUF );
    if(!screen){
        fprintf(stderr,"Failed SDL video mode request: %s\n",SDL_GetError());
        exit(1);
    }
#else
    glutInitContextVersion( 3, 3 );
    glutInitContextFlags( GLUT_CORE_PROFILE | GLUT_DEBUG );
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowSize( g_width, g_height );
    glutInitWindowPosition( 0, 0 );
    glutCreateWindow( argv[0] );
    glutFullScreen();

    // Set up our callbacks
    glutIdleFunc( idleFunc );
    glutDisplayFunc( displayFunc );
    glutReshapeFunc( reshapeFunc );
    glutKeyboardFunc( keyboardFunc );
    glutMouseFunc( mouseFunc );
#endif

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

    //showUniforms( shader );

    // New vertex format
    VDesc desc[] =
    {  {"point", 3},
       {"rgba",  4},
       {"uv01",  2}  };
    VFormat v3c4t2 = vtxNewFormat( shader, COUNTED_ARRAY(desc) );


    // Create some textures
    GLuint tex1 = proceduralTex( 512, 512, fn1 );
    GLuint tex2 = proceduralTex( 512, 512, fn2 );


#ifdef TERRAIN
    // Terrain
    int maxTess = 7; // to go higher would require larger index format (uint32)
    float scale = 100.;
    g_terrain = genTerrain( shader, &v3c4t2, maxTess, scale );
    renderableAddTex( &g_terrain->r, "rgbaMap", tex2 );
#endif


    // Objects to render
    Renderable square = renderableCreate( shader, &v3c4t2, BYTE_ARRAY(squareVerts), BYTE_ARRAY(squareIdx), GL_STATIC_DRAW );
    Renderable floor = square;
    renderableAddTex( &square, "rgbaMap", tex2 );
    renderableAddTex( &floor, "rgbaMap", tex1 );


#ifdef TERRAIN
    RList r0;
    r0.rend = &g_terrain->r;
    mat4_identity( r0.mtx );
    r0.next = NULL;
    g_renderList = &r0;
#else

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
#endif


    // Describe viewport/buffers to render into

    SensorDisplayInfo *displayInfo = getDisplayInfo( dev );

    View framebuffer = frameBuffer( displayInfo->HResolution, displayInfo->VResolution );
    g_framebuffer = &framebuffer;

    int stereo = 0;
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


#ifdef USE_SDL
	 while( processEvents() ){
		  displayFunc();
		  // TODO force retrace sync
	 }
#else
    // Go Glut
    glutMainLoop();
#endif

    // FIXME mainloop isn't exited -- we should add sane cleanup (a lot more
    // than the offscreen-buffer...)
    if( g_backbuffer )
       freeOffscreenBuffer( g_backbuffer );

#ifdef USE_SDL
	 SDL_Quit();
#endif
    return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Procedural Texturing
/////////////////////////////////////////////////////////////////////////////////////////////

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
   tex = texCreateMip( TEX_BYTE4, w, h, GL_LINEAR, TEX_MIRROR, texdata );
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



/////////////////////////////////////////////////////////////////////////////////////////////
// Heightfield tesselation
/////////////////////////////////////////////////////////////////////////////////////////////

#define SQRT3BY3 (0.5773502692f)

// Some integer and bit-fiddling functions

uint32_t bitmaskFill( uint32_t n ){
  n|= n >> 1;
  n|= n >> 2;
  n|= n >> 4;
  n|= n >> 8;
  return n | (n >> 16);
}

uint32_t roundPow2( uint32_t n ){ return 1 + bitmaskFill(n-1); }
int isPow2( uint32_t n ){ return ((n & (n-1)) == 0); }
uint32_t sumToN( uint32_t n ){ return ((n*n+n) >> 1); }

uint32_t interleaveBits( uint32_t x, uint32_t y ){
   static const uint32_t B[] = {0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF};
   static const uint32_t S[] = {1, 2, 4, 8};
   x = (x | (x << S[3])) & B[3];
   x = (x | (x << S[2])) & B[2];
   x = (x | (x << S[1])) & B[1];
   x = (x | (x << S[0])) & B[0];
   y = (y | (y << S[3])) & B[3];
   y = (y | (y << S[2])) & B[2];
   y = (y | (y << S[1])) & B[1];
   y = (y | (y << S[0])) & B[0];
   return x | (y << 1);
}


// Simple hashtable to map a key to an index

typedef struct {
   uint32_t mask; // size-1, where size is 2^n
   uint32_t table[1];
} Hashtable;

Hashtable *hashCreate( uint32_t size ){
   Hashtable *h = NULL;
   if( size > 0 && isPow2(size) ){
      h = (Hashtable*)calloc( sizeof(Hashtable) + (size-1) * sizeof(uint32_t), 1 );
      h->mask = size-1;
   }
   return h;
}

uint32_t hashHsieh( uint32_t hash ){
	hash ^= hash << 3;
	hash += hash >> 5;
	hash ^= hash << 4;
	hash += hash >> 17;
	hash ^= hash << 25;
	hash += hash >> 6;
	return hash;
}

// this is used for store and retrieval, returning the index for a key
// -keys must be nonzero
uint32_t hashIndex( Hashtable *h, uint32_t key ){
   uint32_t idx = hashHsieh(key) & h->mask;
   uint32_t *ht = h->table;
   while( ht[idx] ){
      if( ht[idx] == key ) return idx;
      idx = (idx+1) & h->mask;
   }
   ht[idx] = key;
   return idx;
}


// Tesselation

typedef float (*HeightFn)(float*,float,float);

void initVertXZ( Vert *v, float x, float z ){
   memset( v, 0, sizeof(Vert) );
   v->p[0] = x;
   v->p[2] = z;
   v->faces = 0;
}

void adjAdd( Vert *v, int iAdd ){
   int i;
   for( i=0;i<v->faces;i++ )
      if( v->adj[i] == iAdd ) return;
   assert( v->faces < MAX_ADJ );
   v->adj[i] = iAdd;
   v->faces++;
}

void initFace( Vert *v, Face *f, int i, int a, int b, int c ){
   f[i].a = a;
   f[i].b = b;
   f[i].c = c;
   adjAdd( v+a, i );
   adjAdd( v+b, i );
   adjAdd( v+c, i );
}

// sets 'c' as midpoint between a,b
// height function has what parameters?
//  -edge length, coordinate, averageTargetHeight
void midVert( HeightFn fn, Vert *v, int ia, int ib, int ic ){
   Vert *a = v+ia;
   Vert *b = v+ib;
   Vert *c = v+ic;

   c->p[0] = (a->p[0] + b->p[0]) * 0.5f;
   c->p[1] = (a->p[1] + b->p[1]) * 0.5f;
   c->p[2] = (a->p[2] + b->p[2]) * 0.5f;
   float dx = (a->p[0] - b->p[0]);
   float dz = (a->p[2] - b->p[2]);
   float edgeLen = sqrtf( dx*dx + dz*dz );
   float targetHeight = (a->hgt + b->hgt) * 0.5f;
   c->hgt = (*fn)( c->p, edgeLen, targetHeight );
   c->vel = 0.f;
   c->faces = 0;
}

// Edge is directionless: a->b and b->a define the same edge.
uint32_t edgeId( uint32_t a, uint32_t b ){
   if( a < b ) interleaveBits( a, b );
   else        interleaveBits( b, a );
}

// given current vertex state, fracture into new vertices with target heights
// return new count
// -for every edge (ordered vertex pair), have a mapping to its midpoint vertex
// -index list is completely recreated on tesselate
// XXX 'v' must be large enough to contain 4*verts; 'index' must be malloc'd
int tesselate( HeightFn fn, int verts, Vert *v, int *pfaces, Face **pface ){
   int faces = *pfaces;
   Face *face = *pface;
   printf("tesselate: verts = %d, tris = %d\n", verts, faces );

   // For every triangle, we'll now have four...
   Face *f = (Face*)calloc( 4 * faces, sizeof(Face) );
   int newFaces = 0;

   int i;

   // hashtable of vertex indices mapped via edges (vertex pairs)
   int hashSize = roundPow2( verts*4 );  // should be at least verts*2
   Hashtable *h = hashCreate( hashSize );
   int *edgeMap = (int*)malloc( hashSize * sizeof(int) );
   for( i=0; i<hashSize; i++ ) edgeMap[i] = -1; // -1 is empty

   // face-adjacency is fully updated every time, so clear it now
   for( i=0; i<verts; i++ ){
      v[i].faces = 0;
   }

   for( i=0; i<faces; i++ ){
      int a = face[i].a;
      int b = face[i].b;
      int c = face[i].c;

      // a,b,c are the vertex indices for this triangle... now create or find the
      // vertices between them, to create 4 new triangles with.
      uint32_t n;
      n = hashIndex( h, edgeId(a,b) );
      if( edgeMap[n] < 0 ){
         midVert( fn, v, a, b, verts );
         edgeMap[n] = verts++;
      }
      int ab = edgeMap[n];

      n = hashIndex( h, edgeId(b,c) );
      if( edgeMap[n] < 0 ){
         midVert( fn, v, b, c, verts );
         edgeMap[n] = verts++;
      }
      int bc = edgeMap[n];

      n = hashIndex( h, edgeId(c,a) );
      if( edgeMap[n] < 0 ){
         midVert( fn, v, c, a, verts );
         edgeMap[n] = verts++;
      }
      int ca = edgeMap[n];

      // Four new triangles
      initFace( v, f, newFaces++, a,  ab, ca );
      initFace( v, f, newFaces++, b,  bc, ab );
      initFace( v, f, newFaces++, c,  ca, bc );
      initFace( v, f, newFaces++, ab, bc, ca );
   }

   free( edgeMap );
   free( h );

   free( *pface );
   *pface = f;
   *pfaces = newFaces;
   printf("   output: verts = %d, tris = %d\n", verts, newFaces );
   return verts;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Terrain Generation
/////////////////////////////////////////////////////////////////////////////////////////////

// Vertex stuff... TODO: just use gl-matrix.c 
void vZero( float *a ){
   a[0] = 0.f; a[1] = 0.f; a[2] = 0.f;
}
void vNeg( float *a ){
   a[0]=-a[0]; a[1]=-a[1]; a[2]=-a[2];
}
void vAdd( float *a, float *b ){
   a[0] += b[0]; a[1] += b[1]; a[2] += b[2];
}
void vSub( float *a, float *b ){
   a[0] -= b[0]; a[1] -= b[1]; a[2] -= b[2];
}
void vScale( float *a, float s ){
   a[0] *= s; a[1] *= s; a[2] *= s;
}
float vDot( float *a, float *b ){
   return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
void vCross( float *a, float *b, float *r ){
   r[0] = a[1] * b[2] - a[2] * b[1];
   r[1] = a[2] * b[0] - a[0] * b[2];
   r[2] = a[0] * b[1] - a[1] * b[0];
}
void vNormalize( float *a ){
   float len = sqrtf( vDot(a,a) );
   vScale( a, 1.f/len );
}
void vCopy( float *s, float *d ){
   d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
}


// Color functions

void cMix( float t, float *a, float *b, float *r ){
   int i;
   for( i=0; i<4; i++ )
      r[i] = t * (b[i] - a[i]) + a[i];
}

// mix from -1 (lo) to +1 (hi)
void cMixSigned( float *rgba, float *lo, float *hi, float mix ){
   if( mix < 0.f )
      cMix( -mix, rgba, lo, rgba );
   else
      cMix( mix, rgba, hi, rgba );
}

void cMap( int entries, float map[][5], float t, float *rgba ){
   int i = 0;
   while( i < entries && t > map[i][0] ) i++;
   if( i <= 0 || i >= entries ){
      if( i >= entries ) i--;
      memcpy( rgba, &(map[i][1]), 4*sizeof(float) );
      return;
   }
   i--;
   t = (t - map[i][0]) / (map[i+1][0] - map[i][0]);
   t = (3.f-t-t) * t * t;  // s-curve
   float *a = &(map[i][1]);
   float *b = &(map[i+1][1]);
   cMix( t, &(map[i][1]), &(map[i+1][1]), rgba );
}

float terrainColor[][5] = {
   { 0.00,   0.4, 1.0, 0.2, 1.0 },
   { 0.20,   0.1, 1.0, 0.2, 1.0 },
   { 0.40,   0.0, 0.8, 0.0, 1.0 },
   { 0.47,   0.0, 0.8, 0.0, 1.0 },
   { 0.50,   0.7, 0.5, 0.2, 1.0 },
   { 0.55,   0.7, 0.7, 0.6, 1.0 },
   { 0.85,   0.6, 0.6, 0.6, 1.0 },
   { 0.90,   1.0, 1.0, 1.0, 1.0 },
   { 1.00,   1.0, 1.0, 1.0, 1.0 }
};

float randf( void ){
   static float rscale = 1.f/(float)RAND_MAX;
   return (float)(rand())*rscale;
}

void faceMidpoint( Vert *v, Face *f, float *m ){
   vZero( m );
   vAdd( m, v[f->a].p );
   vAdd( m, v[f->b].p );
   vAdd( m, v[f->c].p );
   vScale( m, 0.3333333333f );
}

// add non-duplicate entry 'n'
int adjacentAdd( int *a, int count, int vIdx, int n ){
   if( n != vIdx ){
      int i = 0; do{
         if( i == count ){ a[count++] = n; break; }
      }while( a[i++] != n );
   }
   return count;
}

// fills 'a' with indices of adjacent vertices to vIdx, returning number
int adjacent( Model *m, int vIdx, int *a ){
   int i, count=0;
   Vert *v = &m->v[vIdx];
   for( i=0; i<v->faces; i++ ){
      Face *f = &m->f[v->adj[i]];
      count = adjacentAdd( a, count, vIdx, f->a );
      count = adjacentAdd( a, count, vIdx, f->b );
      count = adjacentAdd( a, count, vIdx, f->c );
   }
   return count;
}

float distSqXZ( float *a, float *b ){
   float dx = b[0] - a[0];
   float dz = b[2] - a[2];
   return dx*dx + dz*dz;
}

// hint is a vertex index to start at; which is also set to closest found vertex
// NOTE face index can't be used since they can change with tesselation
float heightAt( Model *m, float x, float z, int *phint ){
   // Since vertices have no organization, I effectively have to test
   // each triangle... well, not quite: I can walk in the right direction

   // find support triangle
   int vi = *phint;
   Vert *v = m->v;

   // from here I can get all adjacent faces
   // from faces I can move in direction of x,z which might bound it
   float p[3] = { x, 0., z };

   float closest = distSqXZ( v[vi].p, p );
   int adj[16];
   int i,n,closer;
   do{
      closer = 0;
      n = adjacent( m, vi, adj );
      for( i=0; i<n; i++ ){
         int vj = adj[i];
         float d = distSqXZ( v[vj].p, p );
         if( d < closest ){
            vi = vj;
            closest = d;
            closer = 1;
         }
      }
   }while( closer );
   // now vi is index of closest vertex, and closest is distSqXZ of it
   // also, adj is a valid adjacency list

   // let's just get the next two closest verts and use this as a triangle
   int a = adj[0]; float da = distSqXZ( v[a].p, p );
   int b = adj[1]; float db = distSqXZ( v[b].p, p );
   if( db < da ){ // swap so a is closer than b
      float t = da; da = db; db = t;
      a = b; b = adj[0];
   }
   for( i=2; i<n; i++ ){
      int vj = adj[i];
      float d = distSqXZ( v[vj].p, p );
      if( d < db ){
         if( d < da ){
            b = a; db = da;
            a = vj; da = d;
         }else{
            b = vj; db = d;
         }
      }
   }

   // finally, we have vi, a, b as a triangle -- now determine height

   // even better might be determining a face, because then we have a normal
   // to use as a plane for intersection
   float va[3],vb[3],norm[3];
   vCopy( v[a].p, va );
   vCopy( v[b].p, vb );
   vSub( va, v[vi].p );
   vSub( vb, v[vi].p );
   vCross( va, vb, norm );
   vNormalize( norm );
   float d = vDot( v[vi].p, norm );

   float y = (d - norm[0]*x - norm[2]*z) / norm[1];
   return y;
}

void vertColor( Model *m, int idx, float peakHeight, float *rgba ){
   // use height to lookup color; modify by local curvature
   Vert *vtx = m->v+idx;

   // calculate vertex normal
   float n[3] = {0.f, 0.f, 0.f};
   int i;
   for( i=0; i<vtx->faces; i++ ){
      unsigned short j = vtx->adj[i];
      vAdd( n, m->f[j].n );
   }
   vNormalize( n );

   // curvature... face midpoint relative to p; dotprod with n
   float curvature = 0.f;
   if( vtx->faces > 0 ){
      float mid[3];
      for( i=0; i<vtx->faces; i++ ){
         unsigned short j = vtx->adj[i];
         faceMidpoint( m->v, m->f+j, mid );
         vSub( mid, vtx->p );
         vNormalize( mid );
         curvature-= vDot( mid, n );
      }
      curvature /= (float)(vtx->faces);
   }
   
   float t = vtx->p[1] / peakHeight;
   cMap( COUNTED_ARRAY(terrainColor), t, rgba );

   float lo[] = { 0.f, 0.f, 0.f, 1.f };
   float hi[] = { 1.f, 1.f, 1.f, 1.f };
   cMixSigned( rgba, lo, hi, curvature<0.?-sqrtf(-curvature):sqrtf(curvature) );
}

void modelUpdateGL( Model *m, int vStart, int vEnd ){
   int i;
   Vert *v = m->v;
   Face *f = m->f;
   //printf(" modelUpdateGL lo/hi = %d/%d\n", vStart, vEnd );

   if( vStart <= vEnd ){
      // XXX forcing all verts to update so colors are updated
      vStart = 0;
      vEnd = m->verts - 1;

      // calc face-normals
      float va[3],vb[3];
      for( i=0; i<m->faces; i++ ){
         unsigned short a = f[i].a, b = f[i].b, c = f[i].c;
         vCopy( v[a].p, va );
         vCopy( v[b].p, vb );
         vSub( va, v[c].p );
         vSub( vb, v[c].p );
         vCross( va, vb, f[i].n );
         vNormalize( f[i].n );
      }

      // vdata in v3c4t2 format
      float *d;
      // XXX to get vertColor right, this should be recomputed for every vertex
      for( i=vStart; i<=vEnd; i++ ){
         d = m->vdata + 9 * i;
         *d++ = v[i].p[0];
         *d++ = v[i].p[1];
         *d++ = v[i].p[2];
         vertColor( m, i, 60.f, d ); d+= 4;
         *d++ = v[i].p[0] * 0.1f;
         *d++ = v[i].p[2] * 0.1f;
      }

      // update GPU vertex buffer
      int stride = m->r.fmt->stride;
      glBindBuffer( GL_ARRAY_BUFFER, m->r.vbo );
      glBufferSubData( GL_ARRAY_BUFFER, vStart*stride, (vEnd-vStart+1)*stride, m->vdata + vStart*9 );
      glBindBuffer( GL_ARRAY_BUFFER, 0 );
   }

   if( m->updateIndices ){
      // fill the index array
      unsigned short *p = m->idata;
      for( i=0; i<m->faces; i++ ){
         *p++ = f[i].a; *p++ = f[i].b; *p++ = f[i].c;
      }

      // update GPU index buffer
      int indices = m->faces * 3;
      glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m->r.ibo );
      glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, indices*sizeof(GLushort), m->idata );
      //glBufferData( GL_ELEMENT_ARRAY_BUFFER, indices*sizeof(GLushort), m->idata, GL_STREAM_DRAW );
      glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
      m->r.count = indices;
      m->updateIndices = 0;
   }

	// XXX need to update the VAO for Intel GM45 (?)
	// FIXME this shouldn't be necessary but helps, so something is fishy
   /*
   glBindVertexArray( m->r.vao );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m->r.ibo );
   glBindVertexArray(0);
   */
}

void modelTesselate( Model *m, HeightFn fn ){
   m->verts = tesselate( fn, m->verts, m->v, &m->faces, &m->f );
   m->updateIndices = 1;
}

float heightMid( float *p, float edgeLen, float hgt ){
   return hgt;
}

float heightPerlin( float *p, float edgeLen, float hgt ){
   return hgt + 0.4 * edgeLen * perlin( p[0], p[1], p[2]+3.1f );
}

Model *genTerrain( GLuint prog, VFormat *fmt, int maxTess, float scale ){
   int verts = 3;
   int faces = 1;

   // For tesselation level 'n'...
   //  rows(n) = 2^n+1
   //  verts(n) = sumToN(rows(n))
   //  tris(n) = 4^n
   int maxVerts = sumToN( (1<<maxTess) + 1 );
   int maxTris = 1 << (maxTess+maxTess);

   int vsize = maxVerts * fmt->stride;
   int isize = maxTris * 3 * sizeof(GLushort);
   GLfloat *vdata = (GLfloat*)calloc( vsize, 1 );
   GLushort *idata = (GLushort*)calloc( isize, 1 );
   Vert *v = calloc( maxVerts, sizeof(Vert) );
   Face *f = calloc( 1, sizeof(Face) );

   initVertXZ( v+0,  0.f, -2.f*scale*SQRT3BY3 );
   initVertXZ( v+1, -scale,    scale*SQRT3BY3 );
   initVertXZ( v+2,  scale,    scale*SQRT3BY3 );

   f[0].a = 0;
   f[0].b = 1;
   f[0].c = 2;

   Model *m = (Model*)malloc( sizeof(Model) );
   m->v = v;
   m->f = f;
   m->vdata = vdata;
   m->idata = idata;
   m->verts = verts;
   m->faces = faces;
   m->updateIndices = 1;

   Renderable *r = &m->r;
   r->fmt = fmt;
   r->prog = prog;
   r->vbo = newBufObj( GL_ARRAY_BUFFER, vsize, vdata, GL_STREAM_DRAW );
   r->ibo = newBufObj( GL_ELEMENT_ARRAY_BUFFER, isize, idata, GL_STREAM_DRAW );
   r->mode = GL_TRIANGLES;
   r->type = GL_UNSIGNED_SHORT;
   r->count = faces*3;
   renderBindVao( r );

   // To set a basic shape (caldera), tesselate twice, and
   // then seed the vertex array with the shape (heights).
   modelTesselate( m, heightMid );
   modelTesselate( m, heightMid );

   // seed some initial heights
   int i;
   for( i=0; i<m->verts; i++ ){
      Vert *v = &m->v[i];
      float x = v->p[0];
      float z = v->p[2];
      float d = sqrtf( x*x + z*z );
      float h = d < scale*0.3? 0. : 30.f * ( 1.f - cosf( d * 3.f/70.f ) );
      v->hgt = heightPerlin( v->p, scale*0.5f, h );
   }

   modelUpdateGL( m, 0, m->verts-1 );

   return m;
}

void modelUpdate( Model *m, float dt ){
   // animate vertex height
   // first: step by a fixed velocity toward hgt, or stop at hgt
   static float time = 0.f;
   static int tess = 2;
   time+= dt;
   if( tess < 7 && time > 3.f ){
      modelTesselate( m, heightPerlin );
      tess++;
      time = 0.f;
   }

   // track low and high watermark of changed vertices
   int lo = m->verts;
   int hi = 0;
   int i;
   for( i=0; i<m->verts; i++ ){
      Vert *v = &m->v[i];
      float dy = v->hgt - v->p[1];
      if( fabs(dy) > 0.01 ){
         if( i<lo ) lo = i;
         if( i>hi ) hi = i;
         dy-= v->vel * dt;
         float vel = dy * 0.2;
         v->vel = vel;
         v->p[1]+= vel * dt;
      }
   }
   modelUpdateGL( m, lo, hi );
}


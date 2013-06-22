/* libovr-nsb/gldemo
 *
 * Author: Anthony Tavener
 *
 * A simple scene to view with the Oculus Rift.
 * Procedurally generated terrain, rendered using modern OpenGL.
 *
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

#include <libovr_nsb/OVR.h>

#include "gltools.h"
#include "glstereo.h"

// TODO
//  -improve user movement (rotate view with body? mouse-based?)
//  -lighting?
//
//  -aspect (projection) is all wrong with stereo disabled -- need a
//   different proj matrix
//
//  -set Distortion struct from HMD info
//  -rather than GetAttribLocation, use BindAttribLocation (do before linking)
//  -properly clean-up resources (buffers, textures, mallocs), and exit
//  -fix any remaining FIXME's and TODO's; check XXX's.
//  -make code easier to read


/* ---------------------------------------------------------------- */

/////////////////////////////////////////////////////////////////////////////////////////////
// Utils
/////////////////////////////////////////////////////////////////////////////////////////////

#define PRINT_GL_ERROR() printOGLError(__FILE__, __LINE__)

void printOGLError( char *file, int line )
{
    GLenum err = glGetError();
    if( err != GL_NO_ERROR )
        printf( "glError in file %s, line %d: %s (0x%08x)\n", file, line, gluErrorString(err), err );
}

// convert to OpenGL float matrix, from gl-matrix.c double
void oglMatrix( mat4_t m, float *f )
{
    int i;
    for( i=0;i<16;i++ ) f[i] = m[i];
}

// Elapsed time since glutInit(), in seconds
float simtime()
{
    return (float)( glutGet( GLUT_ELAPSED_TIME ) ) / 1000.0f;
}

// Fullscreen shader uses builtin vertex shader with specified fragment shader.
//
// I prefer PROCEDURAL_FULLSCREEN for simplicity and elegance, but I'm not
// sure if it's widely supported in practice (OpenGL3.3).
#define PROCEDURAL_FULLSCREEN
#ifdef PROCEDURAL_FULLSCREEN
char screenVertShader[] = "\
#version 330 core\n\
uniform mat4 toWorld;\
out vec3 ray;\
void main(void){\
    vec2 p = vec2( (gl_VertexID << 1) & 2, gl_VertexID & 2 ) - vec2(1.0);\
    ray = (toWorld * vec4( p, 1.0, 1.0 )).xyz;\
    gl_Position = vec4( p, 0.0, 1.0 );\
}";
#else
char screenVertShader[] = "\
#version 130\n\
uniform mat4 toWorld;\
in vec4 point;\
out vec3 ray;\
void main(void){\
    ray = (toWorld * vec4( p.xy, 1.0, 1.0 )).xyz;\
    gl_Position = point;\
}";
#endif

GLuint fullscreenShader( char *fname ){
    ShaderSource toLoad = { GL_FRAGMENT_SHADER, fname };
    GLuint shader[2];
    shader[0] = shaderCompile( GL_VERTEX_SHADER, "<builtin>screenVertShader", screenVertShader );
    shader[1] = shaderLoadAndCompile( &toLoad );
    return shaderLink( 2, shader );
}



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
void renderBindVao( Renderable *r )
{
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

void renderableAddTex( Renderable *r, char *uname, GLuint tex )
{
    r->tex = tex;
    r->u_tex = glGetUniformLocation( r->prog, uname );
}

// Assumes GL_TRIANGLES, u16 indices, and GL_STATIC_DRAW, since that's typical
Renderable renderableCreate( GLuint prog, VFormat *fmt, GLsizei vbytes, GLfloat *vdata, GLsizei ibytes, GLushort *idata, GLenum usage )
{
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

void renderObj( Renderable *r )
{
    if( r->tex ) texBind( 0, r->u_tex, r->tex );
    glBindVertexArray( r->vao );
    glDrawElements( r->mode, r->count, r->type, 0 );
    glBindVertexArray( 0 );
    if( r->tex ) texUnbind( 0 );
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Model structures, for representing deformable heightfield (or more)
/////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_ADJ 7

typedef struct {
   double p[3];  // current x,y,z
   double hgt;   // target height (y)
   double vel;   // current velocity
   unsigned short adj[MAX_ADJ];  // indices of adjacent faces
   unsigned short faces;         // number of adjacent faces
} Vert;

typedef struct {
   double n[3];
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
Model *genTerrain( GLuint prog, VFormat *fmt, int maxTess, double scale );
void   modelUpdate( Model *m, float dt );
double heightAt( Model *m, double x, double z, int *phint );



/////////////////////////////////////////////////////////////////////////////////////////////
// Global State
/////////////////////////////////////////////////////////////////////////////////////////////


// width and height of the window
GLsizei g_width = 1280;
GLsizei g_height = 800;


// Oculus Rift Device
Device *dev = NULL;

GLStereo *g_stereo = NULL;

RList *g_renderList = NULL;

Model *g_terrain = NULL;

GLuint g_skyshader = 0;

int g_inStereo = 0;


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

ShaderSource shdrDistortAA[] = {
    {GL_VERTEX_SHADER,   "ovr-post.vert"},
    {GL_FRAGMENT_SHADER, "ovr-distort-aa.frag"}
};

ShaderSource shdrDistortChromaAA[] = {
    {GL_VERTEX_SHADER,   "ovr-post.vert"},
    {GL_FRAGMENT_SHADER, "ovr-distortchroma-aa.frag"}
};


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
    glCullFace( GL_BACK );
    glEnable( GL_CULL_FACE );

    glBlendEquation( GL_FUNC_ADD );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_BLEND );
}

void toggleFullScreen()
{
    static int fullscreen = 0;
    if( fullscreen == 1 ){
        glutReshapeWindow( g_width, g_height );
        fullscreen = 0;
    }else{
        glutFullScreen();
        fullscreen = 1;
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Controls
/////////////////////////////////////////////////////////////////////////////////////////////

int g_keyw = 0;
int g_keya = 0;
int g_keys = 0;
int g_keyd = 0;

void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 27:  // Why the hell doesn't glut have GLUT_KEY_ESC?
        case 'q':
            exit( 0 );
            break;

        case 9: // TAB
            toggleFullScreen();
            break;

        case 'w': g_keyw = 1; break;
        case 'a': g_keya = 1; break;
        case 's': g_keys = 1; break;
        case 'd': g_keyd = 1; break;

        case '1':
            glStereoSetDistortCustom( g_stereo, shaderProgram( COUNTED_ARRAY(shdrDistort) ) );
            break;
        case '2':
            glStereoSetDistortCustom( g_stereo, shaderProgram( COUNTED_ARRAY(shdrDistortAA) ) );
            break;
        case '3':
            glStereoSetDistortCustom( g_stereo, shaderProgram( COUNTED_ARRAY(shdrDistortChroma) ) );
            break;
        case '4':
            glStereoSetDistortCustom( g_stereo, shaderProgram( COUNTED_ARRAY(shdrDistortChromaAA) ) );
            break;

        default:
            break;

    }

    glutPostRedisplay( );
}

// key down/up without repeat...
void keyReleaseFunc( unsigned char key, int x, int y ){
    switch( key )
    {
        case 'w': g_keyw = 0; break;
        case 'a': g_keya = 0; break;
        case 's': g_keys = 0; break;
        case 'd': g_keyd = 0; break;
        default: break;
    }
}

void mouseFunc( int button, int state, int x, int y )
{
    glutPostRedisplay( );
}


// heading is turned into xz-direction
void vecOfHeading( double heading, double step, double *dir )
{
    double s = sin(heading);
    double c = cos(heading);
    dir[0] = -s * step;
    dir[1] = 0.;
    dir[2] = -c * step;
}

// user movement with terrain-loft
void move( double pos[3] )
{
    static double heading = 0.;
    static int localVIdx = 0; // cached nearest-vertex to feet

    double da_step = g_keya? (g_keyd? 0.:0.05) : (g_keyd? -0.05:0.);
    heading+= da_step;

    double step = g_keyw? (g_keys? 0.:0.05) : (g_keys? -0.02:0.);
    double v[3];
    vecOfHeading( heading, step, v );
    vec3_add( pos, v, NULL );

    pos[1] = 1.8f + heightAt( g_terrain, pos[0], pos[2], &localVIdx );
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Rendering
/////////////////////////////////////////////////////////////////////////////////////////////

// Sky render
//  -this is handled as a full-screen "raytrace" style shader
//  -'mtx' is the inverse view-proj matrix, to bring the frustum corners into
//    world-space; interpolated between verts, this provides a ray per pixel
static void skyRender( float *mtx, float time, GLuint shader )
{
    static GLuint vao = 0;

#ifdef PROCEDURAL_FULLSCREEN
    if( !vao ) glGenVertexArrays( 1, &vao );
#else
    static GLuint vbo = 0;
    static VDesc desc[] = { {"point", 3} };
    static VFormat v3; // vertex format only used for screen-quad
    static float screenVerts[] =
    { -1.0, -1.0,  0.0,
       1.0, -1.0,  0.0,
      -1.0,  1.0,  0.0,
       1.0,  1.0,  0.0 };

    if( !vbo )
    {
        v3 = vtxNewFormat( shader, COUNTED_ARRAY(desc) );
        vbo = newBufObj( GL_ARRAY_BUFFER, BYTE_ARRAY(screenVerts), GL_STATIC_DRAW );
        glBindBuffer( GL_ARRAY_BUFFER, vbo );
        glGenVertexArrays( 1, &vao );
        glBindVertexArray( vao );
        vtxEnable( &v3 );
        glBindVertexArray( 0 );
        vtxDisable( &v3 );
        glBindBuffer( GL_ARRAY_BUFFER, 0 );
    }
#endif

    GLint u_toWorld = glGetUniformLocation( shader, "toWorld" );
    GLint u_time = glGetUniformLocation( shader, "time" );

    glUseProgram( shader );

    glUniformMatrix4fv( u_toWorld, 1, GL_FALSE, mtx );
    glUniform1fv( u_time, 1, &time );


    // Render 

    glDisable( GL_BLEND );
    glDepthMask( GL_FALSE );

    glBindVertexArray( vao );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
    glBindVertexArray( 0 );

    glDepthMask( GL_TRUE );
    glEnable( GL_BLEND );
    glUseProgram( 0 );
}


void render( mat4_t view, mat4_t proj, void *data )
{
    RList *r = g_renderList;
    double dMtx[16];
    float  fMtx[16];
    GLint u_mvp;
    GLint u_mv;
    GLuint prog = 0;

    // Skysphere
    mat4_multiply( proj, view, dMtx );
    mat4_inverse( dMtx, NULL );
    oglMatrix( dMtx, fMtx );
    skyRender( fMtx, simtime(), g_skyshader );

    // Scene
    while( r )
    {
        if( r->rend->prog != prog )
        {
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

void updateFunc( )
{
    static double headpos[3] = { 0., 0., 0. }; // world-pos of base of head
    static float tprev = 0.f;

    float t = simtime();
    float dt = t - tprev;
    if( dt > 0.05 ) dt = 0.05;
    tprev = t;
    
    modelUpdate( g_terrain, dt );
    move( headpos );

    if( g_inStereo )
        glStereoRender( g_stereo, headpos, render );
    else
        glStereoRenderMono( g_stereo, headpos, render );

    glFlush();
    glutSwapBuffers();
    glutPostRedisplay();
}


void idleFunc( )
{
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
    printf("\tProductID: 0x%04hx\n\n", localDev->productId);

    printf("ESC or 'q' to quit\n");
    printf("TAB to toggle fullscreen/window\n\n");

    printf("Keys to select distortion-shader:\n");
    printf("\t'1' basic distortion shader\n");
    printf("\t'2' +antialias\n");
    printf("\t'3' +chromatic aberration correction (default)\n");
    printf("\t'4' +chromatic aberration correction and green-antialias\n\n");

    printf("Movement of body: w,a,s,d\n");
    printf("\tw,a - forward or backward\n");
    printf("\ts,d - turning left or right\n");
    printf("\t--yes, movement is hacky and completely decoupled from view right now\n\n");

    if( !g_inStereo )
        printf("\tTip: use command-line option --rift for use with the Rift!\n\n");

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



// Procedural texture...

GLuint proceduralTex( int w, int h, void (*fn)(double,double,double,unsigned char*) );
double perlin( double x, double y, double z );

void fn1( double x, double y, double z, unsigned char *d )
{
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

void usage( char *progname ){
    printf("\n");
    printf("%s [options]\n", progname );
    printf("Options:\n");
    printf(" --res <w>x<h>     -\n");
    printf(" --fullscreen      -start in fullscreen mode (TAB still toggles)\n");
    printf(" --stereo          -stereo split, with barrel distortion\n");
    printf(" --rift            -same as fullscreen and stereo, res based on hardware\n");
    printf(" --sky <file.glsl> -specify fragment shader to use for sky\n");
    printf("\n");
}

//-----------------------------------------------------------------------------
// Name: main( )
// Desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    char skyfile[256] = "sky.frag";
    int fullscreen = 0;

    char *progname = argv[0];
    while( argc > 1 )
    {
        if( !strncmp( argv[1],"--res",5 ) ){
            sscanf( argv[2],"%dx%d", &g_width, &g_height );
            argv++; argc--;
        }else
        if( !strncmp( argv[1],"--fullscreen",12 ) ){
            fullscreen = 1;
        }else
        if( !strncmp( argv[1],"--stereo",8 ) ){
            g_inStereo = 1;
        }else
        if( !strncmp( argv[1],"--rift",6 ) ){
            fullscreen = 1;
            g_inStereo = 1;
        }else
        if( !strncmp( argv[1],"--sky",5 ) ){
            sscanf( argv[2],"%255s", skyfile );
            argv++; argc--;
        }else{
            if( strncmp( argv[1],"--help",6 ) )
                printf( "Unrecognized option: %s\n", argv[1] );
            usage( progname );
            exit(1);
        }
        argv++; argc--;
    }


    dev = (Device *)malloc(sizeof(Device));
    runSensorUpdateThread(dev);

    glutInitContextVersion( 3, 3 );
    glutInitContextFlags( GLUT_CORE_PROFILE | GLUT_DEBUG );
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowSize( g_width, g_height );
    glutInitWindowPosition( 0, 0 );
    glutCreateWindow( argv[0] );

    if( fullscreen )
        toggleFullScreen();

    // Set up our callbacks
    glutIdleFunc( idleFunc );
    glutDisplayFunc( updateFunc );
    glutKeyboardFunc( keyboardFunc );
    glutMouseFunc( mouseFunc );

    glutIgnoreKeyRepeat( 1 );
    glutKeyboardUpFunc( keyReleaseFunc );


    initGL();

    PRINT_GL_ERROR();

    g_skyshader = fullscreenShader( skyfile );

    /* XXX The following are "local variables" which must persist...
     * these functions can be made to allocate from the heap instead, with
     * requirement for corresponding free(). Then this lump could be moved to
     * initialize(). */

    // Compile a shader program
    // -vertex format and rendering are dependent on this
    // -note, it's possible to use different shaders with model/verts if the
    //  use of uniform variables is consistent
    GLuint shader = shaderProgram( COUNTED_ARRAY(shdrNoLight) );

    // New vertex format
    VDesc desc[] =
    {  {"point", 3},
       {"rgba",  4},
       {"uv01",  2}  };
    VFormat v3c4t2 = vtxNewFormat( shader, COUNTED_ARRAY(desc) );

    // Create some textures
    GLuint tex = proceduralTex( 512, 512, fn1 );

    // Terrain
    int maxTess = 7; // to go higher would require larger index format (uint32)
    double scale = 100.; // terrain is within a triangle, scale*2 meters per side
    g_terrain = genTerrain( shader, &v3c4t2, maxTess, scale );
    renderableAddTex( &g_terrain->r, "rgbaMap", tex );

    RList r0;
    r0.rend = &g_terrain->r;
    mat4_identity( r0.mtx );
    r0.next = NULL;
    g_renderList = &r0;

    // FIXME is there a nice way to support non-stereo (and do we want to)
    // with "glStereo"? Or should it be purely the stereo abstraction? If so,
    // should the rift orientation/headmodel be extracted?
    g_stereo = glStereoCreate( dev, 1600, 1000, DISTORT_RADIAL );

    // Go Glut
    glutMainLoop();

    glStereoDestroy( g_stereo );

    return 0;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Procedural Texturing
/////////////////////////////////////////////////////////////////////////////////////////////

GLuint proceduralTex( int w, int h, void (*fn)(double,double,double,unsigned char*) )
{
    GLuint tex = 0;
    unsigned char *texdata = malloc( w*h*4 );
    unsigned char *row = texdata;
    double step = 1. / (float)w;
    double x,y;
    int i, j;
    for( j=0, y=0.; j<h; j++, y+=step )
    {
        for( i=0, x=0.; i<w; i++, x+=step )
            fn( x, y, 0., row+i*4 );
        row+= w * 4;
    }
    tex = texCreateMip( TEX_BYTE4, w, h, GL_LINEAR, TEX_MIRROR, texdata );
    free( texdata );
    return tex;
}

/* Perlin's noise function, with permutation table */
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
static double grad(int hash, double x, double y, double z)
{
    int h = hash & 15;
    double u = h<8 ? x : y,
           v = h<4 ? y : h==12||h==14 ? x : z;
    return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

double perlin( double x, double y, double z )
{
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

uint32_t bitmaskFill( uint32_t n )
{
    n|= n >> 1;
    n|= n >> 2;
    n|= n >> 4;
    n|= n >> 8;
    return n | (n >> 16);
}

uint32_t roundPow2( uint32_t n ){ return 1 + bitmaskFill(n-1); }
int isPow2( uint32_t n ){ return ((n & (n-1)) == 0); }
uint32_t sumToN( uint32_t n ){ return ((n*n+n) >> 1); }

uint32_t interleaveBits( uint32_t x, uint32_t y )
{
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
//  -using this to lookup vertices for sharing

typedef struct {
    uint32_t mask; // size-1, where size is 2^n
    uint32_t table[1];
} Hashtable;

Hashtable *hashCreate( uint32_t size )
{
    Hashtable *h = NULL;
    if( size > 0 && isPow2(size) )
    {
        h = (Hashtable*)calloc( sizeof(Hashtable) + (size-1) * sizeof(uint32_t), 1 );
        h->mask = size-1;
    }
    return h;
}

uint32_t hashHsieh( uint32_t hash )
{
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
uint32_t hashIndex( Hashtable *h, uint32_t key )
{
    uint32_t idx = hashHsieh(key) & h->mask;
    uint32_t *ht = h->table;
    while( ht[idx] )
    {
        if( ht[idx] == key ) return idx;
        idx = (idx+1) & h->mask;
    }
    ht[idx] = key;
    return idx;
}


// Tesselation

typedef double (*HeightFn)(double*,double,double);

void initVertXZ( Vert *v, double x, double z )
{
    memset( v, 0, sizeof(Vert) );
    v->p[0] = x;
    v->p[2] = z;
    v->faces = 0;
}

void adjAdd( Vert *v, int iAdd )
{
    int i;
    for( i=0;i<v->faces;i++ )
        if( v->adj[i] == iAdd ) return;
    assert( v->faces < MAX_ADJ );
    v->adj[i] = iAdd;
    v->faces++;
}

void initFace( Vert *v, Face *f, int i, int a, int b, int c )
{
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
void midVert( HeightFn fn, Vert *v, int ia, int ib, int ic )
{
    Vert *a = v+ia;
    Vert *b = v+ib;
    Vert *c = v+ic;

    c->p[0] = (a->p[0] + b->p[0]) * 0.5;
    c->p[1] = (a->p[1] + b->p[1]) * 0.5;
    c->p[2] = (a->p[2] + b->p[2]) * 0.5;
    double dx = (a->p[0] - b->p[0]);
    double dz = (a->p[2] - b->p[2]);
    double edgeLen = sqrt( dx*dx + dz*dz );
    double targetHeight = (a->hgt + b->hgt) * 0.5;
    c->hgt = (*fn)( c->p, edgeLen, targetHeight );
    c->vel = 0.;
    c->faces = 0;
}

// Edge is directionless: a->b and b->a define the same edge.
uint32_t edgeId( uint32_t a, uint32_t b )
{
    return a < b ? interleaveBits( a, b )
                 : interleaveBits( b, a );
}

// given current vertex state, fracture into new vertices with target heights
// return new count
// -for every edge (ordered vertex pair), have a mapping to its midpoint vertex
// -index list is completely recreated on tesselate
// XXX 'v' must be large enough to contain 4*verts; 'index' must be malloc'd
int tesselate( HeightFn fn, int verts, Vert *v, int *pfaces, Face **pface )
{
    int faces = *pfaces;
    Face *face = *pface;

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
    for( i=0; i<verts; i++ )
        v[i].faces = 0;

    for( i=0; i<faces; i++ )
    {
        int a = face[i].a;
        int b = face[i].b;
        int c = face[i].c;

        // a,b,c are the vertex indices for this triangle... now create or find the
        // vertices between them, to create 4 new triangles with.
        uint32_t n;
        n = hashIndex( h, edgeId(a,b) );
        if( edgeMap[n] < 0 )
        {
            midVert( fn, v, a, b, verts );
            edgeMap[n] = verts++;
        }
        int ab = edgeMap[n];

        n = hashIndex( h, edgeId(b,c) );
        if( edgeMap[n] < 0 )
        {
            midVert( fn, v, b, c, verts );
            edgeMap[n] = verts++;
        }
        int bc = edgeMap[n];

        n = hashIndex( h, edgeId(c,a) );
        if( edgeMap[n] < 0 )
        {
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
    return verts;
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Terrain Generation
/////////////////////////////////////////////////////////////////////////////////////////////


// Color functions

void cMix( float t, float *a, float *b, float *r )
{
    int i;
    for( i=0; i<4; i++ )
        r[i] = t * (b[i] - a[i]) + a[i];
}

// mix from -1 (lo) to +1 (hi)
void cMixSigned( float *rgba, float *lo, float *hi, float mix )
{
    if( mix < 0.f )
        cMix( -mix, rgba, lo, rgba );
    else
        cMix( mix, rgba, hi, rgba );
}

void cMap( int entries, float map[][5], float t, float *rgba ){
    int i = 0;
    while( i < entries && t > map[i][0] ) i++;
    if( i <= 0 || i >= entries )
    {
        if( i >= entries ) i--;
        memcpy( rgba, &(map[i][1]), 4*sizeof(float) );
        return;
    }
    i--;
    t = (t - map[i][0]) / (map[i+1][0] - map[i][0]);
    t = (3.f-t-t) * t * t;  // s-curve
    cMix( t, &(map[i][1]), &(map[i+1][1]), rgba );
}

float terrainColor[][5] = {
    { 0.00,   0.4, 1.0, 0.2, 1.0 },
    { 0.20,   0.1, 1.0, 0.2, 1.0 },
    { 0.35,   0.0, 0.8, 0.0, 1.0 },
    { 0.42,   0.0, 0.8, 0.0, 1.0 },
    { 0.50,   0.7, 0.5, 0.2, 1.0 },
    { 0.55,   0.7, 0.7, 0.6, 1.0 },
    { 0.85,   0.6, 0.6, 0.6, 1.0 },
    { 0.90,   1.0, 1.0, 1.0, 1.0 },
    { 1.00,   1.0, 1.0, 1.0, 1.0 }
};



double randf( void )
{
    static double rscale = 1./(double)RAND_MAX;
    return (double)(rand())*rscale;
}

void faceMidpoint( Vert *v, Face *f, double *m )
{
    vec3_set( v[f->a].p, m );
    vec3_add( m, v[f->b].p, NULL );
    vec3_add( m, v[f->c].p, NULL );
    vec3_scale( m, 0.3333333333, NULL );
}

// add non-duplicate entry 'n'
int adjacentAdd( int *a, int count, int vIdx, int n )
{
    if( n != vIdx )
    {
        int i = 0; do{
            if( i == count ){ a[count++] = n; break; }
        }while( a[i++] != n );
    }
    return count;
}

// fills 'a' with indices of adjacent vertices to vIdx, returning number
int adjacent( Model *m, int vIdx, int *a )
{
    int i, count=0;
    Vert *v = &m->v[vIdx];
    for( i=0; i<v->faces; i++ )
    {
        Face *f = &m->f[v->adj[i]];
        count = adjacentAdd( a, count, vIdx, f->a );
        count = adjacentAdd( a, count, vIdx, f->b );
        count = adjacentAdd( a, count, vIdx, f->c );
    }
    return count;
}

double distSqXZ( double *a, double *b )
{
    double dx = b[0] - a[0];
    double dz = b[2] - a[2];
    return dx*dx + dz*dz;
}

// hint is a vertex index to start at; which is also set to closest found vertex
// NOTE face index can't be used since they can change with tesselation
double heightAt( Model *m, double x, double z, int *phint )
{
    // Since vertices have no organization, I effectively have to test
    // each triangle... well, not quite: I can walk in the right direction

    // find support triangle
    int vi = *phint;
    Vert *v = m->v;

    // from here I can get all adjacent faces
    // from faces I can move in direction of x,z which might bound it
    double p[3] = { x, 0., z };

    double closest = distSqXZ( v[vi].p, p );
    int adj[16];
    int i,n,closer;
    do{
        closer = 0;
        n = adjacent( m, vi, adj );
        for( i=0; i<n; i++ )
        {
            int vj = adj[i];
            double d = distSqXZ( v[vj].p, p );
            if( d < closest )
            {
                vi = vj;
                closest = d;
                closer = 1;
            }
        }
    }while( closer );
    // now vi is index of closest vertex, and closest is distSqXZ of it
    // also, adj is a valid adjacency list

    // let's just get the next two closest verts and use this as a triangle
    int a = adj[0]; double da = distSqXZ( v[a].p, p );
    int b = adj[1]; double db = distSqXZ( v[b].p, p );
    if( db < da )
    { // swap so a is closer than b
        double t = da; da = db; db = t;
        a = b; b = adj[0];
    }
    for( i=2; i<n; i++ )
    {
        int vj = adj[i];
        double d = distSqXZ( v[vj].p, p );
        if( d < db )
        {
            if( d < da )
            {
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
    double va[3],vb[3],norm[3];
    vec3_set( v[a].p, va );
    vec3_set( v[b].p, vb );
    vec3_subtract( va, v[vi].p, NULL );
    vec3_subtract( vb, v[vi].p, NULL );
    vec3_cross( va, vb, norm );
    vec3_normalize( norm, NULL );
    double d = vec3_dot( v[vi].p, norm );

    double y = (d - norm[0]*x - norm[2]*z) / norm[1];
    return y;
}

void vertColor( Model *m, int idx, double peakHeight, float *rgba )
{
    // use height to lookup color; modify by local curvature
    Vert *vtx = m->v+idx;

    // calculate vertex normal
    double n[3] = {0., 0., 0.};
    int i;
    for( i=0; i<vtx->faces; i++ )
    {
        unsigned short j = vtx->adj[i];
        vec3_add( n, m->f[j].n, NULL );
    }
    vec3_normalize( n, NULL );

    // curvature... face midpoint relative to p; dotprod with n
    double curvature = 0.f;
    if( vtx->faces > 0 )
    {
        double mid[3];
        for( i=0; i<vtx->faces; i++ )
        {
            unsigned short j = vtx->adj[i];
            faceMidpoint( m->v, m->f+j, mid );
            vec3_subtract( mid, vtx->p, NULL );
            vec3_normalize( mid, NULL );
            curvature-= vec3_dot( mid, n );
        }
        curvature /= (double)(vtx->faces);
    }

    float t = vtx->p[1] / peakHeight;
    cMap( COUNTED_ARRAY(terrainColor), t, rgba );

    float lo[] = { 0.f, 0.f, 0.f, 1.f };
    float hi[] = { 1.f, 1.f, 1.f, 1.f };
    cMixSigned( rgba, lo, hi, (float)(curvature<0.?-sqrt(-curvature):sqrt(curvature)) );
}

void modelUpdateGL( Model *m, int vStart, int vEnd )
{
    int i;
    Vert *v = m->v;
    Face *f = m->f;
    //printf(" modelUpdateGL lo/hi = %d/%d\n", vStart, vEnd );

    if( vStart <= vEnd )
    {
        // XXX forcing all verts to update so colors are updated
        vStart = 0;
        vEnd = m->verts - 1;

        // calc face-normals
        double va[3],vb[3];
        for( i=0; i<m->faces; i++ )
        {
            unsigned short a = f[i].a, b = f[i].b, c = f[i].c;
            vec3_set( v[a].p, va );
            vec3_set( v[b].p, vb );
            vec3_subtract( va, v[c].p, NULL );
            vec3_subtract( vb, v[c].p, NULL );
            vec3_cross( va, vb, f[i].n );
            vec3_normalize( f[i].n, NULL );
        }

        // vdata in v3c4t2 format
        float *d;
        // XXX to get vertColor right, this should be recomputed for every vertex
        for( i=vStart; i<=vEnd; i++ )
        {
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

    if( m->updateIndices )
    {
        // fill the index array
        unsigned short *p = m->idata;
        for( i=0; i<m->faces; i++ )
        {
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

void modelTesselate( Model *m, HeightFn fn )
{
    m->verts = tesselate( fn, m->verts, m->v, &m->faces, &m->f );
    m->updateIndices = 1;
}

double heightMid( double *p, double edgeLen, double hgt ){
    return hgt;
}

double heightPerlin( double *p, double edgeLen, double hgt ){
    return hgt + 0.4 * edgeLen * perlin( p[0], p[1], p[2]+3.1f );
}

Model *genTerrain( GLuint prog, VFormat *fmt, int maxTess, double scale )
{
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
    for( i=0; i<m->verts; i++ )
    {
        Vert *v = &m->v[i];
        double x = v->p[0];
        double z = v->p[2];
        double d = sqrt( x*x + z*z );
        double h = d < scale*0.3? 0. : 25.f * ( 1.f - cosf( d * 3.f/70.f ) );
        v->hgt = heightPerlin( v->p, scale*0.5f, h );
    }

    modelUpdateGL( m, 0, m->verts-1 );

    return m;
}

void modelUpdate( Model *m, float dt )
{
    // animate vertex height
    // first: step by a fixed velocity toward hgt, or stop at hgt
    static float time = 0.f;
    static int tess = 2;
    time+= dt;
    if( tess < 7 && time > 3.f )
    {
        modelTesselate( m, heightPerlin );
        tess++;
        time = 0.f;
    }

    // track low and high watermark of changed vertices
    int lo = m->verts;
    int hi = 0;
    int i;
    for( i=0; i<m->verts; i++ )
    {
        Vert *v = &m->v[i];
        double dy = v->hgt - v->p[1];
        if( fabs(dy) > 0.01 )
        {
            if( i<lo ) lo = i;
            if( i>hi ) hi = i;
            dy-= v->vel * dt;
            double vel = dy * (0.2 + 0.8/(1.+fabs(dy))); // NOTE without fabs, interesting columns form
            v->vel = vel;
            v->p[1]+= vel * dt;
        }
    }
    modelUpdateGL( m, lo, hi );
}


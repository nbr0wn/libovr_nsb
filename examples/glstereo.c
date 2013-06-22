/* libovr-nsb/glstereo
 *
 * Author: Anthony Tavener
 *
 * Simplified stereo-rendering interface for the Rift.
 *
 * This file falls under the Oculus license as code within
 * is derived from parts of the OculusVR SDK.
 *
 */

#include "glstereo.h"

/* TODO
 *
 *  -render pos and orient should be controllable
 *    -what assumptions do we make about eye/head/neck?
 *    -currently a simple head/neck model positions two eyes
 *  -general cleanup and API improvement
 *
 */


/* Built-in distortion shaders. Derived from OculusVR shaders.
 */

char shaderVert[] = "\
#version 130\n\
in vec4 point;\
in vec2 uv01;\
uniform mat4 view;\
uniform mat4 texm;\
out vec2 v_uv;\
void main(void){\
    v_uv = (texm * vec4(uv01,0.,1.)).st;\
    gl_Position = view * point;\
}";

char shaderDistort[] = "\
#version 130\n\
uniform sampler2D texSrc;\
uniform vec2 lensCenter;\
uniform vec2 screenCenter;\
uniform vec2 scale;\
uniform vec2 scaleIn;\
uniform vec4 distortK;\
in vec2 v_uv;\
void main(void){\
    vec2  theta = (v_uv - lensCenter) * scaleIn;\
    float r2 = theta.x * theta.x + theta.y * theta.y;\
    float distort = distortK.x + (distortK.y + (distortK.z + distortK.w * r2) * r2) * r2;\
    vec2  uv = lensCenter + theta * (distort * scale);\
    vec2 bound = vec2(0.25,0.5);\
    if(any(lessThan(uv, screenCenter-bound))) discard;\
    if(any(greaterThan(uv, screenCenter+bound))) discard;\
    gl_FragColor = texture2D( texSrc, uv );\
}";

char shaderDistortChroma[] = "\
#version 130\n\
uniform sampler2D texSrc;\
uniform vec2 lensCenter;\
uniform vec2 screenCenter;\
uniform vec2 scale;\
uniform vec2 scaleIn;\
uniform vec4 distortK;\
uniform vec4 chromaK;\
in vec2 v_uv;\
void main(){\
    vec2  theta = (v_uv - lensCenter) * scaleIn;\
    float r2 = theta.x * theta.x + theta.y * theta.y;\
    float distort = distortK.x + (distortK.y + (distortK.z + distortK.w * r2) * r2) * r2;\
    vec2  theta1 = theta * (distort * scale);\
    vec2 uvBlue = lensCenter + theta1 * (chromaK.z + chromaK.w * r2);\
    vec2 bound = vec2(0.25,0.5);\
    if(any(lessThan(uvBlue, screenCenter-bound))) discard;\
    if(any(greaterThan(uvBlue, screenCenter+bound))) discard;\
    float blue = texture2D( texSrc, uvBlue ).b;\
    vec2 uvGreen = lensCenter + theta1;\
    float green = texture2D( texSrc, uvGreen ).g;\
    vec2 uvRed = lensCenter + theta1 * (chromaK.x + chromaK.y * r2);\
    float red = texture2D( texSrc, uvRed ).r;\
    gl_FragColor = vec4( red, green, blue, 1 );\
}";



// Inverse reference quaternion
// - this is the inverse of a reference orientation
// - intially set with a reference orientation of 90deg pitch which seems to
//   correspond to the Rift's natural orientation
double g_qref[4] = { -0.7071, 0., 0., 0.7071 };



/////////////////////////////////////////////////////////////////////////////////////////////
// Stereo-lens functions translated from OVR SDK
/////////////////////////////////////////////////////////////////////////////////////////////


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

float distortionFn( Distortion *d, float r )
{
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

void projectionMatrix( View *vp, SensorDisplayInfo *hmd, Distortion *d, int stereo, mat4_t m )
{
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


/////////////////////////////////////////////////////////////////////////////////////////////


void defaultSensorDisplayInfo( SensorDisplayInfo *s )
{
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
SensorDisplayInfo *getDisplayInfo( Device *d )
{
   if( d )
      return &d->sensorInfo;
   else
      return &defaultDisplayInfo;
}



static void mapDistortion( GLStereo *sr, GLuint src, Distortion *d, View *v, View *target )
{
    static GLint u_texSrc, u_lensCenter, u_screenCenter, u_scale, u_scaleIn, u_distortK, u_chromaK;
    static GLint u_viewm, u_texm;
    static GLuint distortShader = 0;
    static GLuint vao = 0;
    static GLuint vbo = 0;
    static VDesc desc[] = { {"point", 3}, {"uv01", 2} };
    static VFormat v3t2; // vertex format only used for screen-quad
    static float screenVerts[] =
    {   0.0,  0.0,  0.0,   0., 0.,
        1.0,  0.0,  0.0,   1., 0.,
        0.0,  1.0,  0.0,   0., 1.,
        1.0,  1.0,  0.0,   1., 1. };

    // One-time setup, which could be better done as a distortion-mapping state
    if( distortShader != sr->shader )
    {
        distortShader = sr->shader;

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
    }

    if( !vbo )
    {
        v3t2 = vtxNewFormat( distortShader, COUNTED_ARRAY(desc) );
        vbo = newBufObj( GL_ARRAY_BUFFER, BYTE_ARRAY(screenVerts), GL_STATIC_DRAW );

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
    }


    glUseProgram( distortShader );

    float w = (float)(v->wid) / (float)(target->wid);
    float h = (float)(v->hgt) / (float)(target->hgt);
    float x = (float)(v->x0) / (float)(target->wid);
    float y = (float)(v->y0) / (float)(target->hgt);
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
    glUniform4fv( u_distortK, 1, d->K );
    glUniform4fv( u_chromaK, 1, d->ChromaticAberration );
    */

    // glUniform*f functions weren't working -- uniforms would not be set;
    // reading back values would show garbage. *fv versions seem to work...
    float a[2];
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
static void viewOfPosOrient( vec3_t p, quat_t q, mat4_t dst )
{
    double qinv[4];
    double neg[3];
    vec3_negate( p, neg );
    quat_conjugate( q, qinv );
    quat_multiplyVec3( qinv, neg, NULL );
    mat4_fromRotationTranslation( qinv, neg, dst );
}


static void clearDistortProgram( GLStereo *sr ){
    if( sr->shader && sr->distort != DISTORT_CUSTOM ){
        glDeleteProgram( sr->shader );
        sr->shader = 0;
    }
}


void glStereoSetDistort( GLStereo *sr, DistortKind distort )
{
    if( !sr->shader || sr->distort != distort )
    {
        GLuint shader[2];
        clearDistortProgram( sr );
        switch(distort)
        {
            case DISTORT_RADIAL:
                shader[0] = shaderCompile( GL_VERTEX_SHADER, "<builtin>distort.vert", shaderVert );
                shader[1] = shaderCompile( GL_FRAGMENT_SHADER, "<builtin>distort.frag", shaderDistort );
                sr->shader = shaderLink( 2, shader );
                sr->distort = distort;
                break;
            case DISTORT_RADIAL_CHROMA:
                shader[0] = shaderCompile( GL_VERTEX_SHADER, "<builtin>distort.vert", shaderVert );
                shader[1] = shaderCompile( GL_FRAGMENT_SHADER, "<builtin>distort-chroma.frag", shaderDistortChroma );
                sr->shader = shaderLink( 2, shader );
                sr->distort = distort;
                break;
            case DISTORT_CUSTOM:
                fprintf(stderr,"glStereoSetDistort: use glStereoSetDistortCustom for DISTORT_CUSTOM.\n");
                break;
            default:
                fprintf(stderr,"glStereoSetDistort: unsupported DistortKind %d\n",distort);
                break;
        }
    }
}

void glStereoSetDistortCustom( GLStereo *sr, GLuint shader )
{
    clearDistortProgram( sr );
    sr->distort = DISTORT_CUSTOM;
    sr->shader = shader;
}

static GLStereo *create( Device *dev, int rendw, int rendh )
{
    defaultSensorDisplayInfo( &defaultDisplayInfo ); // FIXME

    GLStereo *sr = (GLStereo*)malloc( sizeof(GLStereo) );

    SensorDisplayInfo *displayInfo = getDisplayInfo( dev ); // FIXME
    Distortion *distort = &g_defaultDistort; // FIXME

    sr->dev = dev;
    sr->shader = 0;

    viewSetFrameBuffer( &sr->framebuffer, displayInfo->HResolution, displayInfo->VResolution );
    viewSetOffscreenBuffer( &sr->backbuffer, GL_LINEAR, rendw, rendh );

    // distortionFitXY... coordinates of a point to 'fit' to the distorted
    // display -- so, -1,0 meaning to keep the leftmost point in-view
    float x = -1.0f;
    float y =  0.0f;
    distortionUpdateOffsetAndScale( distort, displayInfo, &sr->framebuffer, x, y );

    int stereo = 1;
    projectionMatrix( &sr->framebuffer, displayInfo, distort, stereo, sr->proj );

    return sr;
}

GLStereo *glStereoCreate( Device *dev, int rendw, int rendh, DistortKind distort )
{
    GLStereo *sr = create( dev, rendw, rendh );
    glStereoSetDistort( sr, distort );
    return sr;
}

GLStereo *glStereoCreateCustom( Device *dev, int rendw, int rendh, GLuint distortShader )
{
    GLStereo *sr = create( dev, rendw, rendh );
    glStereoSetDistortCustom( sr, distortShader );
    return sr;
}

void glStereoRender( GLStereo *sr, double pos[3], render_fn render )
{
    double eye[16];
    View *bb = &sr->backbuffer;
    View *fb = &sr->framebuffer;

    if( !bb->offscreen || !fboBind(bb->fbo) )
    {
        error();
        return;
    }

    // Orientation, from HMD sensor
    double orient[4] = { 0., 0., 0., 1. };
    if( sr->dev )
    {
        quat_set( sr->dev->Q, orient );
        quat_multiply( orient, g_qref, NULL ); // apply inverse-reference orientation
    }

    /* Offscreen stereo render with distortion */

    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    double proj[16];

    double ipd = 0.064; // inter-pupillary distance
    double leftEye[3] = { -0.5*ipd, 0.15, 0.1 };
    double rightEye[3] = { 0.5*ipd, 0.15, 0.1 };

    double offs = stereoProjectionOffset( getDisplayInfo(sr->dev) );
    int halfwid = bb->wid >> 1;


    /*** Left Eye ***/

    glViewport( 0.0f, 0.0f, halfwid, bb->hgt );

    // offset projection matrix for left lens
    mat4_identity( proj ); proj[12] = offs;
    mat4_multiply( proj, sr->proj, NULL );

    // build view matrix for left eye
    quat_multiplyVec3( orient, leftEye, NULL );
    vec3_add( leftEye, pos, NULL );
    viewOfPosOrient( leftEye, orient, eye );

    render( eye, proj, NULL );


    /*** Right Eye ***/

    glViewport( halfwid, 0.0f, halfwid, bb->hgt );

    // offset projection matrix for right lens
    mat4_identity( proj ); proj[12] = -offs;
    mat4_multiply( proj, sr->proj, NULL );

    // build view matrix for right eye
    quat_multiplyVec3( orient, rightEye, NULL );
    vec3_add( rightEye, pos, NULL );
    viewOfPosOrient( rightEye, orient, eye );

    render( eye, proj, NULL );

    fboUnbind(); // return to framebuffer


    /* distort from backbuffer to framebuffer */

    View v; memset( &v, 0, sizeof(View) );
    Distortion distort = g_defaultDistort;

    v.wid = fb->wid/2.; v.hgt = fb->hgt;
    mapDistortion( sr, bb->rgba, &distort, &v, fb );

    v.x0 = v.wid;
    distort.XCenterOffset = -distort.XCenterOffset;
    mapDistortion( sr, bb->rgba, &distort, &v, fb );

}

void glStereoRenderMono( GLStereo *sr, double pos[3], render_fn render )
{
    double eye[16];
    View *fb = &sr->framebuffer;

    // Orientation, from HMD sensor
    double orient[4] = { 0., 0., 0., 1. };
    if( sr->dev )
    {
        quat_set( sr->dev->Q, orient );
        quat_multiply( orient, g_qref, NULL ); // apply inverse-reference orientation
    }

    /* Direct framebuffer render with no stereo or distortion */
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glViewport( 0.0f, 0.0f, fb->wid, fb->hgt );
    viewOfPosOrient( pos, orient, eye );
    render( eye, sr->proj, NULL );
}

void glStereoDestroy( GLStereo *sr )
{
    viewFreeOffscreenBuffer( &sr->framebuffer );
    viewFreeOffscreenBuffer( &sr->backbuffer );
    clearDistortProgram( sr );
    free( sr );
}


/* libovr-nsb/gltools
 *
 * Author: Anthony Tavener
 *
 * Various OpenGL tools for simplifying common usage...
 * Loading shaders, creating buffers, defining vertex formats.
 *
 */


#include <stdio.h>
#include "gltools.h"

// return file-length
static long flen( FILE *fp )
{
    fseek( fp, 0L, SEEK_END );
    long len = ftell( fp );
    rewind( fp );
    return len;
}

// allocates a buffer and reads in a text file -- used for shader loading
static char *loadText( char *fname )
{
    char *text = NULL;
    FILE *fp = fopen( fname, "r" );
    if( fp )
    {
        long len = flen( fp );
        text = malloc( len+1 );
        fread( text, 1, len, fp );
        fclose( fp );
        text[len] = 0;  // null-terminate string
    }
    else
    {
        printf( "Error opening '%s'.\n", fname );
    }
    return text;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// GL Shader Load, Compile, Link
/////////////////////////////////////////////////////////////////////////////////////////////

GLuint shaderCompile( GLuint type, char *name, char *source )
{
    /* Create shader and compile it */
    GLuint shader = glCreateShader( type );
    glShaderSource( shader, 1, &source, NULL );
    glCompileShader( shader );

    /* Report error and return zero if compile failed */
    GLint compiled;
    glGetShaderiv( shader, GL_COMPILE_STATUS, &compiled );
    if( !compiled )
    {
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

GLuint shaderLoadAndCompile( ShaderSource *s )
{
    GLuint shader = 0;
    GLchar *text = loadText( s->fname );
    if( text )
    {
        shader = shaderCompile( s->type, s->fname, text );
        free( text );
    }
    return shader;
}

GLuint shaderLink( int shaders, GLuint shader[] )
{
    int i;
    GLuint program = glCreateProgram();

    for( i=0; i<shaders; i++ )
    {
        glAttachShader( program, shader[i] );
        glDeleteShader( shader[i] );  // NOTE only flags shader for deletion when unreferenced
    }

    glLinkProgram( program );

    /* Report error and return zero if link failed */
    GLint linked;
    glGetProgramiv( program, GL_LINK_STATUS, &linked );
    if( !linked )
    {
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

GLuint shaderProgram( int shaders, ShaderSource src[] )
{
    int i;
    GLuint shader[shaders];

    for( i=0; i<shaders; i++ )
        shader[i] = shaderLoadAndCompile( &src[i] );

    return shaderLink( shaders, shader );
}


/////////////////////////////////////////////////////////////////////////////////////////////
// GL Buffer Object
/////////////////////////////////////////////////////////////////////////////////////////////

GLuint newBufObj( GLenum target, GLsizei size, const void *data, GLenum kind )
{
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


VFormat vtxNewFormat( GLuint prog, int count, VDesc desc[] )
{
    VFormat fmt;

    int i;
    int stride = 0;
    for( i=0; i<count; i++ )
    {
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
void vtxEnable( VFormat *fmt )
{
    int i;
    VAttrib *a = fmt->attr;
    for( i=0; i<fmt->attribs; i++ )
    {
        glEnableVertexAttribArray( a[i].idx );
        glVertexAttribPointer( a[i].idx, a[i].elems, GL_FLOAT, GL_FALSE, fmt->stride, a[i].offs );
    }
}

void vtxDisable( VFormat *fmt )
{
    int i;
    VAttrib *a = fmt->attr;
    for( i=0; i<fmt->attribs; i++ )
        glDisableVertexAttribArray( a[i].idx );
}


/////////////////////////////////////////////////////////////////////////////////////////////
// Textures, Framebuffers, and Renderbuffers
/////////////////////////////////////////////////////////////////////////////////////////////

static GLuint texNew( GLint filter, GLint wrapS, GLint wrapT )
{
    GLuint idx;
    glGenTextures( 1, &idx );
    glBindTexture( GL_TEXTURE_2D, idx );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter );
    return idx;
}

GLuint texCreate( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data )
{
    GLuint idx = texNew( filter, wrapS, wrapT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, data );
    glBindTexture( GL_TEXTURE_2D, 0 );
    return idx;
}

GLuint texCreateMip( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT, const GLvoid *data )
{
    GLuint idx = texNew( filter, wrapS, wrapT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, data );
    glGenerateMipmap( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, 0 );
    return idx;
}

// exactly like texLoad, but no data... useful for generating a render target
GLuint texCreateTarget( GLint intern, GLenum form, GLenum typ, GLsizei w, GLsizei h, GLint filter, GLint wrapS, GLint wrapT )
{
    GLuint idx = texNew( filter, wrapS, wrapT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter );
    glTexImage2D( GL_TEXTURE_2D, 0, intern, w, h, 0, form, typ, 0 );
    glBindTexture( GL_TEXTURE_2D, 0 );
    return idx;
}

//delete: glDeleteTextures( n, array );

//(* Binds texture unit 'n' to use 'tex_id' as source, and feeding
// * the current shader via 'uniform_location'. *)
void texBind( GLint n, GLuint uniformLoc, GLuint tex )
{
    glActiveTexture( GL_TEXTURE0+n );
    glUniform1i( uniformLoc, n );
    glBindTexture( GL_TEXTURE_2D, tex );
}

void texUnbind( GLint n )
{
    glActiveTexture( GL_TEXTURE0+n );
    glBindTexture( GL_TEXTURE_2D, 0 );
}


// RenderBuffer
GLuint renderbufferNew( GLenum component, GLsizei w, GLsizei h )
{
    GLuint rb;
    glGenRenderbuffers( 1, &rb );
    glBindRenderbuffer( GL_RENDERBUFFER, rb );
    glRenderbufferStorage( GL_RENDERBUFFER, component, w, h );
    glBindRenderbuffer( GL_RENDERBUFFER, 0 );
    return rb;
}

//delete: glDeleteRenderbuffers( n, array );

// FrameBuffer Object
GLuint fboNew( GLuint texture )
{
    GLuint fbo;
    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0 );
    return fbo;
}

// returns true if fbo is good
int fboBind( GLuint fbo )
{
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

void fboUnbind( void )
{
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

GLuint fboCreate( GLuint texture )
{
    GLuint fbo = fboNew( texture );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    return fbo;
}

GLuint fboCreateWithDepth( GLuint texture, GLuint depth )
{
    GLuint fbo = fboNew( texture );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    return fbo;
}

//delete: glDeleteFramebuffers( n, array );



/////////////////////////////////////////////////////////////////////////////////////////////
// View (viewport) and offscreen buffer
/////////////////////////////////////////////////////////////////////////////////////////////

void viewSetFrameBuffer( View *v, GLsizei w, GLsizei h )
{
    v->x0  = 0;
    v->y0  = 0;
    v->wid = w;
    v->hgt = h;
    v->offscreen = 0;
    v->rgba  = 0;
    v->depth = 0;
    v->fbo   = 0;
}

void viewSetOffscreenBuffer( View *v, GLint filter, GLsizei w, GLsizei h )
{
    v->x0  = 0;
    v->y0  = 0;
    v->wid = w;
    v->hgt = h;
    v->offscreen = 1;
    v->rgba  = texCreateTarget( TEX_BYTE4, w, h, filter, TEX_CLAMP );
    v->depth = renderbufferNew( GL_DEPTH_COMPONENT, w, h );
    v->fbo   = fboCreateWithDepth( v->rgba, v->depth );
}

void viewFreeOffscreenBuffer( View *v )
{
    glDeleteTextures( 1, &v->rgba );
    glDeleteRenderbuffers( 1, &v->depth );
    glDeleteFramebuffers( 1, &v->fbo );
}



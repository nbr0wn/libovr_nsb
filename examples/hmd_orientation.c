#include <GL/glut.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <sys/select.h>
#include <libovr_nsb/OVR.h>

void idleFunc( );
void displayFunc( );
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );
void initialize( );
void material( );
void lights( );

// width and height of the window
GLsizei g_width = 1280;
GLsizei g_height = 800;

// fill mode
GLenum g_fillmode = GL_FILL;
GLfloat eye[3] = { 10, 10, 20};
GLfloat ovrLook[3] = { 0, 0, 0};

// Oculus Rift Device
Device *dev;

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void material( )
{
  static float front_mat_shininess[] =
  {90.0};
  static float front_mat_specular[] =
  {0.2, 0.2, 0.2, 1.0};
  static float front_mat_diffuse[] =
  {0.5, 0.5, 0.28, 1.0};

  static float back_mat_shininess[] =
  {90.0};
  static float back_mat_specular[] =
  {0.5, 0.5, 0.2, 1.0};
  static float back_mat_diffuse[] =
  {1.0, 0.9, 0.2, 1.0};

  glMaterialfv(GL_FRONT, GL_SHININESS, front_mat_shininess);
  glMaterialfv(GL_FRONT, GL_SPECULAR, front_mat_specular);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
  glMaterialfv(GL_BACK, GL_SHININESS, back_mat_shininess);
  glMaterialfv(GL_BACK, GL_SPECULAR, back_mat_specular);
  glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void lights(void)
{
  static float ambient[] =
  {0.1, 0.1, 0.1, 1.0};
  static float diffuse[] =
  {0.5, 1.0, 1.0, 1.0};
  static float position[] =
  {90.0, 90.0, 150.0, 0.0};

  static float lmodel_ambient[] =
  {1.0, 1.0, 1.0, 1.0};

  static float lmodel_twoside[] =
  {GL_TRUE};
  static float lmodel_oneside[] =
  {GL_FALSE};

  glDepthFunc(GL_LEQUAL);

  glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
  glLightfv(GL_LIGHT0, GL_POSITION, position);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
}


/////////////////////////////////////////////////////////////////////////////////////////////
// GL Init
/////////////////////////////////////////////////////////////////////////////////////////////
void initialize()
{
    glClearColor( 0.0f, 0.0f,0.0f, 1.0f );
    glShadeModel( GL_SMOOTH );
    glEnable( GL_DEPTH_TEST );
    glFrontFace( GL_CCW );
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    material();
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    lights();
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Handle window resizes
/////////////////////////////////////////////////////////////////////////////////////////////
void reshapeFunc( GLsizei w, GLsizei h )
{
    g_width = w; g_height = h;
    glViewport( 0, 0, w, h );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    gluLookAt( eye[0], eye[1], eye[2],
               0.0f, 0.0f, 0.0f, 
               0.0, 1.0, 0.0 );
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'f':
            g_fillmode = ( g_fillmode == GL_FILL ? GL_LINE : GL_FILL );
            glPolygonMode( GL_FRONT_AND_BACK, g_fillmode );
            break;
        case 27:  // Why the hell doesn't glut have GLUT_KEY_ESC?
        case 'q':
            exit( 0 );
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

    reshapeFunc( g_width, g_height );
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

/////////////////////////////////////////////////////////////////////////////////////////////
// Cheap axis rendering
/////////////////////////////////////////////////////////////////////////////////////////////
void draw_axes( void )
{
    glDisable(GL_LIGHTING);
	// Draw the axes
	glLineWidth( 1.0 );
	glColor3f( 0.25, 0.0, 0.0 );
	glBegin( GL_LINES );
		glVertex3i( -5000, 0, 0 );
		glVertex3i( 0, 0, 0 );
	glEnd( );

	glLineWidth( 3.0 );
	glColor3f( 1.0, 0.0, 0.0 );
	glBegin( GL_LINES );
		glVertex3i( 0, 0, 0 );
		glVertex3i( 5000, 0, 0 );
	glEnd( );

	glLineWidth( 1.0 );
	glColor3f( 0.0, 0.25, 0.0 );
	glBegin( GL_LINES );
		glVertex3i( 0, -5000, 0 );
		glVertex3i( 0, 0, 0 );
	glEnd( );

	glLineWidth( 3.0 );
	glColor3f( 0.0, 1.0, 0.0 );
	glBegin( GL_LINES );
		glVertex3i( 0, 0, 0 );
		glVertex3i( 0, 5000, 0 );
	glEnd( );

	glLineWidth( 1.0 );
	glColor3f( 0.0, 0.0, 0.25 );
	glBegin( GL_LINES );
		glVertex3i( 0, 0, -5000 );
		glVertex3i( 0, 0, 0 );
	glEnd( );

	glLineWidth( 3.0 );
	glColor3f( 0.0, 0.0, 1.0 );
	glBegin( GL_LINES );
		glVertex3i( 0, 0, 0 );
		glVertex3i( 0, 0, 5000 );
	glEnd( );
	glLineWidth( 1.0 );
    glEnable(GL_LIGHTING);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Glut arrow
/////////////////////////////////////////////////////////////////////////////////////////////
void drawArrow( )
{
    glPushMatrix( );
        glRotatef(-90, 1, 0, 0 );
        glutSolidCylinder(0.25, 5.0, 20, 20);
        glTranslatef(0,0,5);
        glutSolidCone(0.5,2,20,20);
    glPopMatrix( );
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Cheap simulationr rendering
/////////////////////////////////////////////////////////////////////////////////////////////
void displayFunc( )
{
    int i;
    char buf[100];

    double m4[16];
    double rot[16];

    // clear the color and depth buffers
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glPushMatrix();
        quat_toMat4(dev->Q, m4);
        mat4_toRotationMat(m4,rot);

        glMultMatrixd((GLdouble *)rot);
        glColor3f(0.5,0,0);
        drawArrow();
    glPopMatrix();

    draw_axes();

    // Draw some Sensor info
    glPushMatrix();
        glColor3f(0.0, 1.0, 0.0);
        glRasterPos2d(3,2);
        sprintf(buf,"A: %+f %+f %+f %+f", dev->A[0], dev->A[1], dev->A[2], dev->A[3] );
        glutBitmapString(GLUT_BITMAP_HELVETICA_18,buf);

        glColor3f(1.0, 0.0, 0.0);
        glRasterPos2d(3,3);
        sprintf(buf,"Q: %+f %+f %+f %+f", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] );
        glutBitmapString(GLUT_BITMAP_HELVETICA_18,buf);

        glColor3f(0.8, 0.3, 0.5);
        for( i=0; i < 4; i++)
        {
            glRasterPos2d(3,4+i*0.5);
            sprintf(buf,"rot: %+f %+f %+f %+f", 
                    rot[i*4+0],
                    rot[i*4+1],
                    rot[i*4+2],
                    rot[i*4+3] );
            glutBitmapString(GLUT_BITMAP_HELVETICA_18,buf);
        }
    glPopMatrix();

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

    printf("\n\nf - toggle wireframe\n");
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


//-----------------------------------------------------------------------------
// Name: main( )
// Desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    dev = (Device *)malloc(sizeof(Device));
    // Fire up Sensor update thread
    runSensorUpdateThread(dev);

    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowSize( g_width, g_height );
    glutInitWindowPosition( 200, 200 );
    glutCreateWindow( argv[0] );

    // Set up our callbacks
    glutIdleFunc( idleFunc );
    glutDisplayFunc( displayFunc );
    glutReshapeFunc( reshapeFunc );
    glutKeyboardFunc( keyboardFunc );
    glutMouseFunc( mouseFunc );

    // Local init
    initialize();

    // Go Glut
    glutMainLoop();

    return 0;
}


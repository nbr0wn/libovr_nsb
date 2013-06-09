#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "OVR.h"

/////////////////////////////////////////////////////////////////////////////////////
// Continuous sample/update thread code
// select() chosen for portability
/////////////////////////////////////////////////////////////////////////////////////
void *threadRun( void *data )
{
    Device *dev = (Device *)data;
<<<<<<< HEAD
=======
    fd_set readset;
    struct timeval waitTime;

    // 500ms
    waitTime.tv_sec = 0;
    waitTime.tv_usec = 500000;

    FD_ZERO(&readset);
    FD_SET(dev->fd,&readset);
>>>>>>> 407ee91efdfafab106757040c9bfa0a25fe2ecd3

    sendSensorKeepAlive(dev);

    while( dev->runSampleThread )
    {
<<<<<<< HEAD
        waitSampleDevice(dev,500);
=======
        waitTime.tv_sec = 0;
        waitTime.tv_usec = 500000;
        int result = select(dev->fd + 1, &readset, NULL, NULL, &waitTime );

        if ( result && FD_ISSET( dev->fd, &readset ) )
        {
            sampleDevice(dev);
        }
>>>>>>> 407ee91efdfafab106757040c9bfa0a25fe2ecd3
        // Send a keepalive - this is too often.  Need to only send on keepalive interval
        sendSensorKeepAlive(dev);
    }
    return 0;
}

//-----------------------------------------------------------------------------
// Name: main( )
// Desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    Device *dev = openRift(0,0);

    if( !dev )
    {
        printf("Could not locate Rift\n");
        printf("Be sure you have read/write permission to the proper /dev/hidrawX device\n");
        return -1;
    }

    printf("Device Info:\n");
<<<<<<< HEAD
    printf("\tmanufacturer: %ls\n", dev->manufacturer);
    printf("\tproduct:      %ls\n", dev->product);
    printf("\tserial:       %ls\n", dev->serial);
    printf("\tvendor:       0x%04hx\n", dev->vendorId);
    printf("\tproduct:      0x%04hx\n", dev->productId);
=======
    printf("\tname:     %s\n", dev->name);
    printf("\tlocation: %s\n", dev->location);
    printf("\tvendor:   0x%04hx\n", dev->vendorId);
    printf("\tproduct:  0x%04hx\n", dev->productId);
>>>>>>> 407ee91efdfafab106757040c9bfa0a25fe2ecd3

    printf("CTRL-C to quit\n\n");

    // Run a thread
    pthread_t f1_thread; 
    dev->runSampleThread = TRUE;
    pthread_create(&f1_thread,NULL,threadRun,dev);
    for(;;)
    {
        printf("\tQ:%+-10g %+-10g %+-10g %+-10g\n", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] ); 
        usleep(100000);
    }
    return 0;
}

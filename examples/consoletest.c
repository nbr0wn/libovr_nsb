#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "OVR.h"

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
    printf("\tname:     %s\n", dev->name);
    printf("\tlocation: %s\n", dev->location);
    printf("\tvendor:   0x%04hx\n", dev->vendorId);
    printf("\tproduct:  0x%04hx\n", dev->productId);

    printf("CTRL-C to quit\n\n");

    sendSensorKeepAlive(dev);

    fd_set readset;
    struct timeval waitTime;

    // 500ms
    waitTime.tv_sec = 0;
    waitTime.tv_usec = 500000;

    FD_ZERO(&readset);
    FD_SET(dev->fd,&readset);

    for(;;)
    {
        waitTime.tv_sec = 0;
        waitTime.tv_usec = 500000;

        // Wait for the device to have some data available
        int result = select(dev->fd + 1, &readset, NULL, NULL, &waitTime );

        if ( result && FD_ISSET( dev->fd, &readset ) )
        {
            sampleDevice(dev);
        }
        // Send a keepalive - this is too often.  Need to only send on keepalive interval
        sendSensorKeepAlive(dev);

        printf("\tQ:%+-10g %+-10g %+-10g %+-10g\n", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] ); 
    }

    return 0;
}

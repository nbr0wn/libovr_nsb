#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <sys/epoll.h>

#include <libovr_nsb/OVR.h>

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
    printf("\tName:      %s\n", dev->name);
    printf("\tProduct:   %s\n", dev->product);
    printf("\tVendorID:  0x%04hx\n", dev->vendorId);
    printf("\tProductID: 0x%04hx\n", dev->productId);

    printf("CTRL-C to quit\n\n");

    sendSensorKeepAlive(dev);

    for(;;)
    {
        // Try to sample the device for 1ms
        waitSampleDevice(dev, 1000);

        // Send a keepalive - this is too often.  Need to only send on keepalive interval
        sendSensorKeepAlive(dev);

        printf("\tQ:%+-10g %+-10g %+-10g %+-10g\n", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] ); 
    }

    return 0;
}

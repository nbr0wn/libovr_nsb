#if !defined(_OVR_H_)
#define _OVR_H_

#include <libovr_nsb/OVR_Sensor.h>

// Open the nthDevice Rift attached to the system, in the order they
// appear in /dev's dirent.
//
// Return: Initialized device struct
//         NULL on failure
Device * openRift( int nthDevice, Device *myDev );

// Attempt to process one device sample
// Should be called as frequently as possible
//
// Return: TRUE if a sample was processed
BOOLEAN sampleDevice( Device *dev );

// Wait for a period of time for a device sample and then process it
// Should be called as frequently as possible
//
// Return: TRUE if a sample was processed
BOOLEAN waitSampleDevice(Device *dev, UInt16 waitMsec);

// Send a keepalive to the device.  Do this at least every 
// 5 seconds
//
// Return: TRUE if keepalive was successful
BOOLEAN sendSensorKeepAlive(Device *dev);

#endif

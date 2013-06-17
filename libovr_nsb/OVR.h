#if !defined(_OVR_H_)
#define _OVR_H_

#include "OVR_Sensor.h"

// Open the nthDevice Rift attached to the system, in the order they
// appear in /dev's dirent.
Device * openRift( int nthDevice, Device *mydev );

#endif

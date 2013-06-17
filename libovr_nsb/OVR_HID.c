#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>

#include "OVR_HID.h"
static BOOLEAN getDeviceInfo( Device *dev );
static BOOLEAN isRift( const char *path );
static BOOLEAN openDevice(Device *dev, const char *path);

/////////////////////////////////////////////////////////////////////////////////////////////
// Scan /dev looking for hidraw devices and then check to see if each is a Rift
// nthDevice is 0-based
/////////////////////////////////////////////////////////////////////////////////////////////
Device * openRiftHID( int nthDevice, Device *myDev )
{
    struct dirent *d;
    DIR *dir;
    char fileName[32];
    Device *dev = 0;

    // Open /dev directory
    dir = opendir("/dev");

    // Iterate over /dev files
    while( (d = readdir(dir)) != 0)
    {
        // Is this a hidraw device?
        if( strstr(d->d_name, "hidraw") )
        {
            sprintf(fileName, "/dev/%s", d->d_name);
            if( isRift( fileName ) )
            {
                // Skip to the nth Rift
                if ( ! nthDevice )
                {
                    // Use passed in space if we have it
                    if( myDev )
                    {
                        dev = myDev;
                    }
                    else
                    {
                        dev = (Device *)malloc(sizeof(Device));
                    }
                    openDevice(dev,fileName);
                    getDeviceInfo(dev);
                    break;
                }
                nthDevice--;
            }
        }
    }
    closedir(dir);
    return dev;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void closeRiftHID( Device *myDev )
{
    // TODO - clean up device
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Open the device and check the vendor and product codes to see if it's a Rift
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN isRift( const char *path )
{
    int fd;
	int res;
	struct hidraw_devinfo info;

    // Open the device
	fd = open(path, O_RDWR);
	if (fd < 0) 
    {
		perror("Unable to open device");
		return FALSE;
	}
	// Get USB info
	res = ioctl(fd, HIDIOCGRAWINFO, &info);
    close(fd);
	if (res < 0) 
    {
		perror("HIDIOCGRAWINFO");
        return FALSE;
	} 
    else 
    {
        // Check to see if the vendor and product match
        if( info.vendor == OVR_VENDOR && info.product == OVR_PRODUCT )
        {
            return TRUE;
        }
	}
    return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Just a quick open
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN openDevice( Device *dev, const char *path )
{
	dev->fd = open(path, O_RDWR|O_NONBLOCK);

	if (dev->fd < 0) 
    {
		return FALSE;
	}
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN getDeviceInfo( Device *dev )
{
	int res;
	char buf[256];
	struct hidraw_devinfo info;

	memset(&info, 0x0, sizeof(info));
	memset(buf, 0x0, sizeof(buf));

	// Name
	res = ioctl(dev->fd, HIDIOCGRAWNAME(256), buf);
	if (res < 0)
    {
		perror("HIDIOCGRAWNAME");
        return FALSE;
    }
    else
    {
        dev->name = (char *)malloc(strlen(buf)+1);
        strcpy(dev->name, buf);
    }

	// /sys location
	res = ioctl(dev->fd, HIDIOCGRAWPHYS(256), buf);
	if (res < 0)
    {
		perror("HIDIOCGRAWPHYS");
        return FALSE;
    }
    else
    {
        dev->location = (char *)malloc(strlen(buf)+1);
        strcpy(dev->location, buf);
    }

	// USB info
	res = ioctl(dev->fd, HIDIOCGRAWINFO, &info);
	if (res < 0) 
    {
		perror("HIDIOCGRAWINFO");
        return FALSE;
	} 
    else 
    {
        dev->vendorId = info.vendor;
        dev->productId = info.product;
	}

    // Also get the sensor info
    return getSensorInfo(dev);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Scale Range
// HID Type: Set Feature
// HID Packet Length: 7 - TODO really 8?  Check
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorScaleRange( Device *dev, const struct SensorScaleRange *r)
{
    UInt8 Buffer[8];
    int res;
    UInt16 CommandId = 0;

    Buffer[0] = 4;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = r->AccelScale;
    Buffer[4] = r->GyroScale & 0xFF;
    Buffer[5] = r->GyroScale >> 8;
    Buffer[6] = r->MagScale & 0xFF;
    Buffer[7] = r->MagScale >> 8;

    res = ioctl(dev->fd, HIDIOCSFEATURE(7), Buffer);
    if (res < 0)
    {
        perror("sendSensorScaleRange");
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Config
// HID Type: Set Feature
// HID Packet Length: 7
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorConfig(Device *dev, UInt8 flags, UInt8 packetInterval, UInt16 keepAliveIntervalMs)
{
    UInt8 Buffer[7];
    int res;
    UInt16 CommandId = 0;

    Buffer[0] = 2;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = flags;
    Buffer[4] = packetInterval;
    Buffer[5] = keepAliveIntervalMs & 0xFF;
    Buffer[6] = keepAliveIntervalMs >> 8;

    res = ioctl(dev->fd, HIDIOCSFEATURE(7), Buffer);
    if (res < 0)
    {
        perror("sendSensorConfig");
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor KeepAlive
// HID Type: Set Feature
// HID Packet Length: 5
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorKeepAlive(Device *dev)
{
    UInt8 Buffer[5];
    int res;
    UInt16 CommandId = 0;

    Buffer[0] = 8;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = dev->keepAliveIntervalMs & 0xFF;
    Buffer[4] = dev->keepAliveIntervalMs >> 8;

    res = ioctl(dev->fd, HIDIOCSFEATURE(5), Buffer);
    if (res < 0)
    {
        perror("sendSensorKeepAlive");
        return FALSE;
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Display Info
// HID Type: Get Feature
// HID Packet Length: 56
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN getSensorInfo( Device *dev )
{
    UInt8 Buffer[56];
    //UInt16 CommandId;
    int res;

    memset(Buffer,0,56);

    Buffer[0] = 0x9;  // DisplayInfo

    res = ioctl(dev->fd, HIDIOCGFEATURE(56), Buffer);
    if (res < 0) 
    {
        perror("getSensorInfo");
        return FALSE;
    } 
    else 
    {
        int i;
        //CommandId                               = Buffer[1] | (Buffer[2] << 8);
        dev->sensorInfo.DistortionType          = Buffer[3];
        dev->sensorInfo.HResolution             = DecodeUInt16(Buffer+4);
        dev->sensorInfo.VResolution             = DecodeUInt16(Buffer+6);
        dev->sensorInfo.HScreenSize             = DecodeUInt32(Buffer+8) *  (1/1000000.f);
        dev->sensorInfo.VScreenSize             = DecodeUInt32(Buffer+12) * (1/1000000.f);
        dev->sensorInfo.VCenter                 = DecodeUInt32(Buffer+16) * (1/1000000.f);
        dev->sensorInfo.LensSeparation          = DecodeUInt32(Buffer+20) * (1/1000000.f);
        dev->sensorInfo.EyeToScreenDistance[0]  = DecodeUInt32(Buffer+24) * (1/1000000.f);
        dev->sensorInfo.EyeToScreenDistance[1]  = DecodeUInt32(Buffer+28) * (1/1000000.f);
        dev->sensorInfo.DistortionK[0]          = DecodeFloat(Buffer+32);
        dev->sensorInfo.DistortionK[1]          = DecodeFloat(Buffer+36);
        dev->sensorInfo.DistortionK[2]          = DecodeFloat(Buffer+40);
        dev->sensorInfo.DistortionK[3]          = DecodeFloat(Buffer+44);
        dev->sensorInfo.DistortionK[4]          = DecodeFloat(Buffer+48);
        dev->sensorInfo.DistortionK[5]          = DecodeFloat(Buffer+52);

        printf ("\nSensor Info:\n");
		for (i = 0; i < res; i++)
			printf("%hhx ", Buffer[i]);
		puts("\n");

        printf ("\nR: %d x %d", dev->sensorInfo.HResolution, dev->sensorInfo.VResolution );
        printf ("\tS: %f x %f\n", dev->sensorInfo.HScreenSize, dev->sensorInfo.VScreenSize );
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
// single-sample with ms wait
/////////////////////////////////////////////////////////////////////////////////////
int waitForSample(Device *dev, UInt16 msec, UInt8 *buf, UInt16 maxLen )
{
    fd_set readset;
    struct timeval waitTime;

    FD_ZERO(&readset);
    FD_SET(dev->fd,&readset);

    waitTime.tv_sec = 0;
    waitTime.tv_usec = msec * 1000;
    int result = select(dev->fd + 1, &readset, NULL, NULL, &waitTime );

    if ( result && FD_ISSET( dev->fd, &readset ) )
    {
        return readSample(dev, buf, maxLen);
    }
    return -1;
}

/////////////////////////////////////////////////////////////////////////////////////
// Wait some period of time for data and sample it if some arrives
/////////////////////////////////////////////////////////////////////////////////////
int readSample(Device *dev, UInt8 *buf, UInt16 maxLen)
{
    // Just read from the 
    return read(dev->fd, buf, maxLen);
}

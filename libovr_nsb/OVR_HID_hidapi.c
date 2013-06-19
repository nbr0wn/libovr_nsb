#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>

#include <libovr_nsb/OVR_HID.h>
#include <libovr_nsb/OVR_Sensor.h>

#define MAX_STR 255

/////////////////////////////////////////////////////////////////////////////////////////////
// Scan /dev looking for hidraw devices and then check to see if each is a Rift
// nthDevice is 0-based
/////////////////////////////////////////////////////////////////////////////////////////////
Device * openRiftHID( int nthDevice, Device *myDev )
{
    BOOLEAN didAlloc = FALSE;
	struct hid_device_info *devs, *cur_dev;
	wchar_t wstr[MAX_STR];
    Device *dev = myDev;
	
    // Iterate over the devices matching our IDs
	devs = hid_enumerate(0x2833, 0x0001);
	cur_dev = devs;	
	while (cur_dev) {
        // Skip to the one we want
        if( nthDevice-- )
        {
            cur_dev = cur_dev->next;
            continue;
        }

        // Allocate space if we need to
        if (! dev )
        {
            dev = (Device *)malloc(sizeof(Device));
            didAlloc = TRUE;
        }

        // Open the device
        dev->hidapi_dev = (hid_device *)hid_open(cur_dev->vendor_id, 
                cur_dev->product_id, cur_dev->serial_number);

        // Did we fail to open the device?
        if( !dev->hidapi_dev )
        {
            // Yep. Clean up
            if( didAlloc )
            {
                free(dev);
            }
            return 0;
        }

        // Save the vendor and product IDs (not that we need them)
        dev->vendorId = cur_dev->vendor_id;
        dev->productId = cur_dev->product_id;

        //printf("\n\nDevice Found:\n");

        // Read the Manufacturer String
        hid_get_manufacturer_string(dev->hidapi_dev, wstr, MAX_STR);
        //printf("Manufacturer:  %ls\n", wstr);
        dev->name = malloc(sizeof(wchar_t) * wcslen(wstr) + 1);
        wcstombs(dev->name, wstr, wcslen(wstr));

        // Read the Product String
        hid_get_product_string(dev->hidapi_dev, wstr, MAX_STR);
        //printf("Product:       %ls\n", wstr);
        dev->product = malloc(sizeof(wchar_t) * wcslen(wstr) + 1);
        wcstombs(dev->product, wstr, wcslen(wstr));

        // Read the Serial Number String
        hid_get_serial_number_string(dev->hidapi_dev, wstr, MAX_STR);
        //printf("Serial Number: %ls", wstr);
        dev->serial = malloc(sizeof(wchar_t) * wcslen(wstr) + 1);
        wcstombs(dev->serial, wstr, wcslen(wstr));

        //printf("\n\n");
        break;
	}
	hid_free_enumeration(devs);

    if ( dev && dev->hidapi_dev > 0 )
    {
        // Make our device nonblocking
        hid_set_nonblocking(dev->hidapi_dev, 1);

        // Init the Rift sensor info
        if( ! getSensorInfo( dev ) )
        {
            // Clean up
            if( didAlloc )
            {
                free(dev);
            }
            dev = 0;
        }
    }
    else
    {
        // Clean up
        if ( didAlloc )
        {
            free(dev);
        }
        dev = 0;
    }

    return dev;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void closeRiftHID( Device *myDev )
{
    // TODO - clean up device
    hid_exit();
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Scale Range
// HID Type: Set Feature
// HID Packet Length: 7 -- TODO Really 8? Check
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorScaleRange( Device *dev, const struct SensorScaleRange *r)
{
    UInt8 Buffer[8];
    UInt16 CommandId = 0;

    Buffer[0] = 4;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = r->AccelScale;
    Buffer[4] = r->GyroScale & 0xFF;
    Buffer[5] = r->GyroScale >> 8;
    Buffer[6] = r->MagScale & 0xFF;
    Buffer[7] = r->MagScale >> 8;

    return hid_send_feature_report(dev->hidapi_dev, Buffer, 8 ) == 8;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Config
// HID Type: Set Feature
// HID Packet Length: 7
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorConfig(Device *dev, UInt8 flags, UInt8 packetInterval, UInt16 keepAliveIntervalMs)
{
    UInt8 Buffer[7];
    UInt16 CommandId = 0;

    Buffer[0] = 2;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = flags;
    Buffer[4] = packetInterval;
    Buffer[5] = keepAliveIntervalMs & 0xFF;
    Buffer[6] = keepAliveIntervalMs >> 8;

    return hid_send_feature_report(dev->hidapi_dev, Buffer, 8 ) == 7;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor KeepAlive
// HID Type: Set Feature
// HID Packet Length: 5
/////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sendSensorKeepAlive(Device *dev)
{
    UInt8 Buffer[5];
    UInt16 CommandId = 0;

    Buffer[0] = 8;
    Buffer[1] = CommandId & 0xFF;
    Buffer[2] = CommandId >> 8;
    Buffer[3] = dev->keepAliveIntervalMs & 0xFF;
    Buffer[4] = dev->keepAliveIntervalMs >> 8;

    return hid_send_feature_report(dev->hidapi_dev, Buffer, 8 ) == 5;
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

    Buffer[0] = 0x9;  // DisplayInfo
    res = hid_get_feature_report(dev->hidapi_dev, Buffer, 56 );
    if (res < 0) 
    {
        return FALSE;
    } 
    else 
    {
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

#if 0
        {
        int i;
        printf ("\nSensor Info:\n");
		for (i = 0; i < res; i++)
			printf("%hhx ", Buffer[i]);
		puts("\n");

        printf ("\nR: %d x %d", dev->sensorInfo.HResolution, dev->sensorInfo.VResolution );
        printf ("\tS: %f x %f\n", dev->sensorInfo.HScreenSize, dev->sensorInfo.VScreenSize );
        }
#endif
    }
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////////////
// single-sample with ms wait
/////////////////////////////////////////////////////////////////////////////////////
int waitForSample(Device *dev, UInt16 msec, UInt8 *buf, UInt16 maxLen )
{
    return hid_read_timeout(dev->hidapi_dev, buf, maxLen, msec );
}

/////////////////////////////////////////////////////////////////////////////////////
// Wait some period of time for data and sample it if some arrives
/////////////////////////////////////////////////////////////////////////////////////
int readSample(Device *dev, UInt8 *buf, UInt16 maxLen)
{
    return hid_read(dev->hidapi_dev, buf, maxLen);
}

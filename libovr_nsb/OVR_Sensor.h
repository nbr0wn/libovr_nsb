#if !defined(_OVR_SENSOR_H)
#define _OVR_SENSOR_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include <gl_matrix/gl_matrix.h>

#include <libovr_nsb/OVR_Defs.h>
#include <libovr_nsb/OVR_Device.h>

//////////////////////////////////////////////////////////////////////////////////////////////
// Message Structs
//////////////////////////////////////////////////////////////////////////////////////////////

// Flag values for SensorConfig Flags.
enum {
    Flag_RawMode            = 0x01,
    Flag_CallibrationTest   = 0x02, // Internal test mode
    Flag_UseCallibration    = 0x04,
    Flag_AutoCallibration   = 0x08,
    Flag_MotionKeepAlive    = 0x10,
    Flag_CommandKeepAlive   = 0x20,
    Flag_SensorCoordinates  = 0x40
};

// Sensor configuration command, ReportId == 2.
struct SensorConfig
{
    UByte   Flags;
    UInt16  PacketInterval;
    UInt16  KeepAliveIntervalMs;
};


// SensorKeepAlive - feature report that needs to be sent at regular intervals for sensor
// to receive commands.
struct SensorKeepAlive
{
    UInt16  KeepAliveIntervalMs;
};

// DisplayInfo obtained from sensor; these values are used to report distortion
// settings and other coefficients.
// Older SensorDisplayInfo will have all zeros, causing the library to apply hard-coded defaults.
// Currently, only resolutions and sizes are used.

enum
{
    Mask_BaseFmt    = 0x0f,
    Mask_OptionFmts = 0xf0,
    Base_None       = 0,
    Base_ScreenOnly = 1,
    Base_Distortion = 2,
};


// Messages we care about
typedef enum
{
    TrackerMessage_None              = 0,
    TrackerMessage_Sensors           = 1,
    TrackerMessage_Unknown           = 0x100,
    TrackerMessage_SizeError         = 0x101,
} TrackerMessageType;

typedef struct
{
    SInt32 AccelX, AccelY, AccelZ;
    SInt32 GyroX, GyroY, GyroZ;
} TrackerSample;

typedef struct
{
    UByte	SampleCount;
    UInt16	Timestamp;
    UInt16	LastCommandID;
    SInt16	Temperature;

    TrackerSample Samples[3];

    SInt16	MagX, MagY, MagZ;
} TrackerSensors;

typedef struct 
{
    double Acceleration[3];   // Acceleration in m/s^2.
    double RotationRate[3];   // Angular velocity in rad/s^2.
    double MagneticField[3];  // Magnetic field strength in Gauss.
    float    Temperature;    // Temperature reading on sensor surface, in degrees Celsius.
    float    TimeDelta;      // Time passed since last Body Frame, in seconds.
} MessageBodyFrame;

//#pragma pack(pop)


// Functions
void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z);
TrackerMessageType DecodeTracker(const UByte* buffer, TrackerSensors *sensorMsg, int size);
UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count, float val, float factor, const char* label);
void SetSensorRange(struct SensorScaleRange *s, const SensorRange *r );
void GetSensorRange(SensorRange* r, struct SensorScaleRange *s);
void initDevice(Device *dev);
void setKeepAliveInterval(Device *dev, UInt16 interval);
void processTrackerData(Device *dev, TrackerSensors *s);
void updateOrientation(Device *dev, MessageBodyFrame *msg);
void GetAngVFilterVal(Device *dev, vec3_t out);
void ResetAngVFilter(Device *dev );
BOOLEAN processSample(Device *dev, UInt8 *buf, UInt16 len );

#endif

#if !defined(_OVR_DEVICE_H)
#define _OVR_DEVICE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include <hidapi/hidapi.h>
#include <gl_matrix/gl_matrix.h>

#include <libovr_nsb/OVR_Defs.h>



//////////////////////////////////////////////////////////////////////////////////////////////
// Range
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    // Maximum detected acceleration in m/s^2. Up to 8*G equivalent support guaranteed,
    // where G is ~9.81 m/s^2.
    // Oculus DK1 HW has thresholds near: 2, 4 (default), 8, 16 G.
    float   MaxAcceleration;  
    // Maximum detected angular velocity in rad/s. Up to 8*Pi support guaranteed.
    // Oculus DK1 HW thresholds near: 1, 2, 4, 8 Pi (default).
    float   MaxRotationRate;
    // Maximum detectable Magnetic field strength in Gauss. Up to 2.5 Gauss support guaranteed.
    // Oculus DK1 HW thresholds near: 0.88, 1.3, 1.9, 2.5 gauss.
    float   MaxMagneticField;
} SensorRange;

//////////////////////////////////////////////////////////////////////////////////////////////
// Scale Range
// SensorScaleRange provides buffer packing logic for the Sensor Range
// record that can be applied to DK1 sensor through Get/SetFeature. We expose this
// through SensorRange class, which has different units.
//////////////////////////////////////////////////////////////////////////////////////////////
struct SensorScaleRange
{
    UInt16  AccelScale;
    UInt16  GyroScale;
    UInt16  MagScale;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// Coordinate Reference
//////////////////////////////////////////////////////////////////////////////////////////////
typedef enum 
{
    Coord_Sensor = 0,
    Coord_HMD    = 1
} CoordinateFrame;

//////////////////////////////////////////////////////////////////////////////////////////////
// DisplayInfo struct
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    UByte   DistortionType;    
    UInt16  HResolution, VResolution;
    float   HScreenSize, VScreenSize;
    float   VCenter;
    float   LensSeparation;
    float   EyeToScreenDistance[2];
    float   DistortionK[6];
} SensorDisplayInfo;

//////////////////////////////////////////////////////////////////////////////////////////////
// Device struct
//////////////////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    int               fd;
    char              *devicePath;
    BOOLEAN           runSampleThread;
    UInt16            keepAliveIntervalMs;
    char              *name;
    char              *product;
    char              *serial;
    UInt16            vendorId;
    UInt16            productId;
    SensorDisplayInfo sensorInfo;

    hid_device *hidapi_dev;

    // Set if the sensor is located on the HMD.
    // Older prototype firmware doesn't support changing HW coordinates,
    // so we track its state.
    CoordinateFrame   Coordinates;
    CoordinateFrame   HWCoordinates;
    UInt64            NextKeepAliveTicks;

    BOOLEAN           SequenceValid;
    SInt16            LastTimestamp;
    UByte             LastSampleCount;
    float             LastTemperature;
    double            LastAcceleration[3]; // vec3_t
    double            LastRotationRate[3]; // vec3_t
    double            LastMagneticField[3]; // vec3_t

    // Current sensor range obtained from device. 
    SensorRange MaxValidRange;
    SensorRange CurrentRange;

    // Orientation goodies
    double            Q[4];    // quat_t
    double            A[3];    // vec3_t
    double            AngV[3]; // vec3_t
    float             Gain;
    float             YawMult;
    volatile BOOLEAN     EnableGravity;

    // Prediction goodies
    BOOLEAN              EnablePrediction;
	BOOLEAN			  FilterPrediction;
    float             PredictionDT;
    double            QP[4]; // quat_t

	// Testing AngV filtering suggested by Steve
	double		      AngVFilterHistory[8][3]; // vec3_t
} Device;

#endif

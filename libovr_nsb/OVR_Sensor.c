#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include <pthread.h>
#include <sys/epoll.h>

#include <libovr_nsb/OVR_Sensor.h>
#include <libovr_nsb/OVR_HID.h>

///////////////////////////////////////////////////////////////////////////////
// Quick wrapper to keep the HID stuff somewhat separate
///////////////////////////////////////////////////////////////////////////////
Device * openRift( int nthDevice, Device *myDev )
{
    Device *dev = openRiftHID(nthDevice,myDev);
    if( dev )
    {
        initDevice(dev);
    }
    return dev;
}

///////////////////////////////////////////////////////////////////////////////
// Sensor reports data in the following coordinate system:
// Accelerometer: 10^-4 m/s^2; X forward, Y right, Z Down.
// Gyro:          10^-4 rad/s; X positive roll right, Y positive pitch up; Z positive yaw right.

// We need to convert it to the following RHS coordinate system:
// X right, Y Up, Z Back (out of screen)
///////////////////////////////////////////////////////////////////////////////
void AccelFromBodyFrameUpdate( vec3_t result, const TrackerSensors *update, UByte sampleNumber,
                                  BOOLEAN convertHMDToSensor )
{
    float ax = (float)update->Samples[sampleNumber].AccelX;
    float ay = (float)update->Samples[sampleNumber].AccelY;
    float az = (float)update->Samples[sampleNumber].AccelZ;

    result[0] = ax;
    if (convertHMDToSensor)
    {
        result[1] = az;
        result[2] = -ay;
    }
    else
    {
        result[1] = ay;
        result[2] = az;
    }
    vec3_scale(result,0.0001f, 0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void MagFromBodyFrameUpdate(vec3_t result, const TrackerSensors *update, BOOLEAN convertHMDToSensor )
{    
    result[0] = update->MagX;
    if (convertHMDToSensor)
    {
        result[1] = update->MagZ;
        result[2] = -update->MagY;
    }
    else
    {
        result[1] = update->MagY;
        result[2] = update->MagZ;
    }
    vec3_scale(result,0.0001f, 0);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EulerFromBodyFrameUpdate(vec3_t result, const TrackerSensors *update, UByte sampleNumber,
                                  BOOLEAN convertHMDToSensor )
{
    float gx = (float)update->Samples[sampleNumber].GyroX;
    float gy = (float)update->Samples[sampleNumber].GyroY;
    float gz = (float)update->Samples[sampleNumber].GyroZ;

    result[0] = gx;
    if (convertHMDToSensor)
    {
        result[1] = gz;
        result[2] = -gy;
    }
    else
    {
        result[1] = gy;
        result[2] = gz;
    }
    vec3_scale(result,0.0001f, 0);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void initDevice(Device *dev)
{
    setKeepAliveInterval(dev, 1000);
    dev->Coordinates = Coord_Sensor;
    dev->HWCoordinates = Coord_HMD;
    dev->Gain = 0.5;
    dev->YawMult = 1.0;
    dev->EnablePrediction = FALSE;
    dev->EnableGravity = TRUE;
    dev->Q[3] = 1.0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void setKeepAliveInterval(Device *dev, UInt16 interval )
{
    dev->keepAliveIntervalMs = interval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void processTrackerData(Device *dev, TrackerSensors *s)
{
    const float     timeUnit   = (1.0f / 1000.f);

    if (dev->SequenceValid)
    {
        unsigned long timestampDelta;

        if (s->Timestamp < dev->LastTimestamp)
            timestampDelta = ((((int)s->Timestamp) + 0x10000) - (int)dev->LastTimestamp);
        else
            timestampDelta = (s->Timestamp - dev->LastTimestamp);

        // If we missed a small number of samples, replicate the last sample.
        if ((timestampDelta > dev->LastSampleCount) && (timestampDelta <= 254))
        {
            MessageBodyFrame sensors;
            sensors.TimeDelta     = (timestampDelta - dev->LastSampleCount) * timeUnit;

            vec3_set(dev->LastAcceleration, sensors.Acceleration);
            vec3_set(dev->LastRotationRate, sensors.RotationRate);
            vec3_set(dev->LastMagneticField, sensors.MagneticField);
            
            sensors.Temperature   = dev->LastTemperature;

            // TODO - Send faked update to listener
            updateOrientation(dev, &sensors);
        }
    }
    else
    {
        vec3_clear(dev->LastAcceleration);
        vec3_clear(dev->LastRotationRate);
        vec3_clear(dev->LastMagneticField);
        dev->LastTemperature  = 0;
        dev->SequenceValid    = TRUE;
    }

    dev->LastSampleCount = s->SampleCount;
    dev->LastTimestamp   = s->Timestamp;

    BOOLEAN convertHMDToSensor = (dev->Coordinates == Coord_Sensor) && (dev->HWCoordinates == Coord_HMD);

    MessageBodyFrame sensors;
    UByte            iterations = s->SampleCount;

    if (s->SampleCount > 3)
    {
        iterations        = 3;
        sensors.TimeDelta = (s->SampleCount - 2) * timeUnit;
    }
    else
    {
        sensors.TimeDelta = timeUnit;
    }

    UByte i;
    for (i = 0; i < iterations; i++)
    {            
        AccelFromBodyFrameUpdate(sensors.Acceleration, s, i, convertHMDToSensor);
        EulerFromBodyFrameUpdate(sensors.RotationRate, s, i, convertHMDToSensor);
        MagFromBodyFrameUpdate(sensors.MagneticField, s, convertHMDToSensor);
        sensors.Temperature  = s->Temperature * 0.01f;

        // Update our orientation
        updateOrientation(dev, &sensors);

        // TimeDelta for the last two sample is always fixed.
        sensors.TimeDelta = timeUnit;
    }

    vec3_set(sensors.Acceleration, dev->LastAcceleration);
    vec3_set(sensors.RotationRate, dev->LastRotationRate);
    vec3_set(sensors.MagneticField, dev->LastMagneticField);
    dev->LastTemperature  = sensors.Temperature;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Data
/////////////////////////////////////////////////////////////////////////////////////////////
void UnpackSensor(const UByte* buffer, SInt32* x, SInt32* y, SInt32* z)
{
    // Sign extending trick
    // from http://graphics.stanford.edu/~seander/bithacks.html#FixedSignExtend
    struct {SInt32 x:21;} s;

    *x = s.x = (buffer[0] << 13) | (buffer[1] << 5) | ((buffer[2] & 0xF8) >> 3);
    *y = s.x = ((buffer[2] & 0x07) << 18) | (buffer[3] << 10) | (buffer[4] << 2) |
               ((buffer[5] & 0xC0) >> 6);
    *z = s.x = ((buffer[5] & 0x3F) << 15) | (buffer[6] << 7) | (buffer[7] >> 1);
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Handle tracker message
/////////////////////////////////////////////////////////////////////////////////////////////
TrackerMessageType DecodeTracker(const UByte* buffer, TrackerSensors *sensorMsg, int size)
{
    if (size < 62)
    {
        return TrackerMessage_SizeError;
    }

    sensorMsg->SampleCount		= buffer[1];
    sensorMsg->Timestamp		= DecodeUInt16(buffer + 2);
    sensorMsg->LastCommandID	= DecodeUInt16(buffer + 4);
    sensorMsg->Temperature		= DecodeSInt16(buffer + 6);
    
    //if (SampleCount > 2)        
    //    OVR_DEBUG_LOG_TEXT(("TackerSensor::Decode SampleCount=%d\n", SampleCount));        

    // Only unpack as many samples as there actually are
    UByte iterationCount = (sensorMsg->SampleCount > 2) ? 3 : sensorMsg->SampleCount;

    UByte i;
    for (i = 0; i < iterationCount; i++)
    {
        UnpackSensor(buffer + 8 + 16 * i,  &sensorMsg->Samples[i].AccelX, &sensorMsg->Samples[i].AccelY, &sensorMsg->Samples[i].AccelZ);
        UnpackSensor(buffer + 16 + 16 * i, &sensorMsg->Samples[i].GyroX,  &sensorMsg->Samples[i].GyroY,  &sensorMsg->Samples[i].GyroZ);
    }

    sensorMsg->MagX = DecodeSInt16(buffer + 56);
    sensorMsg->MagY = DecodeSInt16(buffer + 58);
    sensorMsg->MagZ = DecodeSInt16(buffer + 60);

    return TrackerMessage_Sensors;
}



/////////////////////////////////////////////////////////////////////////////////////////////
// Sensor HW only accepts specific maximum range values, used to maximize
// the 16-bit sensor outputs. Use these ramps to specify and report appropriate values.
/////////////////////////////////////////////////////////////////////////////////////////////
static const UInt16 AccelRangeRamp[] = { 2, 4, 8, 16 };
static const UInt16 GyroRangeRamp[]  = { 250, 500, 1000, 2000 };
static const UInt16 MagRangeRamp[]   = { 880, 1300, 1900, 2500 };

UInt16 SelectSensorRampValue(const UInt16* ramp, unsigned count,
                                    float val, float factor, const char* label)
{    
    UInt16 threshold = (UInt16)(val * factor);

    UByte i;
    for (i = 0; i<count; i++)
    {
        if (ramp[i] >= threshold)
            return ramp[i];
    }
    //OVR_DEBUG_LOG(("SensorDevice::SetRange - %s clamped to %0.4f",
                   //label, float(ramp[count-1]) / factor));
    //OVR_UNUSED2(factor, label);
    return ramp[count-1];
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void SetSensorRange(struct SensorScaleRange *s, const SensorRange *r )
{
    s->AccelScale = SelectSensorRampValue(AccelRangeRamp, sizeof(AccelRangeRamp)/sizeof(AccelRangeRamp[0]),
            r->MaxAcceleration, (1.0f / 9.81f), "MaxAcceleration");
    s->GyroScale  = SelectSensorRampValue(GyroRangeRamp, sizeof(GyroRangeRamp)/sizeof(GyroRangeRamp[0]),
            r->MaxRotationRate, RAD_TO_DEG, "MaxRotationRate");
    s->MagScale   = SelectSensorRampValue(MagRangeRamp, sizeof(MagRangeRamp)/sizeof(MagRangeRamp[0]),
            r->MaxMagneticField, 1000.0f, "MaxMagneticField");
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void GetSensorRange(SensorRange* r, struct SensorScaleRange *s)
{
    r->MaxAcceleration = s->AccelScale * 9.81f;
    r->MaxRotationRate = DEG_TO_RAD * ((float)s->GyroScale);
    r->MaxMagneticField= s->MagScale * 0.001f;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void updateOrientation(Device *dev, MessageBodyFrame *msg)
{
    vec3_set(msg->RotationRate,dev->AngV);
    //dev->AngV = msg->RotationRate;
    dev->AngV[_Y_] *= dev->YawMult;
    vec3_scale(msg->Acceleration, msg->TimeDelta, dev->A);
    //dev->A = msg->Acceleration * msg->TimeDelta;

    //printf("A:%+-10g %+-10g %+-10g\t", dev->A[0], dev->A[1], dev->A[2] ); 

    /*
    // Mike's original integration approach. Subdivision to reduce error.
    Quatf q = AngVToYawPitchRollQuatf(msg.AngV * msg.TimeDelta * (1.0f / 16.0f));
    Quatf q2 = q * q;
    Quatf q4 = q2 * q2;
    Quatf q8 = q4 * q4;
    Q = q8 * q8 * Q;
    */

    // Integration based on exact movement on 4D unit quaternion sphere.
    // Developed by Steve & Anna, this technique is based on Lie algebra
    // and exponential map.
    
    double dV[3];
    vec3_scale(dev->AngV, msg->TimeDelta, dV);
    //vec3_t    dV    = dev->AngV * msg->TimeDelta;
    const float angle = vec3_length(dV);  // Magnitude of angular velocity.

    if (angle > 0.0f)
    {
        float halfa = angle * 0.5f;
        float sina  = sin(halfa) / angle;
        double dQ[4]; // quat_t
        dQ[0] = dV[_X_]*sina;
        dQ[1] = dV[_Y_]*sina;
        dQ[2] = dV[_Z_]*sina;
        dQ[3] = cos(halfa);
        //quat_t dQ(dV[_X_]*sina, dV[_Y_]*sina, dV[_Z_]sina, cos(halfa));
        quat_multiply(dev->Q, dQ, 0);
        //dev->Q =  dev->Q * dQ;
        
        //printf("DQ:%+-10g %+-10g %+-10g %+-10g", dQ[0], dQ[1], dQ[2], dQ[3] ); 
        //printf("\tQ:%+-10g %+-10g %+-10g %+-10g", dev->Q[0], dev->Q[1], dev->Q[2], dev->Q[3] ); 

        if (dev->EnablePrediction)
        {
            double AngVF[3];
			GetAngVFilterVal(dev, AngVF);
            float angSpeed = vec3_length(AngVF);
            if (angSpeed > 0.001f)
            {
                double axis[3];
                vec3_set(AngVF,axis);
                vec3_scale(axis, 1.0 / angSpeed,0);
                //axis = AngVF / angSpeed;
                float       halfaP = angSpeed * (msg->TimeDelta + dev->PredictionDT) * 0.5f;
                double       dQP[4]; // quat_t
                //dQP[3] = 1;
                //quat_t       dQP(0, 0, 0, 1);
                float       sinaP  = sin(halfaP);  
                dQP[0] = axis[_X_]*sinaP;
                dQP[1] = axis[_Y_]*sinaP;
                dQP[2] = axis[_Z_]*sinaP;
                dQP[3] = cos(halfaP);
                //dQP = quat_t(axis[_X_]*sinaP, axis[_Y_]*sinaP, axis[_Z_]*sinaP, cos(halfaP));
                quat_multiply(dev->Q, dQP, dev->QP);
                //dev->QP =  dev->Q * dQP;
            }
            else
            {
                quat_set(dev->Q, dev->QP);
                //dev->QP = dev->Q;
            }
        }
        else
        {
            quat_set(dev->Q, dev->QP);
            //dev->QP = dev->Q;
        }
    }    

    
    // This introduces gravity drift adjustment based on gain
    float        accelMagnitude = vec3_length(msg->Acceleration);
    float        angVMagnitude  = vec3_length(dev->AngV);
    const float  gravityEpsilon = 0.4f;
    const float  angVEpsilon    = 3.0f; // Relatively slow rotation
    
    if (dev->EnableGravity &&
        (fabs(accelMagnitude - 9.81f) < gravityEpsilon) &&
        (angVMagnitude < angVEpsilon))
    {
        // TBD: Additional conditions:
        //  - Angular velocity < epsilon, or
        //  - Angle of transformed Acceleration < epsilon

        //printf("AC");

        double yUp[3]; // vec3_t
        yUp[0] = 0;
        yUp[1] = 1;
        yUp[2] = 0;
        //vec3_t yUp(0,1,0);
        double aw[3];
        quat_rotate(dev->Q, dev->A, aw);
        //vec3_t aw = dev->Q.Rotate(dev->A);

        double    qfeedback[4]; // quat_t
        qfeedback[0] = -aw[_Z_] * dev->Gain;
        qfeedback[1] = 0;
        qfeedback[2] = aw[_X_] * dev->Gain;
        qfeedback[3] = 1;
        //quat_t    qfeedback(-aw[_Z_] * dev->Gain, 0, aw[_X_] * dev->Gain, 1);

        double    q1[4]; // quat_t
        quat_multiply(qfeedback,dev->Q,q1);
        quat_normalize(q1,0);
        //quat_t    q1 = (qfeedback * dev->Q).Normalized();

        float angle0 = vec3_angle(yUp,aw);
        //float    angle0 = yUp.Angle(aw);
        
        double temp[3];
        quat_rotate(q1,dev->A,temp);
        float angle1 = vec3_angle(yUp,temp);
        //float    angle1 = yUp.Angle(q1.Rotate(dev->A));

        if (angle1 < angle0)
        {
            quat_set(q1,dev->Q);
            //dev->Q = q1;
        }
        else
        {
            double qfeedback2[4]; // quat_t
            qfeedback2[0] = aw[_Z_] * dev->Gain;
            qfeedback2[1] =  0;
            qfeedback2[2] = -aw[_X_] * dev->Gain;
            qfeedback2[3] = 1;
            //quat_t    qfeedback2(aw[_Z_] * dev->Gain, 0, -aw[_X_] * dev->Gain, 1);
            double q2[4]; // quat_t
            quat_multiply(qfeedback2,dev->Q,q2);
            quat_normalize(q2,0);
            //quat_t    q2 = (qfeedback2 * dev->Q).Normalized();

            double temp2[3];
            quat_rotate(q2,dev->A,temp2);
            float angle2 = vec3_angle(yUp,temp2);
            //float    angle2 = yUp.Angle(q2.Rotate(dev->A));

            if (angle2 < angle0)
            {
                quat_set(q2,dev->Q);
                //dev->Q = q2;
            }
        }
    }    

    //printf("\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void ResetAngVFilter(Device *dev)
{
    double temp[3];
    int i;
	for (i = 0; i < 8; i++)
    {
        vec3_set(temp,dev->AngVFilterHistory[i]);
		//dev->AngVFilterHistory[i] = vec3_(0,0,0);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
void GetAngVFilterVal(Device *dev, vec3_t out)
{
    double temp[3];
	if(dev->FilterPrediction == FALSE)
	{
        vec3_set(dev->AngV,out);
		return;
	}

	// rotate history and add latest value
    int i;
	for (i = 6; i >= 0 ; i--)
    {
        vec3_set(dev->AngVFilterHistory[i], dev->AngVFilterHistory[i+1]);
		//dev->AngVFilterHistory[i+1] = dev->AngVFilterHistory[i];	
    }
	
    vec3_set(dev->AngV,dev->AngVFilterHistory[0]);
	//dev->AngVFilterHistory[0] = in;

    vec3_clear(out);
	vec3_scale(dev->AngVFilterHistory[0], 0.41667f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[1], 0.33333f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[2], 0.025f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[3], 0.16667f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[4], 0.08333f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[5], 0.0f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[6], -0.08333f, temp);
    vec3_add(out,temp,0);
	vec3_scale(dev->AngVFilterHistory[7], -0.16667f, temp);
    vec3_add(out,temp,0);
}

/////////////////////////////////////////////////////////////////////////////////////
// Read a single tracker info message
/////////////////////////////////////////////////////////////////////////////////////
BOOLEAN sampleDevice( Device *dev )
{
    int res;
    UInt8 buf[256];

    res = readSample(dev, buf, 256);
    return processSample(dev,buf,res);
}

/////////////////////////////////////////////////////////////////////////////////////
// Non-blocking single-sample
/////////////////////////////////////////////////////////////////////////////////////
BOOLEAN waitSampleDevice(Device *dev, UInt16 waitMsec)
{
    int res;
    UInt8 buf[256];

    res = waitForSample(dev, waitMsec, buf, 256);
    return processSample(dev,buf,res);
}


/////////////////////////////////////////////////////////////////////////////////////
// Read a single tracker info me
/////////////////////////////////////////////////////////////////////////////////////
BOOLEAN processSample(Device *dev, UInt8 *buf, UInt16 len )
{
    if (len <= 0) 
    {
        return FALSE;
    } 
    else 
    {
        if ( len == 62 )
        {
            TrackerSensors sensorMsg;
            if( DecodeTracker((UByte *)buf,&sensorMsg, len) != TrackerMessage_SizeError )
            {
                processTrackerData(dev, &sensorMsg);
            }
        }
    }
    return TRUE;
}

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include <gl-matrix.h>

#include "OVR_Defs.h"

// Reported data is little-endian now
UInt16 DecodeUInt16(const UByte* buffer)
{
    return (buffer[1] << 8) |buffer[0];
}

SInt16 DecodeSInt16(const UByte* buffer)
{
    return (buffer[1] << 8) | buffer[0];
}

UInt32 DecodeUInt32(const UByte* buffer)
{    
    return (buffer[0]) | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);    
}

float DecodeFloat(const UByte* buffer)
{
    union {
        UInt32 U;
        float  F;
    } u;


    u.U = DecodeUInt32(buffer);
    return u.F;
}

void vec3_clear(vec3_t v)
{
    memset(v,0,sizeof(double)*3);
}

double vec3_angle(vec3_t v1, vec3_t v2)
{
    return acos(vec3_dot(v1,v2) / (vec3_length(v1)*vec3_length(v2)));
}

vec3_t quat_rotate(quat_t q, vec3_t v, vec3_t result)
{
    double qbuf1[4];
    double qbuf2[4];
    double qbuf3[4];

    quat_t temp = qbuf1;
    quat_t temp2 = qbuf2;
    quat_t qInv = qbuf3;

    if( result == 0 )
    {
        result = vec3_create(0);
    }
    temp[_X_] = v[_X_];
    temp[_Y_] = v[_Y_];
    temp[_Z_] = v[_Z_];

    quat_multiply(q,temp,temp2);
    quat_inverse(q,qInv);
    quat_multiply(temp2,qInv,temp);
    
    result[_X_] = temp[_X_];
    result[_Y_] = temp[_Y_];
    result[_Z_] = temp[_Z_];

    return result;
}



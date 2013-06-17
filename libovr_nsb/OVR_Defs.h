#if !defined(_OVR_DEFS_H)
#define _OVR_DEFS_H

// For vector readability 
#define _X_ 0
#define _Y_ 1
#define _Z_ 2

#if !defined(DEG_TO_RAD)
#define DEG_TO_RAD (1.0 / 180.0 * M_PI)
#define RAD_TO_DEG (180.0 / M_PI)
#endif

typedef unsigned char BOOLEAN;

#if !defined(TRUE)
#define TRUE 1
#define FALSE 0
#endif

typedef unsigned long long  UInt64;
typedef          long long  SInt64;
typedef unsigned      long  UInt32;
typedef               long  SInt32;
typedef unsigned      short UInt16;
typedef               short SInt16;
typedef unsigned      char  UByte;
typedef               char  SByte;
typedef unsigned      char  UInt8;
typedef               char  SInt8;

UInt16 DecodeUInt16(const UByte* buffer);
SInt16 DecodeSInt16(const UByte* buffer);
UInt32 DecodeUInt32(const UByte* buffer);
float DecodeFloat(const UByte* buffer);
void vec3_clear(vec3_t v);
double vec3_angle(vec3_t v1, vec3_t v2);
vec3_t quat_rotate(quat_t q, vec3_t v, vec3_t result);

#endif

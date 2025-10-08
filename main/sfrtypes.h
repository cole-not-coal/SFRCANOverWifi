/*
sfrtypes.h | Sheffield Formula Racing
Generic types for SFR codebase

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#ifndef SFRTypes
#define TRUE 1
#define FALSE 0

typedef int boolean;

typedef unsigned char byte;
typedef signed char sbyte;

typedef unsigned short word;
typedef signed short sword;

typedef unsigned long dword;
typedef signed long sdword;

typedef unsigned long long qword;
typedef signed long long sqword;

typedef struct {
    dword dwId;      // CAN ID (11- bit packed in 16-bit)
    byte  bDLC;      // 0-8 (Data Length Code)
    byte  abData[8];  // up to 8 bytes
} CAN_frame_t;

#define SFRTypes
#endif
#pragma once

#define VRI_API_VERSION 1.0

#define VRI_HANDLE(obj) typedef struct obj##_T* obj

#ifndef ARRAY_LENGTH
#    define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef MIN
#    define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#    define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif


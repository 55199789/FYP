#ifndef UDER_TYPES_Z
#define UDER_TYPES_Z
#include <stdbool.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define TIMEPRINT false
#define DATATYPE int64_t
#define SGXSSL_CTR_BITS	128
#define SHIFT_BYTE	8

#if defined(__cplusplus)
extern "C" {
#endif
#pragma pack(1)
typedef struct Element {
    unsigned int idx; 
    // 1 -> postive, 0 -> negative
    bool sign; 
} Element;
#pragma pack()
#if defined(__cplusplus)
}
#endif

#endif
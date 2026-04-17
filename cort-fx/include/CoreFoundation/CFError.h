#ifndef FX_COREFOUNDATION_CFERROR_H
#define FX_COREFOUNDATION_CFERROR_H

#include "CFBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

CF_EXPORT CFTypeID CFErrorGetTypeID(void);
CF_EXPORT CFIndex CFErrorGetCode(CFErrorRef err);

#if defined(__cplusplus)
}
#endif

#endif

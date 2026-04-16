#include "FXCFInternal.h"

#include <CoreFoundation/CFNumber.h>

#include <float.h>
#include <limits.h>

struct __CFBoolean {
    CFRuntimeBase _base;
    Boolean value;
};

struct __CFNumber {
    CFRuntimeBase _base;
    CFNumberType type;
    union {
        SInt64 i64;
        double f64;
    } value;
};

static CFHashCode __CFHashUInt64(uint64_t value) {
    value ^= value >> 33u;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33u;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33u;
    return (CFHashCode)value;
}

static CFHashCode __CFHashSInt64(SInt64 value) {
    return __CFHashUInt64((uint64_t)value);
}

static CFHashCode __CFHashFloat64(double value) {
    union {
        double f64;
        uint64_t bits;
    } storage;

    storage.f64 = value;
    return __CFHashUInt64(storage.bits);
}

static Boolean __CFDoubleToSInt64Exact(double value, SInt64 *result) {
    if (value != value) {
        return false;
    }
    if (value > DBL_MAX || value < -DBL_MAX) {
        return false;
    }
    if (value < (double)LLONG_MIN || value > (double)LLONG_MAX) {
        return false;
    }

    SInt64 converted = (SInt64)value;
    if ((double)converted != value) {
        return false;
    }

    if (result != NULL) {
        *result = converted;
    }
    return true;
}

static Boolean __CFBooleanEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFBoolean *boolean1 = (const struct __CFBoolean *)cf1;
    const struct __CFBoolean *boolean2 = (const struct __CFBoolean *)cf2;
    return boolean1->value == boolean2->value;
}

static CFHashCode __CFBooleanHash(CFTypeRef cf) {
    const struct __CFBoolean *boolean = (const struct __CFBoolean *)cf;
    return __CFHashSInt64(boolean->value ? 1 : 0);
}

static const CFRuntimeClass __CFBooleanClass = {
    0,
    "CFBoolean",
    NULL,
    NULL,
    NULL,
    __CFBooleanEqual,
    __CFBooleanHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

static struct __CFBoolean __kCFBooleanTrue = {
    {
        (uintptr_t)&__CFBooleanClass,
        FX_CF_INFO_USES_DEFAULT_ALLOCATOR | FX_CF_INFO_IMMORTAL | ((uint64_t)_kCFRuntimeIDCFBoolean << FX_CF_INFO_TYPE_SHIFT)
    },
    true
};

static struct __CFBoolean __kCFBooleanFalse = {
    {
        (uintptr_t)&__CFBooleanClass,
        FX_CF_INFO_USES_DEFAULT_ALLOCATOR | FX_CF_INFO_IMMORTAL | ((uint64_t)_kCFRuntimeIDCFBoolean << FX_CF_INFO_TYPE_SHIFT)
    },
    false
};

const CFBooleanRef kCFBooleanTrue = &__kCFBooleanTrue;
const CFBooleanRef kCFBooleanFalse = &__kCFBooleanFalse;

static Boolean __CFNumberEqual(CFTypeRef cf1, CFTypeRef cf2) {
    const struct __CFNumber *number1 = (const struct __CFNumber *)cf1;
    const struct __CFNumber *number2 = (const struct __CFNumber *)cf2;

    if (number1->type == number2->type) {
        if (number1->type == kCFNumberFloat64Type) {
            return number1->value.f64 == number2->value.f64;
        }
        return number1->value.i64 == number2->value.i64;
    }

    if (number1->type != kCFNumberFloat64Type && number2->type != kCFNumberFloat64Type) {
        return number1->value.i64 == number2->value.i64;
    }

    if (number1->type == kCFNumberFloat64Type) {
        SInt64 integerValue = 0;
        return __CFDoubleToSInt64Exact(number1->value.f64, &integerValue) && integerValue == number2->value.i64;
    }

    if (number2->type == kCFNumberFloat64Type) {
        SInt64 integerValue = 0;
        return __CFDoubleToSInt64Exact(number2->value.f64, &integerValue) && integerValue == number1->value.i64;
    }

    return false;
}

static CFHashCode __CFNumberHash(CFTypeRef cf) {
    const struct __CFNumber *number = (const struct __CFNumber *)cf;

    if (number->type == kCFNumberFloat64Type) {
        SInt64 integerValue = 0;
        if (__CFDoubleToSInt64Exact(number->value.f64, &integerValue)) {
            return __CFHashSInt64(integerValue);
        }
        return __CFHashFloat64(number->value.f64);
    }

    return __CFHashSInt64(number->value.i64);
}

static const CFRuntimeClass __CFNumberClass = {
    0,
    "CFNumber",
    NULL,
    NULL,
    NULL,
    __CFNumberEqual,
    __CFNumberHash,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFBooleanClass(void) {
    return &__CFBooleanClass;
}

const CFRuntimeClass *_FXCFNumberClass(void) {
    return &__CFNumberClass;
}

CFTypeID CFBooleanGetTypeID(void) {
    return _kCFRuntimeIDCFBoolean;
}

Boolean CFBooleanGetValue(CFBooleanRef boolean) {
    _FXCFValidateType((CFTypeRef)boolean, CFBooleanGetTypeID(), "CFBooleanGetValue called with non-boolean object");
    return ((const struct __CFBoolean *)boolean)->value;
}

CFTypeID CFNumberGetTypeID(void) {
    return _kCFRuntimeIDCFNumber;
}

CFNumberRef CFNumberCreate(CFAllocatorRef allocator, CFNumberType theType, const void *valuePtr) {
    if (valuePtr == NULL) {
        return NULL;
    }

    struct __CFNumber *number = (struct __CFNumber *)_CFRuntimeCreateInstance(
        allocator,
        CFNumberGetTypeID(),
        (CFIndex)(sizeof(struct __CFNumber) - sizeof(CFRuntimeBase)),
        NULL
    );
    if (number == NULL) {
        return NULL;
    }

    switch (theType) {
        case kCFNumberSInt32Type:
            number->type = kCFNumberSInt32Type;
            number->value.i64 = *(const SInt32 *)valuePtr;
            return number;
        case kCFNumberSInt64Type:
            number->type = kCFNumberSInt64Type;
            number->value.i64 = *(const SInt64 *)valuePtr;
            return number;
        case kCFNumberFloat64Type:
            number->type = kCFNumberFloat64Type;
            number->value.f64 = *(const double *)valuePtr;
            return number;
        default:
            CFRelease((CFTypeRef)number);
            return NULL;
    }
}

CFNumberType CFNumberGetType(CFNumberRef number) {
    _FXCFValidateType((CFTypeRef)number, CFNumberGetTypeID(), "CFNumberGetType called with non-number object");
    return ((const struct __CFNumber *)number)->type;
}

Boolean CFNumberGetValue(CFNumberRef number, CFNumberType theType, void *valuePtr) {
    _FXCFValidateType((CFTypeRef)number, CFNumberGetTypeID(), "CFNumberGetValue called with non-number object");
    if (valuePtr == NULL) {
        return false;
    }

    const struct __CFNumber *typedNumber = (const struct __CFNumber *)number;

    switch (theType) {
        case kCFNumberSInt32Type: {
            SInt32 result = 0;
            Boolean exact = false;

            if (typedNumber->type == kCFNumberFloat64Type) {
                SInt64 converted = 0;
                exact =
                    __CFDoubleToSInt64Exact(typedNumber->value.f64, &converted) &&
                    converted >= INT32_MIN &&
                    converted <= INT32_MAX;
                result = (SInt32)converted;
            } else {
                exact = typedNumber->value.i64 >= INT32_MIN && typedNumber->value.i64 <= INT32_MAX;
                result = (SInt32)typedNumber->value.i64;
            }

            *(SInt32 *)valuePtr = result;
            return exact;
        }
        case kCFNumberSInt64Type: {
            if (typedNumber->type == kCFNumberFloat64Type) {
                SInt64 result = 0;
                Boolean exact = __CFDoubleToSInt64Exact(typedNumber->value.f64, &result);
                *(SInt64 *)valuePtr = result;
                return exact;
            }

            *(SInt64 *)valuePtr = typedNumber->value.i64;
            return true;
        }
        case kCFNumberFloat64Type: {
            double result = typedNumber->type == kCFNumberFloat64Type ? typedNumber->value.f64 : (double)typedNumber->value.i64;
            *(double *)valuePtr = result;

            if (typedNumber->type == kCFNumberFloat64Type) {
                return true;
            }
            return (SInt64)result == typedNumber->value.i64;
        }
        default:
            return false;
    }
}

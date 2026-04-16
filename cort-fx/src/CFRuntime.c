#include "FXCFInternal.h"

static const CFRuntimeClass __CFNotATypeClass = {
    0,
    "Not A Type",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

static const CFRuntimeClass __CFTypeClass = {
    0,
    "CFType",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

static pthread_once_t __FXCFInitializeOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t __FXCFClassTableLock = PTHREAD_MUTEX_INITIALIZER;
static _Atomic(uintptr_t) __FXCFClassTable[FX_CF_MAX_RUNTIME_TYPES];
static CFTypeID __FXCFNextDynamicTypeID = _kCFRuntimeStartingClassID;

static void __FXCFPublishClass(CFTypeID typeID, const CFRuntimeClass *cls) {
    if (typeID >= FX_CF_MAX_RUNTIME_TYPES) {
        _FXCFAbort("builtin runtime type ID exceeds class table size");
    }
    atomic_store_explicit(&__FXCFClassTable[typeID], (uintptr_t)cls, memory_order_release);
}

static void __FXCFInitializeRuntime(void) {
    __FXCFPublishClass(_kCFRuntimeIDNotAType, &__CFNotATypeClass);
    __FXCFPublishClass(_kCFRuntimeIDCFType, &__CFTypeClass);
    __FXCFPublishClass(_kCFRuntimeIDCFAllocator, _FXCFAllocatorClass());
    __FXCFPublishClass(_kCFRuntimeIDCFBoolean, _FXCFBooleanClass());
    __FXCFPublishClass(_kCFRuntimeIDCFNumber, _FXCFNumberClass());
    __FXCFPublishClass(_kCFRuntimeIDCFData, _FXCFDataClass());
    __FXCFPublishClass(_kCFRuntimeIDCFDate, _FXCFDateClass());
    __FXCFPublishClass(_kCFRuntimeIDCFString, _FXCFStringClass());
    __FXCFPublishClass(_kCFRuntimeIDCFArray, _FXCFArrayClass());
    __FXCFPublishClass(_kCFRuntimeIDCFDictionary, _FXCFDictionaryClass());
}

static void __FXCFInitialize(void) {
    pthread_once(&__FXCFInitializeOnce, __FXCFInitializeRuntime);
}

static const CFRuntimeClass *__FXCFClassForTypeID(CFTypeID typeID) {
    __FXCFInitialize();
    if (typeID >= FX_CF_MAX_RUNTIME_TYPES) {
        return NULL;
    }
    return (const CFRuntimeClass *)atomic_load_explicit(&__FXCFClassTable[typeID], memory_order_acquire);
}

CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass *cls) {
    __FXCFInitialize();
    if (cls == NULL || cls->className == NULL) {
        return _kCFRuntimeNotATypeID;
    }
    if ((cls->version & _kCFRuntimeCustomRefCount) != 0) {
        return _kCFRuntimeNotATypeID;
    }

    pthread_mutex_lock(&__FXCFClassTableLock);
    if (__FXCFNextDynamicTypeID >= FX_CF_MAX_RUNTIME_TYPES) {
        pthread_mutex_unlock(&__FXCFClassTableLock);
        return _kCFRuntimeNotATypeID;
    }
    CFTypeID typeID = __FXCFNextDynamicTypeID++;
    atomic_store_explicit(&__FXCFClassTable[typeID], (uintptr_t)cls, memory_order_release);
    pthread_mutex_unlock(&__FXCFClassTableLock);
    return typeID;
}

const CFRuntimeClass *_CFRuntimeGetClassWithTypeID(CFTypeID typeID) {
    return __FXCFClassForTypeID(typeID);
}

void _CFRuntimeUnregisterClassWithTypeID(CFTypeID typeID) {
    (void)typeID;
}

static CFAllocatorRef __FXCFGetAllocatorFromObject(CFTypeRef cf) {
    if (cf == NULL) {
        return kCFAllocatorSystemDefault;
    }
    CFRuntimeBase *base = _FXCFBaseFromRef(cf);
    uint64_t info = atomic_load_explicit(&base->_cfinfoa, memory_order_acquire);
    if ((info & FX_CF_INFO_USES_DEFAULT_ALLOCATOR) != 0) {
        return kCFAllocatorSystemDefault;
    }
    return *(CFAllocatorRef *)((unsigned char *)base - FX_CF_ALLOCATOR_PREFIX_SIZE);
}

CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category) {
    (void)category;
    __FXCFInitialize();
    if (extraBytes < 0) {
        return NULL;
    }

    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID);
    if (cls == NULL || typeID == _kCFRuntimeNotATypeID || typeID == _kCFRuntimeIDCFType) {
        return NULL;
    }
    if ((cls->version & _kCFRuntimeCustomRefCount) != 0) {
        return NULL;
    }

    CFAllocatorRef realAllocator = allocator == NULL ? CFAllocatorGetDefault() : allocator;
    if (realAllocator == kCFAllocatorNull) {
        return NULL;
    }

    Boolean usesDefaultAllocator = _FXCFAllocatorIsSystemDefault(realAllocator);
    size_t prefixSize = usesDefaultAllocator ? 0u : FX_CF_ALLOCATOR_PREFIX_SIZE;
    size_t objectSize = sizeof(CFRuntimeBase) + (size_t)extraBytes;
    size_t allocationSize = FX_CF_ALIGN_16(prefixSize + objectSize);
    unsigned char *raw = NULL;
    Boolean needsClear = false;

    if ((cls->version & _kCFRuntimeRequiresAlignment) != 0) {
        if (!usesDefaultAllocator) {
            return NULL;
        }
        uintptr_t alignment = cls->requiredAlignment;
        if (alignment < alignof(max_align_t)) {
            alignment = alignof(max_align_t);
        }
        if ((alignment & (alignment - 1u)) != 0u) {
            return NULL;
        }
        void *aligned = NULL;
        if (posix_memalign(&aligned, alignment, allocationSize) != 0) {
            return NULL;
        }
        raw = aligned;
        needsClear = true;
    } else {
        CFOptionFlags hint = _FXCFAllocatorRespectsHintZero(realAllocator) ? FX_CF_ALLOCATOR_HINT_ZERO : 0;
        raw = CFAllocatorAllocate(realAllocator, (CFIndex)allocationSize, hint);
        needsClear = hint != FX_CF_ALLOCATOR_HINT_ZERO;
    }

    if (raw == NULL) {
        return NULL;
    }
    if (needsClear) {
        memset(raw, 0, allocationSize);
    }

    CFRuntimeBase *base = (CFRuntimeBase *)(void *)(raw + prefixSize);
    if (!usesDefaultAllocator) {
        *(CFAllocatorRef *)raw = (CFAllocatorRef)CFRetain((CFTypeRef)realAllocator);
    }

    base->_cfisa = (uintptr_t)cls;
    atomic_store_explicit(&base->_cfinfoa, _FXCFInfoMake(typeID, 1, usesDefaultAllocator, false), memory_order_release);

    if (cls->init != NULL) {
        cls->init((CFTypeRef)base);
    }
    return (CFTypeRef)base;
}

void _CFRuntimeInitStaticInstance(void *memory, CFTypeID typeID) {
    __FXCFInitialize();
    if (memory == NULL) {
        return;
    }
    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID);
    if (cls == NULL) {
        return;
    }

    CFRuntimeBase *base = (CFRuntimeBase *)memory;
    base->_cfisa = (uintptr_t)cls;
    atomic_store_explicit(&base->_cfinfoa, _FXCFInfoMake(typeID, 0, true, true), memory_order_release);
    if (cls->init != NULL) {
        cls->init((CFTypeRef)base);
    }
}

CFTypeID CFGetTypeID(CFTypeRef cf) {
    if (cf == NULL) {
        _FXCFAbort("CFGetTypeID called with NULL");
    }
    CFRuntimeBase *base = _FXCFBaseFromRef(cf);
    uint64_t info = atomic_load_explicit(&base->_cfinfoa, memory_order_acquire);
    CFTypeID typeID = _FXCFTypeIDFromInfo(info);
    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID);
    if (cls == NULL || _FXCFClassFromBase(base) != cls) {
        _FXCFAbort("CFGetTypeID called with non-CORT object");
    }
    return typeID;
}

Boolean CFEqual(CFTypeRef cf1, CFTypeRef cf2) {
    if (cf1 == NULL || cf2 == NULL) {
        _FXCFAbort("CFEqual called with NULL");
    }

    CFTypeID typeID1 = CFGetTypeID(cf1);
    CFTypeID typeID2 = CFGetTypeID(cf2);
    if (cf1 == cf2) {
        return true;
    }
    if (typeID1 != typeID2) {
        return false;
    }

    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID1);
    if (cls == NULL) {
        _FXCFAbort("CFEqual called with non-CORT object");
    }
    if (cls->equal != NULL) {
        return cls->equal(cf1, cf2);
    }
    return false;
}

CFHashCode CFHash(CFTypeRef cf) {
    if (cf == NULL) {
        _FXCFAbort("CFHash called with NULL");
    }

    CFTypeID typeID = CFGetTypeID(cf);
    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID);
    if (cls == NULL) {
        _FXCFAbort("CFHash called with non-CORT object");
    }
    if (cls->hash != NULL) {
        return cls->hash(cf);
    }

    uintptr_t bits = (uintptr_t)cf;
    bits ^= bits >> 33u;
    bits *= 0xff51afd7ed558ccdULL;
    bits ^= bits >> 33u;
    return (CFHashCode)bits;
}

CFIndex CFGetRetainCount(CFTypeRef cf) {
    if (cf == NULL) {
        _FXCFAbort("CFGetRetainCount called with NULL");
    }
    CFRuntimeBase *base = _FXCFBaseFromRef(cf);
    uint64_t info = atomic_load_explicit(&base->_cfinfoa, memory_order_acquire);
    if (_FXCFInfoIsImmortal(info)) {
        return LONG_MAX;
    }
    return (CFIndex)_FXCFRetainCountFromInfo(info);
}

CFTypeRef CFRetain(CFTypeRef cf) {
    if (cf == NULL) {
        _FXCFAbort("CFRetain called with NULL");
    }

    CFRuntimeBase *base = _FXCFBaseFromRef(cf);
    uint64_t info = atomic_load_explicit(&base->_cfinfoa, memory_order_acquire);
    for (;;) {
        if (_FXCFInfoIsImmortal(info)) {
            return cf;
        }
        if ((info & (FX_CF_INFO_DEALLOCATING | FX_CF_INFO_DEALLOCATED)) != 0) {
            _FXCFAbort("CFRetain called on deallocating object");
        }
        if (_FXCFRetainCountFromInfo(info) == UINT32_MAX) {
            _FXCFAbort("CFRetain retain-count overflow");
        }
        uint64_t newInfo = info + FX_CF_INFO_RETAIN_INCREMENT;
        if (atomic_compare_exchange_weak_explicit(&base->_cfinfoa, &info, newInfo, memory_order_acq_rel, memory_order_acquire)) {
            return cf;
        }
    }
}

void CFRelease(CFTypeRef cf) {
    if (cf == NULL) {
        _FXCFAbort("CFRelease called with NULL");
    }

    CFRuntimeBase *base = _FXCFBaseFromRef(cf);
    uint64_t info = atomic_load_explicit(&base->_cfinfoa, memory_order_acquire);
    CFTypeID typeID = _FXCFTypeIDFromInfo(info);
    const CFRuntimeClass *cls = __FXCFClassForTypeID(typeID);
    if (cls == NULL) {
        _FXCFAbort("CFRelease called with non-CORT object");
    }

    for (;;) {
        if (_FXCFInfoIsImmortal(info)) {
            return;
        }
        if ((info & (FX_CF_INFO_DEALLOCATING | FX_CF_INFO_DEALLOCATED)) != 0) {
            _FXCFAbort("CFRelease called on deallocating object");
        }

        uint32_t retainCount = _FXCFRetainCountFromInfo(info);
        if (retainCount == 0) {
            _FXCFAbort("CFRelease over-release detected");
        }

        if (retainCount > 1) {
            uint64_t newInfo = info - FX_CF_INFO_RETAIN_INCREMENT;
            if (atomic_compare_exchange_weak_explicit(&base->_cfinfoa, &info, newInfo, memory_order_acq_rel, memory_order_acquire)) {
                return;
            }
            continue;
        }

        uint64_t deallocatingInfo = info | FX_CF_INFO_DEALLOCATING;
        if (atomic_compare_exchange_strong_explicit(&base->_cfinfoa, &info, deallocatingInfo, memory_order_acq_rel, memory_order_acquire)) {
            break;
        }
    }

    Boolean usesDefaultAllocator = (info & FX_CF_INFO_USES_DEFAULT_ALLOCATOR) != 0;
    unsigned char *raw = usesDefaultAllocator ? (unsigned char *)base : ((unsigned char *)base - FX_CF_ALLOCATOR_PREFIX_SIZE);
    CFAllocatorRef allocator = usesDefaultAllocator ? kCFAllocatorSystemDefault : *(CFAllocatorRef *)raw;

    if ((cls->version & _kCFRuntimeResourcefulObject) != 0 && cls->reclaim != NULL) {
        cls->reclaim(cf);
    }
    if (cls->finalize != NULL) {
        cls->finalize(cf);
    }

    (void)atomic_fetch_or_explicit(&base->_cfinfoa, FX_CF_INFO_DEALLOCATED, memory_order_acq_rel);

    CFAllocatorDeallocate(allocator, raw);
    if (!usesDefaultAllocator) {
        CFRelease((CFTypeRef)allocator);
    }
}

CFAllocatorRef CFGetAllocator(CFTypeRef cf) {
    return __FXCFGetAllocatorFromObject(cf);
}

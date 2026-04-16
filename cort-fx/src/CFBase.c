#include "FXCFInternal.h"

struct __CFAllocator {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFAllocatorContext _context;
};

static void *__CFAllocatorSystemAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    (void)info;
    if (size <= 0) {
        return NULL;
    }
    if (hint == FX_CF_ALLOCATOR_HINT_ZERO) {
        return calloc(1, (size_t)size);
    }
    return malloc((size_t)size);
}

static void *__CFAllocatorSystemReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    (void)hint;
    (void)info;
    if (newsize <= 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)newsize);
}

static void __CFAllocatorSystemDeallocate(void *ptr, void *info) {
    (void)info;
    free(ptr);
}

static void *__CFAllocatorMallocAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    (void)hint;
    (void)info;
    if (size <= 0) {
        return NULL;
    }
    return malloc((size_t)size);
}

static void *__CFAllocatorMallocReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    (void)hint;
    (void)info;
    if (newsize <= 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, (size_t)newsize);
}

static void __CFAllocatorMallocDeallocate(void *ptr, void *info) {
    (void)info;
    free(ptr);
}

static void *__CFAllocatorNullAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    (void)size;
    (void)hint;
    (void)info;
    return NULL;
}

static void *__CFAllocatorNullReallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) {
    (void)ptr;
    (void)newsize;
    (void)hint;
    (void)info;
    return NULL;
}

static void __CFAllocatorFinalize(CFTypeRef cf) {
    const struct __CFAllocator *allocator = (const struct __CFAllocator *)cf;
    CFAllocatorReleaseCallBack release = allocator->_context.release;
    if (release != NULL && allocator->_context.info != NULL) {
        release(allocator->_context.info);
    }
}

static const CFRuntimeClass __CFAllocatorClass = {
    0,
    "CFAllocator",
    NULL,
    NULL,
    __CFAllocatorFinalize,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0
};

const CFRuntimeClass *_FXCFAllocatorClass(void) {
    return &__CFAllocatorClass;
}

static struct __CFAllocator __kCFAllocatorSystemDefault = {
    {
        (uintptr_t)&__CFAllocatorClass,
        FX_CF_INFO_USES_DEFAULT_ALLOCATOR | FX_CF_INFO_IMMORTAL | ((uint64_t)_kCFRuntimeIDCFAllocator << FX_CF_INFO_TYPE_SHIFT)
    },
    NULL,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorSystemAllocate, __CFAllocatorSystemReallocate, __CFAllocatorSystemDeallocate, NULL}
};

static struct __CFAllocator __kCFAllocatorMalloc = {
    {
        (uintptr_t)&__CFAllocatorClass,
        FX_CF_INFO_USES_DEFAULT_ALLOCATOR | FX_CF_INFO_IMMORTAL | ((uint64_t)_kCFRuntimeIDCFAllocator << FX_CF_INFO_TYPE_SHIFT)
    },
    &__kCFAllocatorSystemDefault,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorMallocAllocate, __CFAllocatorMallocReallocate, __CFAllocatorMallocDeallocate, NULL}
};

static struct __CFAllocator __kCFAllocatorNull = {
    {
        (uintptr_t)&__CFAllocatorClass,
        FX_CF_INFO_USES_DEFAULT_ALLOCATOR | FX_CF_INFO_IMMORTAL | ((uint64_t)_kCFRuntimeIDCFAllocator << FX_CF_INFO_TYPE_SHIFT)
    },
    &__kCFAllocatorSystemDefault,
    {0, NULL, NULL, NULL, NULL, __CFAllocatorNullAllocate, __CFAllocatorNullReallocate, NULL, NULL}
};

const CFAllocatorRef kCFAllocatorDefault = NULL;
const CFAllocatorRef kCFAllocatorSystemDefault = &__kCFAllocatorSystemDefault;
const CFAllocatorRef kCFAllocatorMalloc = &__kCFAllocatorMalloc;
const CFAllocatorRef kCFAllocatorNull = &__kCFAllocatorNull;
const CFAllocatorRef kCFAllocatorUseContext = (CFAllocatorRef)(uintptr_t)0x03ab;

static _Atomic(uintptr_t) __FXCFDefaultAllocator = 0;

static CFAllocatorRef __FXCFGetDefaultAllocator(void) {
    uintptr_t stored = atomic_load_explicit(&__FXCFDefaultAllocator, memory_order_acquire);
    if (stored == 0) {
        return kCFAllocatorSystemDefault;
    }
    return (CFAllocatorRef)stored;
}

static const struct __CFAllocator *__FXCFAllocatorCast(CFAllocatorRef allocator) {
    if (allocator == NULL) {
        allocator = __FXCFGetDefaultAllocator();
    }
    if (allocator == kCFAllocatorUseContext) {
        _FXCFAbort("kCFAllocatorUseContext is not a usable allocator in Subset 0");
    }
    if (CFGetTypeID((CFTypeRef)allocator) != _kCFRuntimeIDCFAllocator) {
        _FXCFAbort("invalid CFAllocatorRef");
    }
    return (const struct __CFAllocator *)allocator;
}

CFRange __CFRangeMake(CFIndex loc, CFIndex len) {
    return CFRangeMake(loc, len);
}

CFTypeID CFAllocatorGetTypeID(void) {
    return _kCFRuntimeIDCFAllocator;
}

CFAllocatorRef CFAllocatorGetDefault(void) {
    return __FXCFGetDefaultAllocator();
}

void CFAllocatorSetDefault(CFAllocatorRef allocator) {
    if (allocator == NULL) {
        allocator = kCFAllocatorSystemDefault;
    }
    (void)__FXCFAllocatorCast(allocator);
    CFRetain((CFTypeRef)allocator);
    uintptr_t old = atomic_exchange_explicit(&__FXCFDefaultAllocator, (uintptr_t)allocator, memory_order_acq_rel);
    if (old != 0) {
        CFRelease((CFTypeRef)old);
    }
}

CFAllocatorRef CFAllocatorCreate(CFAllocatorRef allocator, CFAllocatorContext *context) {
    if (context == NULL || context->version != 0) {
        return NULL;
    }
    if (allocator == kCFAllocatorUseContext) {
        return NULL;
    }

    CFAllocatorRef realAllocator = allocator == NULL ? __FXCFGetDefaultAllocator() : allocator;
    if (realAllocator == kCFAllocatorNull) {
        return NULL;
    }

    CFIndex extraBytes = (CFIndex)(sizeof(struct __CFAllocator) - sizeof(CFRuntimeBase));
    struct __CFAllocator *memory = (struct __CFAllocator *)_CFRuntimeCreateInstance(realAllocator, _kCFRuntimeIDCFAllocator, extraBytes, NULL);
    if (memory == NULL) {
        return NULL;
    }

    CFAllocatorRetainCallBack retain = context->retain;
    void *retainedInfo = context->info;
    if (retain != NULL && retainedInfo != NULL) {
        retainedInfo = (void *)retain(retainedInfo);
    }

    memory->_allocator = realAllocator;
    memory->_context.version = context->version;
    memory->_context.info = retainedInfo;
    memory->_context.retain = retain;
    memory->_context.release = context->release;
    memory->_context.copyDescription = context->copyDescription;
    memory->_context.allocate = context->allocate;
    memory->_context.reallocate = context->reallocate;
    memory->_context.deallocate = context->deallocate;
    memory->_context.preferredSize = context->preferredSize;

    return memory;
}

void *CFAllocatorAllocate(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint) {
    const struct __CFAllocator *self = __FXCFAllocatorCast(allocator);
    CFAllocatorAllocateCallBack allocate = self->_context.allocate;
    if (allocate == NULL || size <= 0) {
        return NULL;
    }
    return allocate(size, hint, self->_context.info);
}

void *CFAllocatorReallocate(CFAllocatorRef allocator, void *ptr, CFIndex newsize, CFOptionFlags hint) {
    const struct __CFAllocator *self = __FXCFAllocatorCast(allocator);

    if (ptr == NULL) {
        return CFAllocatorAllocate((CFAllocatorRef)self, newsize, hint);
    }
    if (newsize <= 0) {
        CFAllocatorDeallocate((CFAllocatorRef)self, ptr);
        return NULL;
    }

    CFAllocatorReallocateCallBack reallocate = self->_context.reallocate;
    if (reallocate != NULL) {
        return reallocate(ptr, newsize, hint, self->_context.info);
    }

    return NULL;
}

void CFAllocatorDeallocate(CFAllocatorRef allocator, void *ptr) {
    if (ptr == NULL) {
        return;
    }
    const struct __CFAllocator *self = __FXCFAllocatorCast(allocator);
    CFAllocatorDeallocateCallBack deallocate = self->_context.deallocate;
    if (deallocate != NULL) {
        deallocate(ptr, self->_context.info);
    }
}

CFIndex CFAllocatorGetPreferredSizeForSize(CFAllocatorRef allocator, CFIndex size, CFOptionFlags hint) {
    const struct __CFAllocator *self = __FXCFAllocatorCast(allocator);
    if (size <= 0) {
        return 0;
    }
    CFAllocatorPreferredSizeCallBack preferred = self->_context.preferredSize;
    if (preferred == NULL) {
        return size;
    }
    CFIndex result = preferred(size, hint, self->_context.info);
    return result < size ? size : result;
}

void CFAllocatorGetContext(CFAllocatorRef allocator, CFAllocatorContext *context) {
    if (context == NULL) {
        return;
    }
    const struct __CFAllocator *self = __FXCFAllocatorCast(allocator);
    *context = self->_context;
}

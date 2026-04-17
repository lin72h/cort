#ifndef FX_CF_CONTROL_PACKET_H
#define FX_CF_CONTROL_PACKET_H

#include <CoreFoundation/CFData.h>

#include "FXCFInternal.h"

enum _FXCFControlPacketKind {
    _FXCFControlPacketKindRequest = 1,
    _FXCFControlPacketKindResponse = 2
};

enum _FXCFControlPacketErrorKind {
    _FXCFControlPacketErrorNone = 0,
    _FXCFControlPacketErrorMissingKey = 1,
    _FXCFControlPacketErrorUnsupportedVersion = 2,
    _FXCFControlPacketErrorInvalidPacket = 3
};

struct _FXCFControlPacketError {
    enum _FXCFControlPacketErrorKind kind;
    CFIndex version;
    char text[192];
};

Boolean _FXCFControlPacketDecodeEnvelope(
    CFAllocatorRef allocator,
    CFDataRef payload,
    enum _FXCFControlPacketKind kind,
    CFDictionaryRef *packetOut,
    struct _FXCFControlPacketError *errorOut
);

char *_FXCFControlPacketCopyCanonicalJSON(CFTypeRef value);
char *_FXCFControlPacketCopyAcceptedSummary(CFDictionaryRef packet, enum _FXCFControlPacketKind kind);
char *_FXCFControlPacketCopyErrorJSON(const struct _FXCFControlPacketError *error);
char *_FXCFControlPacketCopyErrorSummary(enum _FXCFControlPacketKind kind, const struct _FXCFControlPacketError *error);

#endif

#ifndef FX_CF_CONTROL_PACKET_H
#define FX_CF_CONTROL_PACKET_H

#include <CoreFoundation/CFData.h>

#include "FXCFInternal.h"

enum _FXCFControlPacketKind {
    _FXCFControlPacketKindRequest = 1,
    _FXCFControlPacketKindResponse = 2
};

enum _FXCFControlResponseStatus {
    _FXCFControlResponseStatusNone = 0,
    _FXCFControlResponseStatusOK = 1,
    _FXCFControlResponseStatusError = 2
};

enum _FXCFControlValueKind {
    _FXCFControlValueKindInvalid = 0,
    _FXCFControlValueKindObject = 1,
    _FXCFControlValueKindArray = 2,
    _FXCFControlValueKindString = 3,
    _FXCFControlValueKindInteger = 4,
    _FXCFControlValueKindDouble = 5,
    _FXCFControlValueKindBool = 6,
    _FXCFControlValueKindDate = 7,
    _FXCFControlValueKindData = 8
};

enum _FXCFControlRequestRouteKind {
    _FXCFControlRequestRouteKindInvalid = 0,
    _FXCFControlRequestRouteKindControlCapabilities = 1,
    _FXCFControlRequestRouteKindControlHealth = 2,
    _FXCFControlRequestRouteKindDiagnosticsSnapshot = 3,
    _FXCFControlRequestRouteKindNotifyCancel = 4,
    _FXCFControlRequestRouteKindNotifyCheck = 5,
    _FXCFControlRequestRouteKindNotifyGetState = 6,
    _FXCFControlRequestRouteKindNotifyIsValidToken = 7,
    _FXCFControlRequestRouteKindNotifyListNames = 8,
    _FXCFControlRequestRouteKindNotifyPost = 9,
    _FXCFControlRequestRouteKindNotifyPrepareFileDescriptorDelivery = 10,
    _FXCFControlRequestRouteKindNotifyRegisterCheck = 11,
    _FXCFControlRequestRouteKindNotifyRegisterDispatch = 12,
    _FXCFControlRequestRouteKindNotifyRegisterFileDescriptor = 13,
    _FXCFControlRequestRouteKindNotifyRegisterSignal = 14,
    _FXCFControlRequestRouteKindNotifyResume = 15,
    _FXCFControlRequestRouteKindNotifySetState = 16,
    _FXCFControlRequestRouteKindNotifySuspend = 17
};

enum _FXCFControlResponseProfileKind {
    _FXCFControlResponseProfileKindInvalid = 0,
    _FXCFControlResponseProfileKindError = 1,
    _FXCFControlResponseProfileKindNotifyPost = 2,
    _FXCFControlResponseProfileKindNotifyRegistrationWrapper = 3,
    _FXCFControlResponseProfileKindNotifyCancel = 4,
    _FXCFControlResponseProfileKindNotifyRegistration = 5,
    _FXCFControlResponseProfileKindNotifyNameState = 6,
    _FXCFControlResponseProfileKindNotifyValidity = 7,
    _FXCFControlResponseProfileKindNotifyCheck = 8,
    _FXCFControlResponseProfileKindNotifyNameList = 9,
    _FXCFControlResponseProfileKindControlCapabilities = 10,
    _FXCFControlResponseProfileKindControlHealth = 11,
    _FXCFControlResponseProfileKindDiagnosticsSnapshot = 12,
    _FXCFControlResponseProfileKindGenericObject = 13
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

struct _FXCFControlRequest {
    CFDictionaryRef packet;
    CFIndex protocolVersion;
    CFStringRef service;
    CFStringRef method;
    CFDictionaryRef params;
};

struct _FXCFControlResponse {
    CFDictionaryRef packet;
    CFIndex protocolVersion;
    enum _FXCFControlResponseStatus status;
    CFTypeRef result;
    CFDictionaryRef errorPayload;
    CFStringRef errorCode;
    CFStringRef errorMessage;
};

struct _FXCFControlRequestRoute {
    struct _FXCFControlRequest request;
    enum _FXCFControlRequestRouteKind kind;
    CFStringRef name;
    CFStringRef scope;
    CFStringRef expectedSessionID;
    CFStringRef clientRegistrationID;
    CFStringRef queueName;
    Boolean hasToken;
    CFIndex token;
    Boolean hasSignal;
    CFIndex signal;
    Boolean hasTargetPID;
    CFIndex targetPID;
    Boolean hasState;
    CFIndex state;
    Boolean hasReuseExistingBinding;
    Boolean reuseExistingBinding;
};

struct _FXCFControlResponseProfile {
    struct _FXCFControlResponse response;
    enum _FXCFControlResponseProfileKind kind;
    CFDictionaryRef resultObject;
    CFArrayRef resultArray;
    CFDictionaryRef registration;
    CFDictionaryRef nameObject;
    CFDictionaryRef capabilities;
    CFDictionaryRef health;
    CFDictionaryRef daemon;
    CFArrayRef notifyNames;
    CFArrayRef deliveredTokens;
    CFArrayRef featureFlags;
    CFArrayRef reasons;
    Boolean hasGeneration;
    CFIndex generation;
    Boolean hasState;
    CFIndex state;
    Boolean hasCurrentState;
    CFIndex currentState;
    Boolean hasToken;
    CFIndex token;
    Boolean hasValid;
    Boolean valid;
    Boolean hasChanged;
    Boolean changed;
    Boolean hasCanceled;
    Boolean canceled;
    Boolean hasPendingGeneration;
    CFIndex pendingGeneration;
};

Boolean _FXCFControlPacketIsBinaryPlist(CFDataRef payload);

Boolean _FXCFControlPacketDecodeEnvelope(
    CFAllocatorRef allocator,
    CFDataRef payload,
    enum _FXCFControlPacketKind kind,
    CFDictionaryRef *packetOut,
    struct _FXCFControlPacketError *errorOut
);

Boolean _FXCFControlRequestInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlRequest *requestOut,
    struct _FXCFControlPacketError *errorOut
);

void _FXCFControlRequestClear(struct _FXCFControlRequest *request);

Boolean _FXCFControlResponseInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlResponse *responseOut,
    struct _FXCFControlPacketError *errorOut
);

void _FXCFControlResponseClear(struct _FXCFControlResponse *response);

enum _FXCFControlValueKind _FXCFControlPacketValueKind(CFTypeRef value);
Boolean _FXCFControlRequestRouteInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlRequestRoute *routeOut,
    struct _FXCFControlPacketError *errorOut
);
void _FXCFControlRequestRouteClear(struct _FXCFControlRequestRoute *route);
char *_FXCFControlRequestRouteCopyCanonicalJSON(const struct _FXCFControlRequestRoute *route);
char *_FXCFControlRequestRouteCopySummary(const struct _FXCFControlRequestRoute *route);
Boolean _FXCFControlResponseProfileInit(
    CFAllocatorRef allocator,
    CFDataRef payload,
    struct _FXCFControlResponseProfile *profileOut,
    struct _FXCFControlPacketError *errorOut
);
void _FXCFControlResponseProfileClear(struct _FXCFControlResponseProfile *profile);
char *_FXCFControlResponseProfileCopyCanonicalJSON(const struct _FXCFControlResponseProfile *profile);
char *_FXCFControlResponseProfileCopySummary(const struct _FXCFControlResponseProfile *profile);
char *_FXCFControlPacketCopyCanonicalJSON(CFTypeRef value);
char *_FXCFControlPacketCopyAcceptedSummary(CFDictionaryRef packet, enum _FXCFControlPacketKind kind);
char *_FXCFControlRequestCopyEnvelopeJSON(const struct _FXCFControlRequest *request);
char *_FXCFControlRequestCopyEnvelopeSummary(const struct _FXCFControlRequest *request);
char *_FXCFControlResponseCopyEnvelopeJSON(const struct _FXCFControlResponse *response);
char *_FXCFControlResponseCopyEnvelopeSummary(const struct _FXCFControlResponse *response);
char *_FXCFControlPacketCopyErrorJSON(const struct _FXCFControlPacketError *error);
char *_FXCFControlPacketCopyErrorSummary(enum _FXCFControlPacketKind kind, const struct _FXCFControlPacketError *error);

#endif

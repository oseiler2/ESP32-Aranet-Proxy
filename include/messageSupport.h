
#ifndef _MESSAGE_SUPPORT_H
#define _MESSAGE_SUPPORT_H

#include <ArduinoJson.h>

typedef void (*updateMessageCallback_t)(char const*);
typedef void (*setPriorityMessageCallback_t)(char const*);
typedef void (*clearPriorityMessageCallback_t)(void);
typedef void (*publishMessageCallback_t)(char const*);
typedef void (*publishMeasurement_t)(const char* name, DynamicJsonDocument* payload);
typedef void (*configChangedCallback_t)();

#endif
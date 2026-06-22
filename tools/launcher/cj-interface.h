#ifndef CJ_INTERFACE_H
#define CJ_INTERFACE_H

#include <stdint.h>

enum RTLogLevel {
    RTLOG_VERBOSE,
    RTLOG_DEBUGY,
    RTLOG_INFO,
    RTLOG_WARNING,
    RTLOG_ERROR,
    RTLOG_FATAL_WITHOUT_ABORT,
    RTLOG_FATAL,
    RTLOG_OFF
};

enum RTErrorCode {
    E_OK      = 0,
    E_ARGS    = -1,
    E_TIMEOUT = -2,
    E_STATE   = -3,
    E_FAILED  = -4
};

struct HeapParam {
    size_t regionSize;
    size_t heapSize;
    double exemptionThreshold;
    double heapUtilization;
    double heapGrowth;
    double allocationRate;
    size_t allocationWaitTime;
};

struct GCParam {
    size_t gcThreshold;
    double garbageThreshold;
    uint64_t gcInterval;
    uint64_t backupGCInterval;
    int32_t gcThreads;
};

struct LogParam {
    enum RTLogLevel logLevel;
};

struct ConcurrencyParam {
    size_t thStackSize;
    size_t coStackSize;
    uint32_t processorNum;
};

struct RuntimeParam {
    struct HeapParam heapParam;
    struct GCParam gcParam;
    struct LogParam logParam;
    struct ConcurrencyParam coParam;
};

/*
 * @struct InterpreterParam
 * @brief Data structure for interpreter configuration parameters.
 */
struct InterpreterParam {
    /* Interpreter dynamic library name. */
    const char* interpreterLibName;
    /* Number of startup arguments passed to interpreter. */
    int interpreterArgsCount;
    /* Startup argument list passed to interpreter. */
    const char** interpreterArgs;
    /* Optional app library handle used by interpreter to resolve application symbols. */
    void* appLibHandle;
};

#endif
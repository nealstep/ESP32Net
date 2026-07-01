// you need to copy this file to your project
// edit his file for messages includes from your libraries
#include "cirq_msgs.hpp"
#include "log_msgs.hpp"
#include "main_msgs.hpp"
#include "net_msgs.hpp"

#define UNIT_LIST(X)  \
    BASE_UNIT_LIST(X) \
    MAIN_UNIT_LIST(X) \
    CIRQ_UNIT_LIST(X) \
    NET_UNIT_LIST(X)  \
    X(Last, "Last+1")

#define ERROR_LIST(X)  \
    BASE_ERROR_LIST(X) \
    MAIN_ERROR_LIST(X) \
    CIRQ_ERROR_LIST(X) \
    NET_ERROR_LIST(X)

#define NOTICE_LIST(X)  \
    BASE_NOTICE_LIST(X) \
    MAIN_NOTICE_LIST(X) \
    CIRQ_NOTICE_LIST(X) \
    NET_NOTICE_LIST(X)

#define WORD_LIST(X)  \
    BASE_WORD_LIST(X) \
    MAIN_WORD_LIST(X) \
    CIRQ_WORD_LIST(X) \
    NET_WORD_LIST(X)

#define SEVERITY_LIST(X) \
    X(Dbg, "Debug")      \
    X(Inf, "Info")       \
    X(Wrn, "Warning")    \
    X(Err, "Error")      \
    X(All, "All")

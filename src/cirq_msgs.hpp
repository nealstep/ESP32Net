#pragma once

#ifdef USE_QUEUE

#define CIRQ_UNIT_LIST(X) X(CirQ, "CirQ")

#define CIRQ_ERROR_LIST(X)              \
    X(QueueEmpty, "Queue empty")        \
    X(NoQueue, "No queue")              \
    X(ItemTooBig, "Item too big")       \
    X(MallocFailed, "Malloc %S failed") \
    X(MallocFailedFreeing, "Malloc failed for buffer freeing entries")

#define CIRQ_NOTICE_LIST(X)                     \
    X(CirQCreate, "CQ::CircularQueue(%ld, %d)") \
    X(ComputedEntries, "Computed entries: %d")  \
    X(PSRAMFound, "PSRAM found")                \
    X(PSRAMCreated, "PSRAM %s created")         \
    X(MallocCreated, "Malloc %s created")       \
    X(FreeCapacity, "Free capacity: %u")        \
    X(FreeEntries, "Free entries: %u")          \
    X(CirQPush, "CQ Push: %d")                      \
    X(CirQRdyToCopy, "Ready to copy data")      \
    X(CirQSimpleCopy, "Simple copy")            \
    X(CirQBrokenCopy, "Broken copy")            \
    X(CirQPop, "CQ::pop: %d")

#define CIRQ_WORD_LIST(X) \
    X(Buffer, "Buffer")   \
    X(Records, "Records")

#endif  // USE_QUEUE

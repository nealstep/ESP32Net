#pragma once

#define MAIN_UNIT_LIST(X) X(Main, "Main")

#define MAIN_ERROR_LIST(X) X(Died, "Died")

#define MAIN_NOTICE_LIST(X)       \
    X(Starting, "Starting up")    \
    X(Started, "Started up")      \
    X(LoopedN, "looped %d times") \
    X(KeepAlive, "Keep alive")    \
    X(Done, "Done")

#define MAIN_WORD_LIST(X)

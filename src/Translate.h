#pragma once

// X() args must be plain identifiers (MSVC preprocessor quirk with `##`).
#define TR_KEYS(X)                                                            \
    X(MIDDLE_MOUSE,         "Middle Mouse")                                   \
    X(CONFLICT_PREFIX,      "Warning: bound to '")                            \
    X(CONFLICT_SUFFIX,      "'")                                              \
    X(ROTATE_KEY_PREFIX,    "Rotate key: ")                                   \
    X(PRESS_A_KEY,          "Press a key...")                                 \
    X(CAPTURE_HELP,         "Middle mouse or keyboard. Esc to cancel.")       \
    X(CHANGE,               "Change")                                         \
    X(RESET,                "Reset")                                          \
    X(ERR_EQUIPPED,         "Cannot rotate: item is equipped")                \
    X(ERR_SQUARE,           "Cannot rotate: item is square")                  \
    X(ERR_HOOKS_FAILED,     "Cannot rotate: save/load hooks failed")          \
    X(ERR_NO_SPACE,         "Cannot rotate: not enough space")                \
    X(NONE,                 "None")                                           \
    X(SECONDARY_KEY_PREFIX, "Secondary key: ")

enum TrKey {
#define X(n, s) TR_##n,
    TR_KEYS(X)
#undef X
    TR_COUNT
};

const char* Tr(TrKey key);
void DetectLanguage();

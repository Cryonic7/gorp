// trit.h — kernel-native ternary boolean (Kleene logic)
// Stored in 2 bits: 0=false, 1=unknown, 2=true (3=reserved)

#ifndef TRIT_H
#define TRIT_H

typedef enum { T_FALSE = 0, T_UNKNOWN = 1, T_TRUE = 2 } trit_t;

static inline trit_t trit_not(trit_t a) {
    switch (a) {
        case T_FALSE: return T_TRUE;
        case T_TRUE:  return T_FALSE;
        default:      return T_UNKNOWN;
    }
}

static inline trit_t trit_and(trit_t a, trit_t b) {
    if (a == T_FALSE || b == T_FALSE) return T_FALSE;
    if (a == T_TRUE && b == T_TRUE) return T_TRUE;
    return T_UNKNOWN;
}

static inline trit_t trit_or(trit_t a, trit_t b) {
    if (a == T_TRUE || b == T_TRUE) return T_TRUE;
    if (a == T_FALSE && b == T_FALSE) return T_FALSE;
    return T_UNKNOWN;
}

// Binary compat: unknown -> false (or trap if STRICT_TERNARY)
static inline int trit_to_bool(trit_t t) { return t == T_TRUE; }
static inline trit_t bool_to_trit(int b) { return b ? T_TRUE : T_FALSE; }

#endif

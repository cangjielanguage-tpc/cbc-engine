#ifndef ENGINE_H
#define ENGINE_H

typedef struct {
    void *(*get_trampoline)(void);
    void (*initialize)(void);
    void (*set_cbcpath)(char const *);
    void (*set_main_cbc)(char const *);
    void (*enable_dasm)(void);
    void (*enable_raw_dasm)(void);
} Engine;

#endif

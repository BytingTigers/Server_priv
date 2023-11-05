#define DEBUG // Delete this when you finish dev

#ifdef DEBUG
#define DEBUG_PRINT(...)                                                       \
    do {                                                                       \
        fprintf(stderr, "[%s:L%d] ", __FILE__, __LINE__);                      \
        fprintf(stderr, __VA_ARGS__);                                          \
    } while (0)
#else
#define DEBUG_PRINT(...)                                                       \
    do {                                                                       \
    } while (0)
#endif

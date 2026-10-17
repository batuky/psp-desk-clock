/* Tiny assert header: no dependencies, one translation unit per test binary. */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdint.h>
#include <stdio.h>

static int mt_checks;
static int mt_failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        mt_checks++;                                                         \
        if (!(cond)) {                                                       \
            mt_failures++;                                                   \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
        }                                                                    \
    } while (0)

#define CHECK_HEX(actual, expected)                                          \
    do {                                                                     \
        uint32_t mt_a = (uint32_t)(actual), mt_e = (uint32_t)(expected);     \
        mt_checks++;                                                         \
        if (mt_a != mt_e) {                                                  \
            mt_failures++;                                                   \
            printf("  FAIL %s:%d: %s = 0x%08X, expected 0x%08X\n", __FILE__, \
                   __LINE__, #actual, mt_a, mt_e);                           \
        }                                                                    \
    } while (0)

#define RUN(test)                                                            \
    do {                                                                     \
        int mt_before = mt_failures;                                         \
        test();                                                              \
        printf("%s %s\n", mt_failures == mt_before ? "ok  " : "FAIL", #test);\
    } while (0)

/* Prints the summary and returns the process exit code. */
static int mt_report(void) {
    printf("%d checks, %d failed\n", mt_checks, mt_failures);
    return mt_failures ? 1 : 0;
}

#endif

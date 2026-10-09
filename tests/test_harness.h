#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H
/**
 * @file   test_harness.h
 * @brief  Minimal assert-based test harness for host (PC) builds. Each test
 *         program includes this once, calls RUN_TEST for each test function,
 *         and returns TEST_RESULT() from main: 0 if every check passed.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdio.h>

static unsigned int g_test_checks   = 0U; // Checks evaluated so far
static unsigned int g_test_failures = 0U; // Checks that failed so far

// Records a check; on failure prints where it was and what was expected
#define TEST_ASSERT(condition)                                              \
    do                                                                      \
    {                                                                       \
        g_test_checks++;                                                    \
        if (!(condition))                                                   \
        {                                                                   \
            g_test_failures++;                                              \
            (void)printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                   \
    } while (0)

// Like TEST_ASSERT(expected == actual), but also prints both values on failure
#define TEST_ASSERT_EQUAL(expected, actual)                                 \
    do                                                                      \
    {                                                                       \
        const long test_expected_ = (long)(expected);                       \
        const long test_actual_   = (long)(actual);                         \
        g_test_checks++;                                                    \
        if (test_expected_ != test_actual_)                                 \
        {                                                                   \
            g_test_failures++;                                              \
            (void)printf("  FAIL %s:%d: %s: expected %ld, got %ld\n",       \
                         __FILE__, __LINE__, #actual, test_expected_, test_actual_); \
        }                                                                   \
    } while (0)

// Runs one test function and prints its name
#define RUN_TEST(test_function)                                             \
    do                                                                      \
    {                                                                       \
        (void)printf("%s\n", #test_function);                               \
        test_function();                                                    \
    } while (0)

// Prints the totals; evaluates to the process exit code (0 = all passed)
#define TEST_RESULT()                                                       \
    ((void)printf("%u checks, %u failed\n", g_test_checks, g_test_failures), \
     ((0U == g_test_failures) ? 0 : 1))

#endif /* TEST_HARNESS_H */

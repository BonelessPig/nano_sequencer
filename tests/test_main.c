/**
 * @file   test_main.c
 * @brief  Host tests for the firmware entry point (app/main.c). Its main() is
 *         renamed while it is included so this program can have its own, and
 *         the two app functions it calls are stubs defined here. The main loop
 *         never returns, so the app_run_once stub jumps back out to the test
 *         after a set number of passes.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <setjmp.h>
#include <stddef.h>

#define main firmware_main
#include "main.c" // The code under test, found via -Iapp
#undef main

#include "test_harness.h"

#define LOOP_ESCAPED (1) // What setjmp returns when the stub jumps out of the loop

static port_status_t g_init_status   = STATUS_OK; // What the app_init stub returns
static unsigned int  g_init_calls    = 0U;
static unsigned int  g_run_calls     = 0U;
static unsigned int  g_run_limit     = 0U; // Pass on which the stub leaves the loop
static unsigned int  g_inits_at_run  = 0U; // app_init calls seen by the first loop pass
static jmp_buf       g_escape;

port_status_t app_init(void)
{
    g_init_calls++;
    return g_init_status;
}



void app_run_once(void)
{
    if (0U == g_run_calls)
    {
        g_inits_at_run = g_init_calls;
    }
    g_run_calls++;
    if (g_run_calls >= g_run_limit)
    {
        longjmp(g_escape, LOOP_ESCAPED);
    }
}



/**
 * @brief Sets what app_init returns and forgets any earlier calls.
 */
static void start_stubs(port_status_t init_status, unsigned int run_limit)
{
    g_init_status  = init_status;
    g_init_calls   = 0U;
    g_run_calls    = 0U;
    g_run_limit    = run_limit;
    g_inits_at_run = 0U;
}



static void test_init_failure_returns_the_status_without_running(void)
{
    static const port_status_t failures[] =
    {
        ERR_GENERAL, ERR_INVALID_PARAM, ERR_BUSY, ERR_TIMEOUT, ERR_NOT_SUPPORTED, ERR_UNKNOWN
    };

    for (size_t i = 0U; i < (sizeof(failures) / sizeof(failures[0])); i++)
    {
        start_stubs(failures[i], 1U);

        if (0 == setjmp(g_escape))
        {
            TEST_ASSERT_EQUAL((int)failures[i], firmware_main());
        }
        TEST_ASSERT_EQUAL(1, g_init_calls);
        TEST_ASSERT_EQUAL(0, g_run_calls);
    }
}



static void test_init_success_runs_the_loop_until_stopped(void)
{
    static unsigned int returned = 0U; // static: must survive the longjmp

    start_stubs(STATUS_OK, 1000U);
    returned = 0U;

    if (0 == setjmp(g_escape))
    {
        (void)firmware_main();
        returned++; // Only reached if main came back by itself
    }
    TEST_ASSERT_EQUAL(0, returned);
    TEST_ASSERT_EQUAL(1000, g_run_calls);
}



static void test_init_runs_once_before_the_first_pass(void)
{
    start_stubs(STATUS_OK, 3U);

    if (0 == setjmp(g_escape))
    {
        (void)firmware_main();
    }
    TEST_ASSERT_EQUAL(1, g_inits_at_run);
    TEST_ASSERT_EQUAL(1, g_init_calls); // And never again while looping
}



int main(void)
{
    RUN_TEST(test_init_failure_returns_the_status_without_running);
    RUN_TEST(test_init_success_runs_the_loop_until_stopped);
    RUN_TEST(test_init_runs_once_before_the_first_pass);
    return TEST_RESULT();
}

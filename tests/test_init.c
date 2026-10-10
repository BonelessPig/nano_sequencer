/**
 * @file   test_init.c
 * @brief  Host tests for the target platform bring-up (adapters/target/init.c).
 *         The three functions it calls are replaced by stubs defined here, so
 *         the order of the calls and the handling of each failure can be checked.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "fake_atmega328p_regs.h" // Must come before the adapter source
#include "init.c"                 // The code under test, found via -Iadapters/target
#include "test_harness.h"

#define NOT_CALLED (0U)

static port_status_t g_serial_status   = STATUS_OK; // What the logger_init stub returns
static port_status_t g_register_status = STATUS_OK; // What the register_init stub returns
static log_level_t   g_serial_level    = LOGLVL_OFF; // Level logger_init was last given
static unsigned int  g_call_count      = 0U;         // Stub calls so far
static unsigned int  g_serial_call     = NOT_CALLED; // Position of the logger_init call, from 1
static unsigned int  g_register_call   = NOT_CALLED; // Position of the register_init call, from 1
static unsigned int  g_timebase_call   = NOT_CALLED; // Position of the timebase_init call, from 1

port_status_t logger_init(log_level_t level)
{
    g_call_count++;
    g_serial_call  = g_call_count;
    g_serial_level = level;
    return g_serial_status;
}



port_status_t register_init(void)
{
    g_call_count++;
    g_register_call = g_call_count;
    return g_register_status;
}



void timebase_init(void)
{
    g_call_count++;
    g_timebase_call = g_call_count;
}



/**
 * @brief Sets what the two stubs that can fail return and forgets any earlier calls.
 */
static void start_stubs(port_status_t serial_status, port_status_t register_status)
{
    g_serial_status   = serial_status;
    g_register_status = register_status;
    g_serial_level    = LOGLVL_OFF;
    g_call_count      = 0U;
    g_serial_call     = NOT_CALLED;
    g_register_call   = NOT_CALLED;
    g_timebase_call   = NOT_CALLED;
}



static void test_success_brings_up_serial_then_registers_then_timebase(void)
{
    start_stubs(STATUS_OK, STATUS_OK);

    // The timebase is last: interrupts are on once it has started
    TEST_ASSERT_EQUAL(STATUS_OK, platform_init());
    TEST_ASSERT_EQUAL(1, g_serial_call);
    TEST_ASSERT_EQUAL(2, g_register_call);
    TEST_ASSERT_EQUAL(3, g_timebase_call);
    TEST_ASSERT_EQUAL(3, g_call_count);
}



static void test_serial_is_started_at_debug_level(void)
{
    start_stubs(STATUS_OK, STATUS_OK);
    (void)platform_init();

    TEST_ASSERT_EQUAL(LOGLVL_DEBUG, g_serial_level);
}



static void test_serial_failure_is_returned_and_stops_the_bring_up(void)
{
    start_stubs(ERR_TIMEOUT, STATUS_OK);

    TEST_ASSERT_EQUAL(ERR_TIMEOUT, platform_init());
    TEST_ASSERT_EQUAL(1, g_serial_call);
    TEST_ASSERT_EQUAL(NOT_CALLED, g_register_call);
    TEST_ASSERT_EQUAL(NOT_CALLED, g_timebase_call); // Interrupts stay off
}



static void test_register_failure_is_returned(void)
{
    start_stubs(STATUS_OK, ERR_NOT_SUPPORTED);

    TEST_ASSERT_EQUAL(ERR_NOT_SUPPORTED, platform_init());
    TEST_ASSERT_EQUAL(2, g_call_count);
    TEST_ASSERT_EQUAL(NOT_CALLED, g_timebase_call); // Interrupts stay off
}



int main(void)
{
    RUN_TEST(test_success_brings_up_serial_then_registers_then_timebase);
    RUN_TEST(test_serial_is_started_at_debug_level);
    RUN_TEST(test_serial_failure_is_returned_and_stops_the_bring_up);
    RUN_TEST(test_register_failure_is_returned);
    return TEST_RESULT();
}

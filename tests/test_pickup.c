/**
 * @file   test_pickup.c
 * @brief  Host unit tests for the pickup (core/pickup.c).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <string.h>
#include "pickup.h"
#include "test_harness.h"

#define POISON_BYTE  (0xA5)
#define CATCH_WINDOW (8U) // How near a reading has to come, as pickup.h says



static void test_init_detaches_and_forgets_the_last_reading(void)
{
    pickup_state_t state;

    (void)memset(&state, POISON_BYTE, sizeof(state));
    pickup_init(&state);
    TEST_ASSERT(!state.b_attached);
    TEST_ASSERT(!state.b_known);
    TEST_ASSERT_EQUAL(0, state.last);
}



static void test_a_detached_pot_leaves_the_value_alone(void)
{
    pickup_state_t state;
    uint16_t       value = 500U;

    // Moving about on one side of the value never meets it
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 100U, &value));
    TEST_ASSERT(!pickup_update(&state, 300U, &value));
    TEST_ASSERT(!pickup_update(&state, 50U, &value));
    TEST_ASSERT_EQUAL(500, value);

    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 900U, &value));
    TEST_ASSERT(!pickup_update(&state, 700U, &value));
    TEST_ASSERT(!pickup_update(&state, 1023U, &value));
    TEST_ASSERT_EQUAL(500, value);
}



static void test_a_reading_near_the_value_attaches(void)
{
    pickup_state_t state;
    uint16_t       value = 500U;

    // One outside the window does not, from either side
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 500U - CATCH_WINDOW - 1U, &value));
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 500U + CATCH_WINDOW + 1U, &value));
    TEST_ASSERT_EQUAL(500, value);

    // The edge of the window does, and the value is then the reading. The
    // first reading of all will do
    pickup_init(&state);
    TEST_ASSERT(pickup_update(&state, 500U - CATCH_WINDOW, &value));
    TEST_ASSERT_EQUAL(500U - CATCH_WINDOW, value);

    value = 500U;
    pickup_init(&state);
    TEST_ASSERT(pickup_update(&state, 500U + CATCH_WINDOW, &value));
    TEST_ASSERT_EQUAL(500U + CATCH_WINDOW, value);
}



static void test_passing_the_value_between_two_readings_attaches(void)
{
    pickup_state_t state;
    uint16_t       value = 500U;

    // Upwards
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 100U, &value));
    TEST_ASSERT(pickup_update(&state, 900U, &value));
    TEST_ASSERT_EQUAL(900, value);

    // Downwards
    value = 500U;
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 900U, &value));
    TEST_ASSERT(pickup_update(&state, 100U, &value));
    TEST_ASSERT_EQUAL(100, value);
}



static void test_a_first_reading_has_passed_nothing(void)
{
    pickup_state_t state;
    uint16_t       value = 500U;

    // The last reading is 0 after init, and is not one
    pickup_init(&state);
    TEST_ASSERT(!pickup_update(&state, 900U, &value));
    TEST_ASSERT_EQUAL(500, value);
    TEST_ASSERT(state.b_known);
    TEST_ASSERT_EQUAL(900, state.last);
}



static void test_an_attached_pot_is_the_value(void)
{
    pickup_state_t state;
    uint16_t       value = 500U;

    pickup_init(&state);
    TEST_ASSERT(pickup_update(&state, 500U, &value));
    TEST_ASSERT(pickup_update(&state, 0U, &value));
    TEST_ASSERT_EQUAL(0, value);
    TEST_ASSERT(pickup_update(&state, 1023U, &value));
    TEST_ASSERT_EQUAL(1023, value);
    TEST_ASSERT(pickup_update(&state, 65535U, &value));
    TEST_ASSERT_EQUAL(65535, value);
}



static void test_release_detaches_and_keeps_the_last_reading(void)
{
    pickup_state_t state;
    uint16_t       first  = 500U;
    uint16_t       second = 200U;

    pickup_init(&state);
    (void)pickup_update(&state, 500U, &first);
    pickup_release(&state);
    TEST_ASSERT(!state.b_attached);
    TEST_ASSERT(state.b_known);
    TEST_ASSERT_EQUAL(500, state.last);

    // Given another value, the pot has to meet that one. The reading from
    // before the release still counts: 500 down to 100 has passed 200
    TEST_ASSERT(pickup_update(&state, 100U, &second));
    TEST_ASSERT_EQUAL(100, second);
    TEST_ASSERT_EQUAL(500, first);
}



static void test_a_pot_that_was_on_the_value_has_met_it(void)
{
    pickup_state_t state;
    uint16_t       first  = 500U;
    uint16_t       second = 500U;

    // Left on 500 by the value before, and then moved in one go
    pickup_init(&state);
    (void)pickup_update(&state, 500U, &first);
    pickup_release(&state);
    TEST_ASSERT(pickup_update(&state, 900U, &second));
    TEST_ASSERT_EQUAL(900, second);

    second = 900U;
    pickup_release(&state);
    TEST_ASSERT(pickup_update(&state, 100U, &second));
    TEST_ASSERT_EQUAL(100, second);
}



static void test_null_pointers_are_ignored(void)
{
    pickup_state_t state;
    pickup_state_t untouched;
    uint16_t       value = 500U;

    pickup_init(NULL);    // Must not crash
    pickup_release(NULL);

    pickup_init(&state);
    untouched = state;
    TEST_ASSERT(!pickup_update(NULL, 500U, &value));
    TEST_ASSERT(!pickup_update(&state, 500U, NULL));
    TEST_ASSERT_EQUAL(500, value);
    TEST_ASSERT_EQUAL(0, memcmp(&untouched, &state, sizeof(state)));
}



int main(void)
{
    RUN_TEST(test_init_detaches_and_forgets_the_last_reading);
    RUN_TEST(test_a_detached_pot_leaves_the_value_alone);
    RUN_TEST(test_a_reading_near_the_value_attaches);
    RUN_TEST(test_passing_the_value_between_two_readings_attaches);
    RUN_TEST(test_a_first_reading_has_passed_nothing);
    RUN_TEST(test_an_attached_pot_is_the_value);
    RUN_TEST(test_release_detaches_and_keeps_the_last_reading);
    RUN_TEST(test_a_pot_that_was_on_the_value_has_met_it);
    RUN_TEST(test_null_pointers_are_ignored);
    return TEST_RESULT();
}

#include "button_edge.h"
#include "minitest.h"

/* Feeds a held/not-held history to a ButtonEdge and returns how many presses
 * it reported. */
static int presses_in(const int *down, int polls) {
    ButtonEdge edge = {0};
    int presses = 0;
    for (int i = 0; i < polls; i++) presses += button_edge_pressed(&edge, down[i]);
    return presses;
}

static void a_button_never_pressed_reports_nothing(void) {
    static const int down[] = {0, 0, 0, 0};
    CHECK(presses_in(down, 4) == 0);
}

static void a_press_is_reported_on_the_poll_it_goes_down(void) {
    ButtonEdge edge = {0};
    CHECK(button_edge_pressed(&edge, 0) == 0);
    CHECK(button_edge_pressed(&edge, 1) == 1);
}

static void holding_the_button_reports_one_press(void) {
    static const int down[] = {0, 1, 1, 1, 1, 1, 1, 1};
    CHECK(presses_in(down, 8) == 1);
}

static void releasing_and_pressing_again_is_a_new_press(void) {
    static const int down[] = {1, 1, 0, 0, 1, 1, 0, 1};
    CHECK(presses_in(down, 8) == 3);
}

int main(void) {
    RUN(a_button_never_pressed_reports_nothing);
    RUN(a_press_is_reported_on_the_poll_it_goes_down);
    RUN(holding_the_button_reports_one_press);
    RUN(releasing_and_pressing_again_is_a_new_press);
    return mt_report();
}

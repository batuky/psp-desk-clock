/* Turns "is the button down right now" into "was it just pressed", so one
 * long press counts once however many polls it spans. */
#ifndef BUTTON_EDGE_H
#define BUTTON_EDGE_H

typedef struct {
    int was_down; /* the button's state at the previous poll */
} ButtonEdge;

/* Feed it the button's state at each poll. Returns 1 only on the poll where
 * the button goes from up to down, else 0. Start from a zeroed ButtonEdge. */
static inline int button_edge_pressed(ButtonEdge *edge, int down) {
    int pressed = down && !edge->was_down;
    edge->was_down = down != 0;
    return pressed;
}

#endif

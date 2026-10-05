/**
 * @file apps/calc/main.c
 * @brief A graphical calculator, in ring 3, drawn with microui.
 *
 * The UI is the vendored microui toolkit -- the same one the kernel's own
 * desktop is built from -- running in a userspace process over the
 * full-screen grab; ../microui/mui.h describes that runtime and this file
 * uses nothing else privileged. Everything here is the calculator itself: the
 * keypad, the arithmetic and the display.
 *
 * The arithmetic is an immediate-execution four-function machine, the kind a
 * pocket calculator does rather than the kind a parser does: one accumulator,
 * at most one pending operator, no precedence. "2 + 3 * 4" is 20 here, as it
 * is on the thing in the desk drawer, not 14. Typing an operator settles the
 * one already pending, which is what lets an arbitrarily long chain of
 * operations run with no stack at all.
 *
 * Mouse and keyboard both drive it: 0-9 and '.' type, + - * / operate, Enter
 * or '=' applies, Backspace rubs out a digit, 'c' clears, 'n' negates, and
 * Esc gives the screen back to the desktop.
 */
#include <printf.h>

#include "mui.h"

/* ------------------------------------------------------------------ *
 *  The machine                                                        *
 * ------------------------------------------------------------------ */

/** Most digits we let someone type: what a display line holds comfortably. */
#define ENTRY_MAX 18

/** Calculator state: one accumulator, one pending operator, one entry. */
typedef struct {
    char entry[ENTRY_MAX + 2]; /**< Digits typed so far, "" while showing a result. */
    double value;              /**< What the display stands for when @c entry is empty. */
    double acc;                /**< Left-hand side of the pending operation. */
    char op;                   /**< Pending operator, 0 for none. */
    int error;                 /**< Set by a division by zero; only C clears it. */
    char shown[48];            /**< The display line, rebuilt every frame. */
    char pending[48];          /**< The "12 +" reminder above it. */
} calc;

/**
 * @brief The number the display currently stands for.
 *
 * The entry is only ever built by @ref type_digit and @ref type_sign, so it
 * is always a well-formed decimal -- optional '-', digits, at most one '.'.
 * That makes this a scan rather than a parse: there is no input it has to
 * reject, which is why there is no strtod in this program.
 */
static double current(const calc *c) {
    if (!c->entry[0])
        return c->value;
    const char *p = c->entry;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    double whole = 0, frac = 0, scale = 1;
    for (; *p && *p != '.'; p++)
        whole = whole * 10 + (*p - '0');
    if (*p == '.')
        for (p++; *p; p++) {
            scale *= 10;
            frac += (*p - '0') / scale;
        }
    double v = whole + frac;
    return neg ? -v : v;
}

/** @brief Length of the entry. */
static int entry_len(const calc *c) {
    int n = 0;
    while (c->entry[n])
        n++;
    return n;
}

/** @brief Stop typing and show @p v instead. */
static void set_value(calc *c, double v) {
    c->entry[0] = 0;
    c->value = v;
}

/** @brief Append one keypad digit, or the decimal point, to the entry. */
static void type_digit(calc *c, char d) {
    if (c->error)
        return;
    int len = entry_len(c);
    if (d == '.') {
        for (int i = 0; i < len; i++)
            if (c->entry[i] == '.')
                return;                  /* at most one point per number */
        if (len == 0) {                  /* ".5" reads better as "0.5" */
            c->entry[len++] = '0';
            c->entry[len] = 0;
        }
    } else if (len == 1 && c->entry[0] == '0') {
        c->entry[0] = d;                 /* no leading zeros */
        return;
    }
    if (len >= ENTRY_MAX)
        return;
    c->entry[len] = d;
    c->entry[len + 1] = 0;
}

/** @brief Rub out the last character typed; a displayed result clears to 0. */
static void type_back(calc *c) {
    if (c->error)
        return;
    if (!c->entry[0]) {
        set_value(c, 0);
        return;
    }
    int len = entry_len(c);
    c->entry[len - 1] = 0;
    if (len == 1 || (len == 2 && c->entry[0] == '-'))
        c->entry[0] = 0;                 /* back to showing @c value (0) */
}

/**
 * @brief Apply the pending operator to @p rhs. @return 0 on divide by zero.
 *
 * Every completed operation is echoed to the console. That console is behind
 * the grabbed screen while this program runs, so it is not something the user
 * reads now -- it goes to the serial log, which is how the port is checked
 * without sitting in front of a display (test/microui-boot.sh).
 */
static int apply(calc *c, double rhs) {
    char lhs[32];
    sprintf(lhs, "%.12g", c->acc);
    switch (c->op) {
    case '+': c->acc += rhs; break;
    case '-': c->acc -= rhs; break;
    case '*': c->acc *= rhs; break;
    case '/':
        if (rhs == 0) {
            printf("calc: %s / 0 -- divide by zero\n", lhs);
            return 0;
        }
        c->acc /= rhs;
        break;
    default:
        c->acc = rhs;                     /* nothing pending: just take it */
        return 1;
    }
    printf("calc: %s %c %.12g = %.12g\n", lhs, c->op, rhs, c->acc);
    return 1;
}

/**
 * @brief Enter an operator: settle whatever was pending, then hold @p op.
 *
 * Each operator both closes the previous operation and opens the next, which
 * is all immediate execution is -- the running total lives in @c acc and
 * "1 + 2 + 3" lands on 6 without anything remembering more than one operator.
 */
static void type_op(calc *c, char op) {
    if (c->error)
        return;
    if (!apply(c, current(c))) {
        c->error = 1;
        return;
    }
    set_value(c, c->acc);
    c->op = op;
}

/** @brief Finish the pending operation and show the result. */
static void type_equals(calc *c) {
    if (c->error)
        return;
    if (!apply(c, current(c))) {
        c->error = 1;
        return;
    }
    set_value(c, c->acc);
    c->op = 0;
}

/** @brief Clear everything. The only way out of an error. */
static void type_clear(calc *c) {
    c->entry[0] = 0;
    c->value = 0;
    c->acc = 0;
    c->op = 0;
    c->error = 0;
}

/** @brief Flip the sign of whatever is on the display. */
static void type_sign(calc *c) {
    if (c->error)
        return;
    if (!c->entry[0]) {
        set_value(c, -c->value);
        return;
    }
    if (c->entry[0] == '-') {
        int i = 0;
        for (; c->entry[i + 1]; i++)
            c->entry[i] = c->entry[i + 1];
        c->entry[i] = 0;
    } else {
        int len = entry_len(c);
        if (len > ENTRY_MAX)
            return;
        for (int i = len; i >= 0; i--)
            c->entry[i + 1] = c->entry[i];
        c->entry[0] = '-';
    }
}

/**
 * @brief Percent: read the display as a percentage, i.e. divide it by 100.
 *
 * The postfix reading a pocket calculator uses, not the "percent of the
 * accumulator" one some desktop calculators do for + and -. This version
 * means the same thing in every context, which is the only one that can be
 * explained in a line -- and the only one that is not a surprise.
 */
static void type_percent(calc *c) {
    if (!c->error)
        set_value(c, current(c) / 100.0);
}

/* ------------------------------------------------------------------ *
 *  The display                                                        *
 * ------------------------------------------------------------------ */

/** @brief Rebuild the two display lines for this frame. */
static void format(calc *c) {
    if (c->error)
        sprintf(c->shown, "divide by zero");
    else if (c->entry[0])
        sprintf(c->shown, "%s", c->entry);
    else
        /*
         * %.12g: enough significant digits that a chain of operations keeps
         * its precision, few enough that 0.1 + 0.2 prints as 0.3 rather than
         * as the binary number it really is.
         */
        sprintf(c->shown, "%.12g", c->value);

    if (c->op) {
        char lhs[32];
        sprintf(lhs, "%.12g", c->acc);
        sprintf(c->pending, "%s %c", lhs, c->op);
    } else {
        c->pending[0] = 0;
    }
}

/** @brief Paint the display panel: the pending operation, then the number. */
static void draw_display(const mui_Surface *s, mu_Rect r, void *udata) {
    const calc *c = (const calc *) udata;

    mui_fill(s, r, mu_color(16, 18, 22, 255));
    mui_frame(s, r, mu_color(70, 75, 86, 255));

    if (c->pending[0])
        mui_string(s, r.x + r.w - 8 - mui_text_width(c->pending, -1), r.y + 6,
                   c->pending, mu_color(118, 124, 138, 255));

    mui_string(s, r.x + r.w - 8 - mui_text_width(c->shown, -1),
               r.y + r.h - 8 - mui_text_height(), c->shown,
               c->error ? mu_color(232, 112, 100, 255)
                        : mu_color(236, 239, 245, 255));
}

/* ------------------------------------------------------------------ *
 *  The keypad                                                         *
 * ------------------------------------------------------------------ */

/** What one key does. */
typedef enum {
    K_DIGIT, K_OP, K_EQUALS, K_CLEAR, K_BACK, K_SIGN, K_PERCENT
} kind;

typedef struct { const char *label; kind what; char ch; } key;

/*
 * Four across, five down, the arrangement every pocket calculator uses: the
 * operators down the right-hand column, the digits in a reverse-telephone
 * block, and the two destructive keys up in the top row rather than next to
 * the digits they would otherwise be mis-hit for.
 */
static const key g_keypad[5][4] = {
    {{"C",   K_CLEAR, 0},   {"<-", K_BACK,  0},  {"%", K_PERCENT, 0}, {"/", K_OP, '/'}},
    {{"7",   K_DIGIT, '7'}, {"8", K_DIGIT, '8'}, {"9", K_DIGIT, '9'}, {"*", K_OP, '*'}},
    {{"4",   K_DIGIT, '4'}, {"5", K_DIGIT, '5'}, {"6", K_DIGIT, '6'}, {"-", K_OP, '-'}},
    {{"1",   K_DIGIT, '1'}, {"2", K_DIGIT, '2'}, {"3", K_DIGIT, '3'}, {"+", K_OP, '+'}},
    {{"+/-", K_SIGN,  0},   {"0", K_DIGIT, '0'}, {".", K_DIGIT, '.'}, {"=", K_EQUALS, 0}}
};

/** @brief Do what a key stands for. */
static void press(calc *c, const key *k) {
    switch (k->what) {
    case K_DIGIT:   type_digit(c, k->ch); break;
    case K_OP:      type_op(c, k->ch);    break;
    case K_EQUALS:  type_equals(c);       break;
    case K_CLEAR:   type_clear(c);        break;
    case K_BACK:    type_back(c);         break;
    case K_SIGN:    type_sign(c);         break;
    case K_PERCENT: type_percent(c);      break;
    }
}

/**
 * @brief Feed this frame's typing to the machine.
 *
 * microui gathers characters into @c ctx->input_text and key edges into
 * @c ctx->key_pressed, and clears both in mu_end() -- the same place a
 * textbox widget reads them, which is what makes a keystroke and a click on
 * the matching button indistinguishable to everything below this function.
 */
static void keyboard(mu_Context *ctx, calc *c) {
    for (const char *p = ctx->input_text; *p; p++) {
        char ch = *p;
        if ((ch >= '0' && ch <= '9') || ch == '.')
            type_digit(c, ch);
        else if (ch == '+' || ch == '-' || ch == '*' || ch == '/')
            type_op(c, ch);
        else if (ch == '=')
            type_equals(c);
        else if (ch == '%')
            type_percent(c);
        else if (ch == 'c' || ch == 'C')
            type_clear(c);
        else if (ch == 'n' || ch == 'N')
            type_sign(c);
    }
    if (ctx->key_pressed & MU_KEY_RETURN)
        type_equals(c);
    if (ctx->key_pressed & MU_KEY_BACKSPACE)
        type_back(c);
}

/* ------------------------------------------------------------------ *
 *  The window                                                         *
 * ------------------------------------------------------------------ */

/*
 * The content size: display, five keypad rows and one hint line, plus
 * microui's padding and row spacing. The title bar is not in it -- whoever
 * owns the window frame draws that (see ../microui/mui.h).
 */
#define WIN_W 320
#define WIN_H 304

static void frame(mu_Context *ctx, void *udata) {
    calc *c = (calc *) udata;

    keyboard(ctx, c);
    format(c);

    mu_layout_row(ctx, 1, (int[]) { -1 }, 54);
    mui_draw_custom(ctx, mu_layout_next(ctx), draw_display, c);

    for (int row = 0; row < 5; row++) {
        mu_layout_row(ctx, 4, (int[]) { 71, 71, 71, -1 }, 40);
        for (int col = 0; col < 4; col++) {
            const key *k = &g_keypad[row][col];
            /* Every label here is distinct, and microui derives a widget's id
             * from its label, so no mu_push_id juggling is needed. */
            if (mu_button(ctx, k->label))
                press(c, k);
        }
    }

    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
    mu_label(ctx, "0-9 . + - * /  Enter =  Esc quits");
}

/** @brief Entry point. `calc -f` takes the whole screen instead of a window. */
int main(int argc, char **argv) {
    static calc c;

    for (int i = 1; i < argc; i++)
        if (argv[i] && argv[i][0] == '-' && argv[i][1] == 'f')
            mui_fullscreen(1);

    if (mui_run("Calculator", WIN_W, WIN_H, frame, &c) < 0) {
        printf("calc: no window and no screen -- is there a framebuffer, and "
               "is another program holding it?\n");
        return 1;
    }
    return 0;
}

/**
 * @file graphics.c
 * @brief The microui desktop: mouse/keyboard input feeding, the log window and
 *        cursor blit, run once per frame by the "draw_thread" kernel process.
 */
#include <graphics.h>

#include <video.h>
#include <mouse.h>
#include <kheap.h>
#include <rtc.h>


#include <microui.h>
#include <renderer.h>
#include <printf.h>
#include <lib/string.h>
#include <keyboard.h>

#include <commands.h>
#include <bmp.h>
#include <sb16.h>
#include <rand.h>
#include <wm.h>

short mouse_icon[] =  {
        1,0,0,0,0,0,0,0,0,0,0,
        1,1,0,0,0,0,0,0,0,0,0,
        1,2,1,0,0,0,0,0,0,0,0,
        1,2,2,1,0,0,0,0,0,0,0,
        1,2,2,2,1,0,0,0,0,0,0,
        1,2,2,2,2,1,0,0,0,0,0,
        1,2,2,2,2,2,1,0,0,0,0,
        1,2,2,2,2,2,2,1,0,0,0,
        1,2,2,2,2,2,2,2,1,0,0,
        1,2,2,2,2,2,1,1,1,1,0,
        1,2,2,2,2,2,2,1,0,0,0,
        1,2,2,1,2,2,2,1,0,0,0,
        1,2,1,0,1,2,2,2,1,0,0,
        1,1,0,0,0,1,2,2,2,1,0,
        0,0,0,0,0,0,1,2,2,1,0,
        0,0,0,0,0,0,0,1,1,0,0,
};

uint32_t mouse_color_mapping[] = {0, 0, 0xFFFFFFFF};

/** @brief Draw the built-in 11x16 mouse cursor from @c mouse_icon (unused path). */
void paint_mouse() {
    short* buf = mouse_icon;
    int mouse_x = get_mouse_info()->x;
    int mouse_y = get_mouse_info()->y;
    for (int i=0; i<16; i++) {
        for (int j=0; j<11; j++) {
            if (*buf) {
                uint32_t color = mouse_color_mapping[*buf];
                if (mouse_x + j >= 0 && mouse_x + j < 1280 &&
                    mouse_y + i >= 0 && mouse_y + i < 1024) {
                    draw_pixel(mouse_x + j, mouse_y + i, color);
                }
            }
            buf++;
        }
    }
}

/**
 * @brief Draw the mouse cursor, loading mouse.bmp once and caching it.
 *
 * The bitmap is staged on both disk images (see floppy.sh / hda.sh); try the
 * floppy first, then the hard disk. If neither is mounted the cursor is simply
 * not drawn — the load is attempted exactly once so a missing file cannot spam
 * the console every frame.
 */
void paint_mouse2() {
    static bmp_image_t *mouse_cursor;
    static int tried;

    if (!mouse_cursor && !tried) {
        tried = 1;
        mouse_cursor = bmp_image_from_file("/rd/mouse.bmp");
        if (!mouse_cursor)
            mouse_cursor = bmp_image_from_file("/fda/mouse.bmp");
        if (!mouse_cursor)
            mouse_cursor = bmp_image_from_file("/hda/mouse.bmp");
    }
    if (!mouse_cursor)
        return;

    int mouse_x = get_mouse_info()->x;
    int mouse_y = get_mouse_info()->y;

    draw_data_with_alfa((uint32_t *) mouse_cursor->data,
                        mouse_cursor->width, mouse_cursor->height,
                        mouse_x, mouse_y);
}

mu_Context ctx;

/** @brief microui text-width callback (delegates to the renderer). */
static int text_width(mu_Font font, const char *text, int len) {
    (void)font;
    if (len == -1) { len = strlen(text); }
    return r_get_text_width(text, len);
}

/** @brief microui text-height callback (delegates to the renderer). */
static int text_height(mu_Font font) {
    (void)font;
    return r_get_text_height();
}

static  char logbuf[64000];
static   int logbuf_updated = 0;

void write_log(char *text) {
    size_t used = strlen(logbuf);
    size_t add = strlen(text);
    if(used + add >= sizeof(logbuf)) {
        /* Log buffer full: drop the oldest half to make room. */
        size_t half = sizeof(logbuf) / 2;
        memmove(logbuf, logbuf + half, used - half + 1);
        used -= half;
        if(used + add >= sizeof(logbuf))
            return;
    }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

/* ------------------------------------------------------------------ *
 *  Ring-3 program windows                                             *
 * ------------------------------------------------------------------ */

/**
 * A draw command of the desktop's own, past the ones microui defines: paint
 * window manager slot @c slot into @c rect.
 *
 * It goes into microui's command list rather than being painted after the
 * frame so that a program's surface is composited *in z-order*, under whatever
 * is in front of it and clipped to its own window's body. Painted afterwards
 * it would sit on top of every kernel window, which is the difference between
 * a window manager and a program that happens to be drawing on the screen.
 */
#define MU_COMMAND_SURFACE MU_COMMAND_MAX

typedef struct {
    mu_BaseCommand base;
    mu_Rect rect;
    int slot;
} surface_command;

/** @brief Queue slot @p slot's surface to be painted into @p rect. */
static void draw_surface_cmd(mu_Rect rect, int slot) {
    /* The same clip dance mu_draw_icon does: emit a clip command when the
     * rect is only partly visible, and restore the unclipped one after. */
    int clipped = mu_check_clip(&ctx, rect);
    if (clipped == MU_CLIP_ALL)
        return;
    if (clipped == MU_CLIP_PART)
        mu_set_clip(&ctx, mu_get_clip_rect(&ctx));
    surface_command *cmd = (surface_command *)
        mu_push_command(&ctx, MU_COMMAND_SURFACE, sizeof(surface_command));
    cmd->rect = rect;
    cmd->slot = slot;
    if (clipped)
        mu_set_clip(&ctx, mu_rect(0, 0, 0x1000000, 0x1000000));
}

/**
 * @brief Give every live program window a microui window of its own.
 *
 * microui is already most of a window manager -- it drags, it resizes, it
 * keeps a z-order, it draws a title bar with a close box -- so a program's
 * window is an ordinary `mu_begin_window` whose body happens to be filled by
 * someone else's pixels. Sizing it to the surface plus one title bar makes
 * @c cnt->body exactly the surface, so the blit is 1:1 and needs no scaling.
 *
 * The close box is a *request*: microui clears @c cnt->open when it is
 * clicked, which this turns into a window event for the program and then
 * undoes. A program that takes the hint exits and its window goes with it; one
 * that ignores it keeps its window, which is what every other system does too.
 */
static void draw_program_windows(void) {
    int clicked_program = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t *win = wm_slot(i);
        if (!win)
            continue;

        /* The container is keyed on the owning pid, not the title: two
         * programs may be called the same thing, and a slot reused by a new
         * program should not inherit the old one's position. */
        mu_push_id(&ctx, &win->pid, sizeof win->pid);

        mu_Rect want = mu_rect(win->init_x, win->init_y,
                               win->w, win->h + ctx.style->title_height);
        if (mu_begin_window_ex(&ctx, win->title, want,
                               MU_OPT_NORESIZE | MU_OPT_NOSCROLL)) {
            mu_Container *cnt = mu_get_current_container(&ctx);
            mu_Rect body = cnt->body;

            draw_surface_cmd(body, i);
            wm_viewport(i, body.x, body.y);

            if (ctx.mouse_pressed && ctx.hover_root == cnt) {
                wm_set_focus(i);
                clicked_program = 1;
            }
            mu_end_window(&ctx);
        } else {
            /* Not open: the close box was clicked last frame. Tell the
             * program and put the window back until it acts on it. */
            wm_request_close(i);
            mu_Container *cnt = mu_get_container(&ctx, win->title);
            if (cnt)
                cnt->open = 1;
        }
        mu_pop_id(&ctx);
    }

    /* A click anywhere else -- a kernel window, the background -- hands the
     * keyboard back to the shell. */
    if (ctx.mouse_pressed && !clicked_program)
        wm_set_focus(-1);
}

/**
 * @brief Build the desktop's microui frame: a "system" window (sound toggle,
 *        shutdown), a "console" window (scrolling log + command textbox) and
 *        one window per ring-3 program that asked for one.
 */
void mu_2() {
    mu_begin(&ctx);
    if (mu_begin_window(&ctx, "system",
                        mu_rect(10, 10, 90, 90))) {
        //mu_layout_row(&ctx, 3, (int[]) { 100, 100, 100 }, 0);
        mu_layout_begin_column(&ctx);
        if (mu_button(&ctx, "sound on")) {
            sound_toggle();
        }
        if (mu_button(&ctx, "shutdown")) {
            exit_qemu(0);
        }
        mu_layout_end_column(&ctx);
        mu_end_window(&ctx);
    }

    if (mu_begin_window(&ctx, "console", mu_rect(50, 130, 500, 320))) {
        mu_layout_row(&ctx, 1, (int[]) { -1 }, -25);
        mu_begin_panel(&ctx, "Log Output");
        mu_Container *panel = mu_get_current_container(&ctx);
        mu_layout_row(&ctx, 1, (int[]) { -1 }, -1);
        mu_text(&ctx, logbuf);
        mu_end_panel(&ctx);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }
        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        mu_layout_row(&ctx, 2, (int[]) { -70, -1 }, 0);
        if (mu_textbox(&ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(&ctx, ctx.last_id);
            submitted = 1;
        }
        if (mu_button(&ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            printf("%s\n", buf);
            console_exec(buf);
            buf[0] = '\0';
        }
        mu_end_window(&ctx);
    }

    draw_program_windows();

    mu_end(&ctx);
}

void mu() {
    mu_init(&ctx);
    ctx.text_width = text_width;
    ctx.text_height = text_height;
}

#define MAX_STARS 1000

typedef struct {
	int x, y, speed;
} star_type;

star_type stars[MAX_STARS];
int stars_initialized = 0;

/** @brief Pack 8-bit r/g/b into a 0x00RRGGBB pixel value. */
unsigned long createRGB(int r, int g, int b) {
    return ((r & 0xff) << 16) + ((g & 0xff) << 8) + (b & 0xff);
}

/** @brief Render one frame of the desktop: starfield background then the UI. */
void paint_desktop() {
    /* Release windows closed since the last frame. This thread is the only
     * reader of a window's pixels, so it is the only place the memory behind
     * them can be freed without racing the compositor (see wm.h). */
    wm_frame_begin();

    int scr_w = vbemem.xres ? vbemem.xres : 1280;
    int scr_h = vbemem.yres ? vbemem.yres : 1024;

    /* Reseed the starfield when the desktop is re-moded (window resize). */
    static int last_w, last_h;
    if (scr_w != last_w || scr_h != last_h) {
        last_w = scr_w;
        last_h = scr_h;
        stars_initialized = 0;
    }

    draw_rect(0, 0, scr_w, scr_h, 0x2D);

    if (stars_initialized == 0) {
        for (int i = 0;i<MAX_STARS;i++){
            stars[i].x=rand() % scr_w;
            stars[i].y=rand() % scr_h;
            stars[i].speed = 1 + rand() % 16; // change 16 for diff effects
        }
        stars_initialized = 1;
    }

    for (int i=0;i<MAX_STARS;i++) {
        stars[i].x -= stars[i].speed;

        if (stars[i].x <= 0)
            stars[i].x = scr_w;

        draw_rect(stars[i].x, stars[i].y, 1, 1, 0xffffff);
    }

    //draw_line(rand() % 639, rand() % 479, rand() % 639, rand() % 479, 0xffffffff);

    rtc_read_datetime();
    char* dt = get_current_datetime_str();
    draw_string(8*65, 0, dt, 0xFFFFFFFF);
    kfree(dt);

    /*char buf[32];
    snprintf(buf, 32, "tick: %03d", get_tick_count());
    draw_string(8*65, 16, buf, 0xFFFFFFFF);*/

    if (mouse_left_button_down()) {
        mu_input_mousedown(&ctx, get_mouse_info()->x, get_mouse_info()->y, MU_MOUSE_LEFT);
    }
    if (mouse_left_button_up()) {
        mu_input_mouseup(&ctx, get_mouse_info()->x, get_mouse_info()->y, MU_MOUSE_LEFT);
    }

    if (mouse_right_button_down()) {
        mu_input_mousedown(&ctx, get_mouse_info()->x, get_mouse_info()->y, MU_MOUSE_RIGHT);
    }
    if (mouse_right_button_up()) {
        mu_input_mouseup(&ctx, get_mouse_info()->x, get_mouse_info()->y, MU_MOUSE_RIGHT);
    }

    mouse_info.prev_button = mouse_info.curr_button;

    mu_input_mousemove(&ctx, get_mouse_info()->x, get_mouse_info()->y);

    /*
     * The keyboard belongs to the interactive shell (the user-space apps/zsh, or
     * the in-kernel console as a fallback), which drains the same single-consumer
     * ring. Draining it here as well would split every keystroke between the two.
     * The desktop is therefore mouse-only; the console window still shows the log
     * output, and commands are typed at the shell prompt.
     */

    mu_2();

    mu_Command *cmd = 0;
    while (mu_next_command(&ctx, &cmd)) {
        switch (cmd->type) {
        case MU_COMMAND_TEXT:
            r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
            break;
        case MU_COMMAND_RECT:
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
            break;
        case MU_COMMAND_ICON:
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        case MU_COMMAND_CLIP:
            r_set_clip_rect(cmd->clip.rect);
            break;
        case MU_COMMAND_SURFACE: {
            surface_command *sc = (surface_command *) cmd;
            wm_window_t *win = wm_slot(sc->slot);
            if (win)
                r_draw_surface(sc->rect, win->pix, win->w, win->h, win->pal);
            break;
        }
        default:
            break;
        }
    }

    /* The pointer, in the focused window's own coordinates. Sampled here, at
     * the end of the frame, because the windows have just been laid out and
     * this is the only moment their positions are known to be current. */
    wm_pointer(get_mouse_info()->x, get_mouse_info()->y,
               get_mouse_info()->curr_button);

    paint_mouse2();
}

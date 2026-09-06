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
#include <pit.h>

#include <microui.h>
#include <renderer.h>
#include <printf.h>
#include <lib/string.h>
#include <keyboard.h>
#include <pcspk.h>
#include <commands.h>
#include <bmp.h>
#include <sound.h>
#include <rand.h>

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

/**
 * @brief Build the desktop's microui frame: a "system" window (sound toggle,
 *        shutdown) and a "console" window (scrolling log + command textbox).
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
    int scr_w = vbemem.xres ? vbemem.xres : 1280;
    int scr_h = vbemem.yres ? vbemem.yres : 1024;

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
        if (cmd->type == MU_COMMAND_TEXT) {
            r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
        }
        if (cmd->type == MU_COMMAND_RECT) {
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
        }
        if (cmd->type == MU_COMMAND_ICON) {
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
        }
        if (cmd->type == MU_COMMAND_CLIP) {
            r_set_clip_rect(cmd->clip.rect);
        }
    }
    
    //paint_mouse();

    paint_mouse2();
}

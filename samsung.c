/*
 * Samsung TV Remote for Flipper Zero
 *
 * Copyright (c) 2026 DJLeleX
 * https://github.com/DJLeleX/Flipper-SamsungTVRemote
 *
 * Based on "Flipper Zero Samsung TV Remote" by Tyler Berndt (purplefox-io),
 * https://github.com/purplefox-io/flipper-samsung-remote
 * Original work Copyright (c) 2025 Tyler Berndt, MIT License (see LICENSE-MIT).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/icon.h>
#include <gui/elements.h>
#include <string.h>

#include "infrared_signal.h"
#include <infrared_transmit.h>

#define SAMSUNG_TX_FRAMES 3
#include "samsungtvremote_icons.h"

#define APP_VERSION "2.0"
#define MOD_AUTHOR     "DJLeleX"
#define MOD_REPO_LINE1 "github.com/DJLeleX/"
#define MOD_REPO_LINE2 "Flipper-SamsungTVRemote"

static const char* const about_text = "\e#\ecSamsung TV Remote\n"
                                      "\ecv" APP_VERSION "\n"
                                      "\n"
                                      "\ecby " MOD_AUTHOR "\n"
                                      "\ec" MOD_REPO_LINE1 "\n"
                                      "\ec" MOD_REPO_LINE2 "\n"
                                      "\n"
                                      "\ecBased on the original app\n"
                                      "\ecby Tyler Berndt\n"
                                      "\ecgithub.com/purplefox-io/\n"
                                      "\ecflipper-samsung-remote\n"
                                      "\n"
                                      "\ecPress Back to return";

static const char* const help_text = "\e#\ecQuick guide\n"
                                     "Hold the Flipper sideways,\n"
                                     "IR window toward the TV.\n"
                                     "\n"
                                     "\e#Flipper keys\n"
                                     "D-pad: move (edges wrap)\n"
                                     "OK: send, hold to repeat\n"
                                     "Back: number pad > remote\n"
                                     "  > exit app\n"
                                     "\n"
                                     "\e#Main page\n"
                                     "Power icon: TV on / off\n"
                                     "123: open the number pad\n"
                                     "Arrow icon: return / back\n"
                                     "House icon: TV home\n"
                                     "Speaker icon: mute / unmute\n"
                                     "Left rocker: volume\n"
                                     "Right rocker: channel\n"
                                     "TV+: Samsung TV Plus\n"
                                     "Prime: Prime Video\n"
                                     "\n"
                                     "\e#Number pad\n"
                                     "0-9: channel digits\n"
                                     "House icon: TV home\n"
                                     "Pre: previous channel\n"
                                     "Info: program info\n"
                                     "Guide: TV guide\n"
                                     "\n"
                                     "\ecPress Back to return";

typedef enum {
    AppViewRemote,
    AppViewText,
} AppView;

typedef enum {
    Button_Power,
    Button_Up,
    Button_Left,
    Button_Enter,
    Button_Right,
    Button_Down,
    Button_Volume_Up,
    Button_Channel_Up,
    Button_Volume_Down,
    Button_Channel_Down,
    Button_TVPlus,
    Button_Amazon,
    Button_Return,
    Button_Mute,
    Button_Home,
    Button_MainHelp,
    Button_MainAbout,
    Button_About,
    Button_Numbers,
    Button_Num1,
    Button_Num2,
    Button_Num3,
    Button_Num4,
    Button_Num5,
    Button_Num6,
    Button_Num7,
    Button_Num8,
    Button_Num9,
    Button_PreCh,
    Button_Num0,
    Button_Info,
    Button_NumHome,
    Button_Guide,
    Button_Help,
    Button_Max,
} TvButton;

typedef enum {
    RemotePageMain = 0,
    RemotePageNumbers,
} RemotePage;

typedef struct {
    const char* label;
    const Icon* icon;
    uint8_t x, y, w, h;
    TvButton nav_up;
    TvButton nav_down;
    TvButton nav_left;
    TvButton nav_right;
    RemotePage page;
} RemoteButtonDef;

static const RemoteButtonDef button_defs[Button_Max] = {
    [Button_Power]        = {NULL,   &I_power8x7,        2,   1, 28, 11, Button_Power,        Button_Enter,        Button_Power,        Button_Numbers},
    [Button_Numbers]      = {"123",  NULL,              34,   1, 28, 11, Button_Numbers,      Button_Enter,        Button_Power,        Button_Numbers},

    [Button_Up]           = {NULL,   &I_up7x4,          28,  15,  7,  4, Button_Power,        Button_Enter,        Button_Up,           Button_Up},
    [Button_Enter]        = {"OK",   NULL,              20,  22, 24, 11, Button_Up,           Button_Home,         Button_Left,         Button_Right},
    [Button_Left]         = {NULL,   &I_left7x4,        13,  24,  4,  7, Button_Up,           Button_Return,       Button_Left,         Button_Enter},
    [Button_Right]        = {NULL,   &I_right7x4,       47,  24,  4,  7, Button_Up,           Button_Mute,         Button_Enter,        Button_Right},
    [Button_Down]         = {NULL,   &I_down7x4,        28,  36,  7,  4, Button_Enter,        Button_Home,         Button_Down,         Button_Down},

    [Button_Return]       = {NULL,   &I_return12x8,      1,  43, 20, 12, Button_Left,         Button_Volume_Up,    Button_Return,       Button_Home},
    [Button_Home]         = {NULL,   &I_home12x8,       22,  43, 20, 12, Button_Down,         Button_Volume_Up,    Button_Return,       Button_Mute},
    [Button_Mute]         = {NULL,   &I_mute14x8,       43,  43, 20, 12, Button_Right,        Button_Channel_Up,   Button_Home,         Button_Mute},

    [Button_Volume_Up]    = {NULL,   &I_volup24x14,      4,  60, 24, 14, Button_Return,       Button_Volume_Down,  Button_Volume_Up,    Button_Channel_Up},
    [Button_Channel_Up]   = {NULL,   &I_chup24x14,      36,  60, 24, 14, Button_Mute,         Button_Channel_Down, Button_Volume_Up,    Button_Channel_Up},
    [Button_Volume_Down]  = {NULL,   &I_voldown24x14,    4,  76, 24, 14, Button_Volume_Up,    Button_TVPlus,       Button_Volume_Down,  Button_Channel_Down},
    [Button_Channel_Down] = {NULL,   &I_chdown24x14,    36,  76, 24, 14, Button_Channel_Up,   Button_Amazon,       Button_Volume_Down,  Button_Channel_Down},

    [Button_TVPlus]       = {"TV+",  NULL,               2,  96, 28, 11, Button_Volume_Down,  Button_MainHelp,     Button_TVPlus,       Button_Amazon},
    [Button_Amazon]       = {"Prime",NULL,              34,  96, 28, 11, Button_Channel_Down, Button_MainAbout,    Button_TVPlus,       Button_Amazon},

    [Button_MainHelp]     = {"Help", NULL,               2, 112, 28, 11, Button_TVPlus,       Button_MainHelp,     Button_MainHelp,     Button_MainAbout},
    [Button_MainAbout]    = {"About",NULL,              34, 112, 28, 11, Button_Amazon,       Button_MainAbout,    Button_MainHelp,     Button_MainAbout},

    [Button_Num1]       = {"1",     NULL,        2, 16, 18, 16, Button_Num1,    Button_Num4,    Button_Num1,    Button_Num2,    RemotePageNumbers},
    [Button_Num2]       = {"2",     NULL,       23, 16, 18, 16, Button_Num2,    Button_Num5,    Button_Num1,    Button_Num3,    RemotePageNumbers},
    [Button_Num3]       = {"3",     NULL,       44, 16, 18, 16, Button_Num3,    Button_Num6,    Button_Num2,    Button_Num3,    RemotePageNumbers},
    [Button_Num4]       = {"4",     NULL,        2, 36, 18, 16, Button_Num1,    Button_Num7,    Button_Num4,    Button_Num5,    RemotePageNumbers},
    [Button_Num5]       = {"5",     NULL,       23, 36, 18, 16, Button_Num2,    Button_Num8,    Button_Num4,    Button_Num6,    RemotePageNumbers},
    [Button_Num6]       = {"6",     NULL,       44, 36, 18, 16, Button_Num3,    Button_Num9,    Button_Num5,    Button_Num6,    RemotePageNumbers},
    [Button_Num7]       = {"7",     NULL,        2, 56, 18, 16, Button_Num4,    Button_NumHome, Button_Num7,    Button_Num8,    RemotePageNumbers},
    [Button_Num8]       = {"8",     NULL,       23, 56, 18, 16, Button_Num5,    Button_Num0,    Button_Num7,    Button_Num9,    RemotePageNumbers},
    [Button_Num9]       = {"9",     NULL,       44, 56, 18, 16, Button_Num6,    Button_PreCh,   Button_Num8,    Button_Num9,    RemotePageNumbers},
    [Button_NumHome]    = {NULL,    &I_home12x8, 2, 76, 18, 16, Button_Num7,    Button_Info,    Button_NumHome, Button_Num0,    RemotePageNumbers},
    [Button_Num0]       = {"0",     NULL,       23, 76, 18, 16, Button_Num8,    Button_Info,    Button_NumHome, Button_PreCh,   RemotePageNumbers},
    [Button_PreCh]      = {"Pre",   NULL,       44, 76, 18, 16, Button_Num9,    Button_Guide,   Button_Num0,    Button_PreCh,   RemotePageNumbers},
    [Button_Info]       = {"Info",  NULL,        2, 98, 28, 11, Button_NumHome, Button_Help,    Button_Info,    Button_Guide,   RemotePageNumbers},
    [Button_Guide]      = {"Guide", NULL,       34, 98, 28, 11, Button_PreCh,   Button_About,   Button_Info,    Button_Guide,   RemotePageNumbers},
    [Button_Help]       = {"Help",  NULL,        2, 113, 28, 11, Button_Info,   Button_Help,    Button_Help,    Button_About,   RemotePageNumbers},
    [Button_About]      = {"About", NULL,       34, 113, 28, 11, Button_Guide,  Button_About,   Button_Help,    Button_About,   RemotePageNumbers},
};

static Font button_font(const RemoteButtonDef* btn) {
    const char* label = btn->label;
    if(label[0] >= '0' && label[0] <= '9' && label[1] == '\0') {
        return FontPrimary;
    }
    return FontSecondary;
}

static TvButton button_wrap(TvButton from, InputKey key) {
    const RemoteButtonDef* f = &button_defs[from];
    const bool vertical = (key == InputKeyUp || key == InputKeyDown);
    const int f_cx = f->x + f->w / 2;
    const int f_cy = f->y + f->h / 2;

    TvButton best = from;
    int best_score = 0;
    for(int i = 0; i < Button_Max; i++) {
        const RemoteButtonDef* b = &button_defs[i];
        if(i == (int)from || b->page != f->page) continue;
        const int b_cx = b->x + b->w / 2;
        const int b_cy = b->y + b->h / 2;

        int score;
        if(vertical) {
            const bool same_column = (f_cx >= b->x && f_cx < b->x + b->w) ||
                                     (b_cx >= f->x && b_cx < f->x + f->w);
            if(!same_column) continue;
            score = (key == InputKeyDown) ? -b->y : b->y;
        } else {
            const bool same_row = (f_cy >= b->y && f_cy < b->y + b->h) ||
                                  (b_cy >= f->y && b_cy < f->y + f->h);
            if(!same_row) continue;
            score = (key == InputKeyRight) ? -b->x : b->x;
        }
        if(best == from || score > best_score) {
            best = (TvButton)i;
            best_score = score;
        }
    }
    return best;
}

static bool button_is_framed_icon(const RemoteButtonDef* btn) {
    return btn->icon &&
           (btn->w > icon_get_width(btn->icon) || btn->h > icon_get_height(btn->icon));
}

static void draw_centered_icon(Canvas* canvas, const RemoteButtonDef* btn) {
    canvas_draw_icon(
        canvas,
        btn->x + (btn->w - icon_get_width(btn->icon)) / 2,
        btn->y + (btn->h - icon_get_height(btn->icon)) / 2,
        btn->icon);
}

static const Icon* button_selected_icon(TvButton button) {
    switch(button) {
    case Button_Volume_Up:
        return &I_volup24x14_sel;
    case Button_Volume_Down:
        return &I_voldown24x14_sel;
    case Button_Channel_Up:
        return &I_chup24x14_sel;
    case Button_Channel_Down:
        return &I_chdown24x14_sel;
    default:
        return NULL;
    }
}

typedef struct {
    TvButton current_button;
} RemoteViewModel;

typedef struct {
    ViewDispatcher* view_dispatcher;
    View* remote_view;
    View* text_view;
    NotificationApp* notifications;
} App;

static void send_ir_code(TvButton button, NotificationApp* notifications);

#define TEXT_MAX_LINES 48
#define TEXT_LINE_MAX  48
#define TEXT_WIDTH     124

typedef struct {
    const char* text;
    uint8_t pos;
    uint8_t max_pos;
} TextViewModel;

typedef struct {
    const char* start;
    uint8_t len;
    Font font;
    Align align;
} TextLine;

static size_t text_split(const char* text, TextLine* lines, size_t max) {
    size_t count = 0;
    const char* p = text;
    while(count < max) {
        TextLine* line = &lines[count++];
        line->font = FontSecondary;
        line->align = AlignLeft;
        while(p[0] == '\e' && p[1] != '\0') {
            if(p[1] == '#') line->font = FontPrimary;
            if(p[1] == 'c') line->align = AlignCenter;
            if(p[1] == 'r') line->align = AlignRight;
            p += 2;
        }
        const char* end = strchr(p, '\n');
        const size_t len = end ? (size_t)(end - p) : strlen(p);
        line->start = p;
        line->len = len < TEXT_LINE_MAX ? len : TEXT_LINE_MAX - 1;
        if(!end) break;
        p = end + 1;
    }
    return count;
}

static void text_view_draw_callback(Canvas* canvas, void* model) {
    TextViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(!m->text) return;

    TextLine lines[TEXT_MAX_LINES];
    const size_t count = text_split(m->text, lines, TEXT_MAX_LINES);

    size_t max_pos = count;
    int height = 0;
    while(max_pos > 0) {
        const int h = canvas_get_font_params(canvas, lines[max_pos - 1].font)->leading_default;
        if(height + h > 64) break;
        height += h;
        max_pos--;
    }
    m->max_pos = max_pos;
    if(m->pos > max_pos) m->pos = max_pos;

    char buf[TEXT_LINE_MAX];
    int y = 0;
    for(size_t i = m->pos; i < count && y < 64; i++) {
        const CanvasFontParameters* fp = canvas_get_font_params(canvas, lines[i].font);
        memcpy(buf, lines[i].start, lines[i].len);
        buf[lines[i].len] = '\0';
        const int x = lines[i].align == AlignCenter ? TEXT_WIDTH / 2 :
                      lines[i].align == AlignRight  ? TEXT_WIDTH :
                                                      0;
        canvas_set_font(canvas, lines[i].font);
        canvas_draw_str_aligned(
            canvas, x, y + fp->leading_default - fp->descender, lines[i].align, AlignBottom, buf);
        y += fp->leading_default;
    }
    if(max_pos > 0) elements_scrollbar_pos(canvas, 128, 0, 64, m->pos, max_pos + 1);
}

static bool text_view_input_callback(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    with_view_model(
        app->text_view,
        TextViewModel * m,
        {
            if(event->key == InputKeyDown) {
                m->pos = (m->pos >= m->max_pos) ? 0 : m->pos + 1;
            } else {
                m->pos = (m->pos == 0) ? m->max_pos : m->pos - 1;
            }
        },
        true);
    return true;
}

static void app_show_text_screen(App* app, const char* text) {
    with_view_model(
        app->text_view,
        TextViewModel * m,
        {
            m->text = text;
            m->pos = 0;
            m->max_pos = 0;
        },
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewText);
}

static uint32_t text_screen_back_callback(void* context) {
    UNUSED(context);
    return AppViewRemote;
}

static void app_draw_callback(Canvas* canvas, void* model) {
    RemoteViewModel* view_model = model;
    canvas_clear(canvas);

    const RemotePage page = button_defs[view_model->current_button].page;

    if(page == RemotePageNumbers) {
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 6, AlignCenter, AlignCenter, "Numbers");
    }

    for(int i = 0; i < Button_Max; i++) {
        const RemoteButtonDef* btn = &button_defs[i];
        if(btn->page != page) continue;
        canvas_set_color(canvas, ColorBlack);
        if(button_is_framed_icon(btn)) {
            canvas_draw_rframe(canvas, btn->x, btn->y, btn->w, btn->h, 3);
            draw_centered_icon(canvas, btn);
        } else if(btn->icon) {
            canvas_draw_icon(canvas, btn->x, btn->y, btn->icon);
        } else {
            canvas_draw_rframe(canvas, btn->x, btn->y, btn->w, btn->h, 3);
            canvas_set_font(canvas, button_font(btn));
            canvas_draw_str_aligned(
                canvas, btn->x + btn->w / 2, btn->y + (btn->h + 1) / 2, AlignCenter, AlignCenter, btn->label);
        }
    }

    const RemoteButtonDef* selected_btn = &button_defs[view_model->current_button];
    const Icon* selected_icon = button_selected_icon(view_model->current_button);
    canvas_set_color(canvas, ColorBlack);
    if(selected_icon) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, selected_btn->x, selected_btn->y, selected_btn->w, selected_btn->h);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, selected_btn->x, selected_btn->y, selected_icon);
    } else if(button_is_framed_icon(selected_btn)) {
        canvas_draw_rbox(canvas, selected_btn->x, selected_btn->y, selected_btn->w, selected_btn->h, 3);
        canvas_set_color(canvas, ColorWhite);
        draw_centered_icon(canvas, selected_btn);
    } else if(selected_btn->icon) {
        canvas_draw_box(canvas, selected_btn->x - 2, selected_btn->y - 2, selected_btn->w + 4, selected_btn->h + 4);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, selected_btn->x, selected_btn->y, selected_btn->icon);
    } else {
        canvas_draw_rbox(canvas, selected_btn->x, selected_btn->y, selected_btn->w, selected_btn->h, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, button_font(selected_btn));
        canvas_draw_str_aligned(
            canvas,
            selected_btn->x + selected_btn->w / 2,
            selected_btn->y + (selected_btn->h + 1) / 2,
            AlignCenter,
            AlignCenter,
            selected_btn->label);
    }
}

static bool app_input_callback(InputEvent* event, void* context) {
    App* app = context;
    bool handled = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    TvButton selected = Button_Power;
    with_view_model(
        app->remote_view, RemoteViewModel * model, { selected = model->current_button; }, false);
    const bool on_numbers_page = button_defs[selected].page == RemotePageNumbers;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            if(on_numbers_page) {
                with_view_model(
                    app->remote_view,
                    RemoteViewModel * model,
                    { model->current_button = Button_Numbers; },
                    true);
            } else {
                view_dispatcher_stop(app->view_dispatcher);
            }
        }
        handled = true;
    } else if(event->key == InputKeyOk) {
        if(selected == Button_About || selected == Button_Help || selected == Button_MainAbout ||
           selected == Button_MainHelp) {
            if(event->type == InputTypeShort) {
                const bool about = selected == Button_About || selected == Button_MainAbout;
                app_show_text_screen(app, about ? about_text : help_text);
            }
        } else if(selected == Button_Numbers) {
            if(event->type == InputTypeShort) {
                with_view_model(
                    app->remote_view,
                    RemoteViewModel * model,
                    { model->current_button = Button_Num1; },
                    true);
            }
        } else {
            send_ir_code(selected, app->notifications);
        }
        handled = true;
    } else {
        with_view_model(
            app->remote_view,
            RemoteViewModel* model,
            {
                const RemoteButtonDef* btn_def = &button_defs[model->current_button];
                if(model->current_button == Button_Enter) {
                    switch(event->key) {
                    case InputKeyUp:
                        model->current_button = Button_Up;
                        break;
                    case InputKeyDown:
                        model->current_button = Button_Down;
                        break;
                    case InputKeyLeft:
                        model->current_button = Button_Left;
                        break;
                    case InputKeyRight:
                        model->current_button = Button_Right;
                        break;
                    default:
                        break;
                    }
                } else {
                    TvButton next = model->current_button;
                    switch(event->key) {
                    case InputKeyUp:
                        next = btn_def->nav_up;
                        break;
                    case InputKeyDown:
                        next = btn_def->nav_down;
                        break;
                    case InputKeyLeft:
                        next = btn_def->nav_left;
                        break;
                    case InputKeyRight:
                        next = btn_def->nav_right;
                        break;
                    default:
                        break;
                    }
                    if(next == model->current_button) {
                        next = button_wrap(model->current_button, event->key);
                    }
                    model->current_button = next;
                }
            },
            true);
        handled = true;
    }

    return handled;
}

static void send_ir_code(TvButton button, NotificationApp* notifications) {
    notification_message(notifications, &sequence_blink_magenta_10);

    uint8_t address = 0x07;
    uint8_t command;

    switch(button) {
    case Button_Power:
        command = 0x02;
        break;
    case Button_Volume_Up:
        command = 0x07;
        break;
    case Button_Volume_Down:
        command = 0x0B;
        break;
    case Button_Channel_Up:
        command = 0x12;
        break;
    case Button_Channel_Down:
        command = 0x10;
        break;
    case Button_Left:
        command = 0x65;
        break;
    case Button_Right:
        command = 0x62;
        break;
    case Button_Up:
        command = 0x60;
        break;
    case Button_Down:
        command = 0x61;
        break;
    case Button_Return:
        command = 0x58;
        break;
    case Button_Enter:
        command = 0x68;
        break;
    case Button_Mute:
        command = 0x0F;
        break;
    case Button_Home:
    case Button_NumHome:
        command = 0x79;
        break;
    case Button_TVPlus:
        command = 0xB4;
        break;
    case Button_Amazon:
        command = 0xF4;
        break;
    case Button_Num1:
        command = 0x04;
        break;
    case Button_Num2:
        command = 0x05;
        break;
    case Button_Num3:
        command = 0x06;
        break;
    case Button_Num4:
        command = 0x08;
        break;
    case Button_Num5:
        command = 0x09;
        break;
    case Button_Num6:
        command = 0x0A;
        break;
    case Button_Num7:
        command = 0x0C;
        break;
    case Button_Num8:
        command = 0x0D;
        break;
    case Button_Num9:
        command = 0x0E;
        break;
    case Button_Num0:
        command = 0x11;
        break;
    case Button_PreCh:
        command = 0x13;
        break;
    case Button_Info:
        command = 0x1F;
        break;
    case Button_Guide:
        command = 0x4F;
        break;
    default:
        notification_message(notifications, &sequence_reset_rgb);
        return;
    }

    InfraredMessage message = {
        .protocol = InfraredProtocolSamsung32, .address = address, .command = command, .repeat = false};
    infrared_send(&message, SAMSUNG_TX_FRAMES);

    furi_delay_ms(50);
    notification_message(notifications, &sequence_reset_rgb);
}

static App* app_alloc() {
    App* app = malloc(sizeof(App));

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->remote_view = view_alloc();
    view_set_orientation(app->remote_view, ViewOrientationVertical);
    view_set_context(app->remote_view, app);
    view_allocate_model(app->remote_view, ViewModelTypeLockFree, sizeof(RemoteViewModel));
    with_view_model(
        app->remote_view,
        RemoteViewModel* model,
        { model->current_button = Button_Power; },
        true);
    view_set_draw_callback(app->remote_view, app_draw_callback);
    view_set_input_callback(app->remote_view, app_input_callback);
    view_dispatcher_add_view(app->view_dispatcher, AppViewRemote, app->remote_view);

    app->text_view = view_alloc();
    view_set_context(app->text_view, app);
    view_allocate_model(app->text_view, ViewModelTypeLockFree, sizeof(TextViewModel));
    with_view_model(
        app->text_view,
        TextViewModel * m,
        {
            m->text = NULL;
            m->pos = 0;
            m->max_pos = 0;
        },
        false);
    view_set_draw_callback(app->text_view, text_view_draw_callback);
    view_set_input_callback(app->text_view, text_view_input_callback);
    view_set_previous_callback(app->text_view, text_screen_back_callback);
    view_dispatcher_add_view(app->view_dispatcher, AppViewText, app->text_view);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewRemote);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    return app;
}

static void app_free(App* app) {
    furi_assert(app);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewRemote);
    view_free(app->remote_view);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewText);
    view_free(app->text_view);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t samsung_remote_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}

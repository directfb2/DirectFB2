/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#define DFB_INPUTDRIVER_HAS_AXIS_INFO
#define DFB_INPUTDRIVER_HAS_SET_CONFIGURATION
#define DISABLE_INPUT_HOTPLUG_FUNCTION_STUB

#include <core/input_driver.h>
#include <direct/thread.h>
#include <fusion/vector.h>
#include <linux/input.h>
#include <linux/keyboard.h>
#include <sys/kd.h>

D_DEBUG_DOMAIN( Linux_Input, "Input/Linux", "Linux Input Driver" );

DFB_INPUT_DRIVER( linux_input )

/**********************************************************************************************************************/

#define NBITS(x) ((((x) - 1) / (sizeof(long) * 8)) + 1)

typedef struct {
     CoreInputDevice         *device;
     int                      index;

     int                      fd;

     bool                     grab;

     bool                     has_keys;
     bool                     has_leds;
     unsigned long            led_state[NBITS(LED_CNT)];
     DFBInputDeviceLockState  locks;

     bool                     touchpad;
     bool                     touch_abs;

     int                      sensitivity;

     int                      dx;
     int                      dy;

     int                      vt_fd;

     DirectThread            *thread;
     int                      quitpipe[2];
} LinuxInputData;

#define MAX_LINUX_INPUT_DEVICES 16

/* Input devices are stored in the device_names and device_nums arrays: entries with the same index in device_names and
device_nums are the same in two different forms (one is /dev/input/eventX, the other is X. */
static char *device_names[MAX_LINUX_INPUT_DEVICES];
static int   device_nums[MAX_LINUX_INPUT_DEVICES] = { 0 };

/* Number of entries in the device_names and device_nums arrays. */
static int   num_devices = 0;

#define MAX_LINUX_INPUT_EVENTS 64

/* Input events filled on read. */
struct input_event input_events[MAX_LINUX_INPUT_EVENTS];

#if !defined(input_event_sec) && !defined(input_event_usec)
#define input_event_sec  time.tv_sec
#define input_event_usec time.tv_usec
#endif

/**********************************************************************************************************************/

#define OFF(x)              ((x) % (sizeof(long) * 8))
#define LONG(x)             ((x) / (sizeof(long) * 8))
#define test_bit(bit,array) ((array[LONG(bit)] >> OFF(bit)) & 1)

static const int basic_keycodes[] = {
     DIKI_UNKNOWN, DIKI_ESCAPE,
     DIKI_1, DIKI_2, DIKI_3, DIKI_4, DIKI_5, DIKI_6, DIKI_7, DIKI_8, DIKI_9, DIKI_0,
     DIKI_MINUS_SIGN, DIKI_EQUALS_SIGN, DIKI_BACKSPACE, DIKI_TAB,
     DIKI_Q, DIKI_W, DIKI_E, DIKI_R, DIKI_T, DIKI_Y, DIKI_U, DIKI_I, DIKI_O, DIKI_P,
     DIKI_BRACKET_LEFT, DIKI_BRACKET_RIGHT, DIKI_ENTER, DIKI_CONTROL_L,
     DIKI_A, DIKI_S, DIKI_D, DIKI_F, DIKI_G, DIKI_H, DIKI_J, DIKI_K, DIKI_L,
     DIKI_SEMICOLON, DIKI_QUOTE_RIGHT, DIKI_QUOTE_LEFT, DIKI_SHIFT_L, DIKI_BACKSLASH,
     DIKI_Z, DIKI_X, DIKI_C, DIKI_V, DIKI_B, DIKI_N, DIKI_M,
     DIKI_COMMA, DIKI_PERIOD, DIKI_SLASH, DIKI_SHIFT_R, DIKI_KP_MULT, DIKI_ALT_L, DIKI_SPACE, DIKI_CAPS_LOCK,
     DIKI_F1, DIKI_F2, DIKI_F3, DIKI_F4, DIKI_F5, DIKI_F6, DIKI_F7, DIKI_F8, DIKI_F9, DIKI_F10,
     DIKI_NUM_LOCK, DIKI_SCROLL_LOCK,
     DIKI_KP_7, DIKI_KP_8, DIKI_KP_9, DIKI_KP_MINUS,
     DIKI_KP_4, DIKI_KP_5, DIKI_KP_6, DIKI_KP_PLUS,
     DIKI_KP_1, DIKI_KP_2, DIKI_KP_3,
     DIKI_KP_0, DIKI_KP_DECIMAL,
     DIKI_BACKSLASH, DFB_FUNCTION_KEY(13), DIKI_LESS_SIGN, DIKI_F11, DIKI_F12, DFB_FUNCTION_KEY(14),
     DFB_FUNCTION_KEY(15), DFB_FUNCTION_KEY(16), DFB_FUNCTION_KEY(17),
     DFB_FUNCTION_KEY(18), DFB_FUNCTION_KEY(19), DFB_FUNCTION_KEY(20),
     DIKI_KP_ENTER, DIKI_CONTROL_R, DIKI_KP_DIV, DIKI_PRINT, DIKS_ALTGR, DIKI_UNKNOWN, DIKI_HOME,
     DIKI_UP, DIKI_PAGE_UP, DIKI_LEFT, DIKI_RIGHT, DIKI_END, DIKI_DOWN, DIKI_PAGE_DOWN, DIKI_INSERT, DIKI_DELETE,
     DIKI_UNKNOWN, DIKS_MUTE, DIKS_VOLUME_DOWN, DIKS_VOLUME_UP, DIKS_POWER, DIKI_KP_EQUAL, DIKI_UNKNOWN, DIKS_PAUSE,
     DFB_FUNCTION_KEY(21), DFB_FUNCTION_KEY(22), DFB_FUNCTION_KEY(23), DFB_FUNCTION_KEY(24), DIKI_KP_SEPARATOR,
     DIKI_META_L, DIKI_META_R, DIKI_SUPER_L, DIKS_STOP, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_HELP, DIKS_MENU, DIKS_CALCULATOR, DIKS_SETUP,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_CUSTOM1, DIKS_CUSTOM2, DIKS_INTERNET, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKS_MAIL,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKS_BACK, DIKS_FORWARD, DIKS_EJECT, DIKS_EJECT, DIKS_EJECT,
     DIKS_NEXT, DIKS_PLAYPAUSE, DIKS_PREVIOUS, DIKS_STOP, DIKS_RECORD, DIKS_REWIND, DIKS_PHONE, DIKI_UNKNOWN,
     DIKS_SETUP, DIKI_UNKNOWN, DIKS_SHUFFLE, DIKS_EXIT, DIKI_UNKNOWN, DIKS_EDITOR,
     DIKS_PAGE_UP, DIKS_PAGE_DOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DFB_FUNCTION_KEY(13), DFB_FUNCTION_KEY(14), DFB_FUNCTION_KEY(15), DFB_FUNCTION_KEY(16),
     DFB_FUNCTION_KEY(17), DFB_FUNCTION_KEY(18), DFB_FUNCTION_KEY(19), DFB_FUNCTION_KEY(20),
     DFB_FUNCTION_KEY(21), DFB_FUNCTION_KEY(22), DFB_FUNCTION_KEY(23), DFB_FUNCTION_KEY(24),
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_PLAY, DIKS_PAUSE, DIKS_CUSTOM3, DIKS_CUSTOM4, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_PLAY, DIKS_FASTFORWARD, DIKI_UNKNOWN, DIKS_PRINT, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_AUDIO, DIKS_HELP, DIKS_MAIL,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKS_CANCEL, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN
};

static const int ext_keycodes[] = {
     DIKS_OK, DIKS_SELECT, DIKS_GOTO, DIKS_CLEAR, DIKS_POWER2, DIKS_OPTION, DIKS_INFO, DIKS_TIME, DIKS_VENDOR,
     DIKS_ARCHIVE, DIKS_PROGRAM, DIKS_CHANNEL, DIKS_FAVORITES, DIKS_EPG, DIKS_PVR, DIKS_MHP, DIKS_LANGUAGE,
     DIKS_TITLE, DIKS_SUBTITLE, DIKS_ANGLE, DIKS_ZOOM, DIKS_MODE, DIKS_KEYBOARD, DIKS_SCREEN, DIKS_PC,
     DIKS_TV, DIKS_TV2, DIKS_VCR, DIKS_VCR2, DIKS_SAT, DIKS_SAT2, DIKS_CD, DIKS_TAPE, DIKS_RADIO, DIKS_TUNER,
     DIKS_PLAYER, DIKS_TEXT, DIKS_DVD, DIKS_AUX, DIKS_MP3, DIKS_AUDIO, DIKS_VIDEO, DIKS_DIRECTORY, DIKS_LIST,
     DIKS_MEMO, DIKS_CALENDAR, DIKS_RED, DIKS_GREEN, DIKS_YELLOW, DIKS_BLUE, DIKS_CHANNEL_UP, DIKS_CHANNEL_DOWN,
     DIKS_FIRST, DIKS_LAST, DIKS_AB, DIKS_NEXT, DIKS_RESTART, DIKS_SLOW, DIKS_SHUFFLE, DIKS_BREAK, DIKS_PREVIOUS,
     DIKS_DIGITS, DIKS_TEEN, DIKS_TWEN
};

/**********************************************************************************************************************/

static void
config_values_parse( FusionVector *vector,
                     const char   *arg )
{
     char *values = D_STRDUP( arg );
     char *s      = values;
     char *r, *p  = NULL;

     if (!values) {
          D_OOM();
          return;
     }

     while ((r = direct_strtok_r( s, ",", &p ))) {
          direct_trim( &r );

          r = D_STRDUP( r );
          if (!r)
               D_OOM();
          else
               fusion_vector_add( vector, r );

          s = NULL;
     }

     D_FREE( values );
}

static void
config_values_free( FusionVector *vector )
{
     char *value;
     int   i;

     fusion_vector_foreach (value, i, *vector)
          D_FREE( value );

     fusion_vector_destroy( vector );
}

/**********************************************************************************************************************/

enum {
     TOUCHPAD_FSM_START,
     TOUCHPAD_FSM_MAIN,
     TOUCHPAD_FSM_DRAG_START,
     TOUCHPAD_FSM_DRAG_MAIN,
};

struct touchpad_axis {
     int old, min, max;
};

struct touchpad_fsm_state {
     int                  fsm_state;
     struct touchpad_axis x;
     struct touchpad_axis y;
     struct timeval       timeout;
};

#define ACCEL_THRESHOLD 25
#define ACCEL_NUM        3
#define ACCEL_DENOM      1

static bool
timeout_is_set( const struct timeval *timeout )
{
     return timeout->tv_sec || timeout->tv_usec;
}

static bool
timeout_passed( const struct timeval *timeout,
                const struct timeval *current )
{
     return !timeout_is_set( timeout )        ||
            current->tv_sec > timeout->tv_sec ||
            (current->tv_sec == timeout->tv_sec && current->tv_usec > timeout->tv_usec);
}

static void
timeout_clear( struct timeval *timeout )
{
     timeout->tv_sec  = 0;
     timeout->tv_usec = 0;
}

static void
timeout_add( struct timeval       *timeout,
             const struct timeval *add )
{
     timeout->tv_sec  += add->tv_sec;
     timeout->tv_usec += add->tv_usec;

     while (timeout->tv_usec >= 1000000) {
          timeout->tv_sec++;
          timeout->tv_usec -= 1000000;
     }
}

static void
timeout_sub( struct timeval       *timeout,
             const struct timeval *sub )
{
     timeout->tv_sec  -= sub->tv_sec;
     timeout->tv_usec -= sub->tv_usec;

     while (timeout->tv_usec < 0) {
          timeout->tv_sec--;
          timeout->tv_usec += 1000000;
     }
}

static void
touchpad_fsm_init( struct touchpad_fsm_state *state )
{
     state->x.old = -1;
     state->y.old = -1;
     state->fsm_state = TOUCHPAD_FSM_START;

     timeout_clear( &state->timeout );
}

static int
touchpad_normalize( const struct touchpad_axis *axis,
                    int                         value )
{
     return ((value - axis->min) << 9) / (axis->max - axis->min);
}

static int
touchpad_translate( struct touchpad_fsm_state *state,
                    bool                       touch_abs,
                    const struct input_event  *levt,
                    DFBInputEvent             *devt )
{
     struct touchpad_axis *axis = NULL;
     int abs, rel;

     devt->flags             = DIEF_TIMESTAMP | (touch_abs ? DIEF_AXISABS : DIEF_AXISREL);
     devt->timestamp.tv_sec  = levt->input_event_sec;
     devt->timestamp.tv_usec = levt->input_event_usec;
     devt->type              = DIET_AXISMOTION;

     switch (levt->code) {
          case ABS_X:
               axis       = &state->x;
               devt->axis = DIAI_X;
               break;
          case ABS_Y:
               axis       = &state->y;
               devt->axis = DIAI_Y;
               break;
          default:
               return 0;
     }

     abs = touchpad_normalize( axis, levt->value );

     if (axis->old == -1)
          axis->old = abs;

     rel = abs - axis->old;

     if (rel > ACCEL_THRESHOLD)
          rel += (rel - ACCEL_THRESHOLD) * ACCEL_NUM / ACCEL_DENOM;
     else if (rel < -ACCEL_THRESHOLD)
          rel += (rel + ACCEL_THRESHOLD) * ACCEL_NUM / ACCEL_DENOM;

     axis->old     = abs;
     devt->axisrel = rel;
     devt->axisabs = levt->value;

     return 1;
}

static bool
touchpad_finger_landing( const struct input_event *levt )
{
     return levt->type == EV_KEY && levt->code == BTN_TOUCH && levt->value == 1;
}

static bool
touchpad_finger_leaving( const struct input_event *levt )
{
     return levt->type == EV_KEY && levt->code == BTN_TOUCH && levt->value == 0;
}

static bool
touchpad_finger_moving( const struct input_event *levt )
{
     return levt->type == EV_ABS && (levt->code == ABS_X || levt->code == ABS_Y);
}

static int
touchpad_fsm( struct touchpad_fsm_state *state,
              bool                       touch_abs,
              const struct input_event  *levt,
              DFBInputEvent             *devt )
{
     struct timeval levt_time;
     struct timeval timeout = { 0, 125000 };

     if (!levt) {
          if (state->fsm_state == TOUCHPAD_FSM_DRAG_START) {
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = state->timeout;
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init( state );

               return 1;
          }

          timeout_clear( &state->timeout );

          return 0;
     }

     levt_time.tv_sec  = levt->input_event_sec;
     levt_time.tv_usec = levt->input_event_usec;

     if ((levt->type == EV_SYN && levt->code == SYN_REPORT)         ||
         (levt->type == EV_ABS && levt->code == ABS_PRESSURE)       ||
         (levt->type == EV_ABS && levt->code == ABS_TOOL_WIDTH)     ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_FINGER)    ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_DOUBLETAP) ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_TRIPLETAP)) {
          if (state->fsm_state == TOUCHPAD_FSM_DRAG_START && timeout_passed( &state->timeout, &levt_time )) {
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = state->timeout;
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init( state );

               return 1;
          }

          return 0;
     }

     /* Use translate_event() for other events. */
     if (!(levt->type == EV_KEY && levt->code == BTN_TOUCH) &&
         !(levt->type == EV_ABS && (levt->code == ABS_X || levt->code == ABS_Y)))
          return -1;

     switch (state->fsm_state) {
          case TOUCHPAD_FSM_START:
               if (touchpad_finger_landing( levt )) {
                    state->fsm_state = TOUCHPAD_FSM_MAIN;
                    state->timeout   = levt_time;

                    timeout_add( &state->timeout, &timeout );
               }
               return 0;

          case TOUCHPAD_FSM_MAIN:
               if (touchpad_finger_moving( levt )) {
                    return touchpad_translate( state, touch_abs, levt, devt );
               }
               else if (touchpad_finger_leaving( levt )) {
                    if (!timeout_passed( &state->timeout, &levt_time )) {
                         devt->flags     = DIEF_TIMESTAMP;
                         devt->timestamp = levt_time;
                         devt->type      = DIET_BUTTONPRESS;
                         devt->button    = DIBI_FIRST;

                         touchpad_fsm_init( state );

                         state->fsm_state = TOUCHPAD_FSM_DRAG_START;
                         state->timeout   = levt_time;

                         timeout_add( &state->timeout, &timeout );

                         return 1;
                    }
                    else {
                         touchpad_fsm_init( state );
                    }
               }
               return 0;

          case TOUCHPAD_FSM_DRAG_START:
               if (timeout_passed( &state->timeout, &levt_time )) {
                    devt->flags     = DIEF_TIMESTAMP;
                    devt->timestamp = state->timeout;
                    devt->type      = DIET_BUTTONRELEASE;
                    devt->button    = DIBI_FIRST;

                    touchpad_fsm_init( state );

                    return 1;
               }
               else {
                    if (touchpad_finger_landing( levt )) {
                         state->fsm_state = TOUCHPAD_FSM_DRAG_MAIN;
                         state->timeout   = levt_time;

                         timeout_add( &state->timeout, &timeout );
                    }
               }
               return 0;

          case TOUCHPAD_FSM_DRAG_MAIN:
               if (touchpad_finger_moving( levt )) {
                    return touchpad_translate( state, touch_abs, levt, devt );
               }
               else if (touchpad_finger_leaving( levt )) {
                    devt->flags     = DIEF_TIMESTAMP;
                    devt->timestamp = levt_time;
                    devt->type      = DIET_BUTTONRELEASE;
                    devt->button    = DIBI_FIRST;

                    touchpad_fsm_init( state );

                    return 1;
               }
               return 0;

          default:
               return 0;
     }

     return 0;
}

/**********************************************************************************************************************/

static DFBInputDeviceKeySymbol
keyboard_get_symbol( int                             code,
                     unsigned short                  value,
                     DFBInputDeviceKeymapSymbolIndex level )
{
     unsigned char type  = KTYP( value );
     unsigned char index = KVAL( value );
     int           base  = (level == DIKSI_BASE);

     switch (type) {
          case KT_FN:
               if (index < 20)
                    return DFB_FUNCTION_KEY(index + 1);
               break;
          case KT_LETTER:
          case KT_LATIN:
               switch (index) {
                    case 0x1c:
                         return DIKS_PRINT;
                    case 0x7f:
                         return DIKS_BACKSPACE;
                    case 0xa4:
                         return 0x20ac; /* euro currency sign */
                    default:
                         return index;
               }
               break;
          case KT_DEAD:
               switch (value) {
                    case K_DGRAVE:
                         return DIKS_DEAD_GRAVE;
                    case K_DACUTE:
                         return DIKS_DEAD_ACUTE;
                    case K_DCIRCM:
                         return DIKS_DEAD_CIRCUMFLEX;
                    case K_DTILDE:
                         return DIKS_DEAD_TILDE;
                    case K_DDIERE:
                         return DIKS_DEAD_DIAERESIS;
                    case K_DCEDIL:
                         return DIKS_DEAD_CEDILLA;
                    default:
                         break;
               }
               break;
          case KT_PAD:
               if (index <= 9 && level != DIKSI_BASE)
                    return DIKS_0 + index;
               break;
     }

     switch (value) {
          case K_LEFT:    return DIKS_CURSOR_LEFT;
          case K_RIGHT:   return DIKS_CURSOR_RIGHT;
          case K_UP:      return DIKS_CURSOR_UP;
          case K_DOWN:    return DIKS_CURSOR_DOWN;
          case K_ENTER:   return DIKS_ENTER;
          case K_CTRL:    return DIKS_CONTROL;
          case K_SHIFT:   return DIKS_SHIFT;
          case K_ALT:     return DIKS_ALT;
          case K_ALTGR:   return DIKS_ALTGR;
          case K_INSERT:  return DIKS_INSERT;
          case K_REMOVE:  return DIKS_DELETE;
          case K_FIND:    return DIKS_HOME;
          case K_SELECT:  return DIKS_END;
          case K_PGUP:    return DIKS_PAGE_UP;
          case K_PGDN:    return DIKS_PAGE_DOWN;
          case K_NUM:     return DIKS_NUM_LOCK;
          case K_HOLD:    return DIKS_SCROLL_LOCK;
          case K_PAUSE:   return DIKS_PAUSE;
          case K_BREAK:   return DIKS_BREAK;
          case K_CAPS:    return DIKS_CAPS_LOCK;
          case K_P0:      return DIKS_INSERT;
          case K_P1:      return DIKS_END;
          case K_P2:      return DIKS_CURSOR_DOWN;
          case K_P3:      return DIKS_PAGE_DOWN;
          case K_P4:      return DIKS_CURSOR_LEFT;
          case K_P5:      return DIKS_BEGIN;
          case K_P6:      return DIKS_CURSOR_RIGHT;
          case K_P7:      return DIKS_HOME;
          case K_P8:      return DIKS_CURSOR_UP;
          case K_P9:      return DIKS_PAGE_UP;
          case K_PPLUS:   return DIKS_PLUS_SIGN;
          case K_PMINUS:  return DIKS_MINUS_SIGN;
          case K_PSTAR:   return DIKS_ASTERISK;
          case K_PSLASH:  return DIKS_SLASH;
          case K_PENTER:  return DIKS_ENTER;
          case K_PCOMMA:  return base ? DIKS_DELETE : DIKS_COMMA;
          case K_PDOT:    return base ? DIKS_DELETE : DIKS_PERIOD;
          case K_PPARENL: return DIKS_PARENTHESIS_LEFT;
          case K_PPARENR: return DIKS_PARENTHESIS_RIGHT;
     }

     /* Special keys not in the map. */

     if (code == 99) /* print key */
          return DIKS_PRINT;

     if (code == 124) /* keypad equal key */
          return DIKS_EQUALS_SIGN;

     if (code == 125) /* left windows key */
          return DIKS_META;

     if (code == 126) /* right windows key */
          return DIKS_META;

     if (code == 127) /* context menu key */
          return DIKS_SUPER;

     return DIKS_NULL;
}

static DFBInputDeviceKeyIdentifier
keyboard_get_identifier( int            code,
                         unsigned short value )
{
     unsigned char type  = KTYP( value );
     unsigned char index = KVAL( value );

     if (type == KT_PAD) {
          if (index <= 9)
               return DIKI_KP_0 + index;

          switch (value) {
               case K_PSLASH: return DIKI_KP_DIV;
               case K_PSTAR:  return DIKI_KP_MULT;
               case K_PMINUS: return DIKI_KP_MINUS;
               case K_PPLUS:  return DIKI_KP_PLUS;
               case K_PENTER: return DIKI_KP_ENTER;
               case K_PCOMMA:
               case K_PDOT:   return DIKI_KP_DECIMAL;
          }
     }

     switch (code) {
          case 12:  return DIKI_MINUS_SIGN;
          case 13:  return DIKI_EQUALS_SIGN;
          case 26:  return DIKI_BRACKET_LEFT;
          case 27:  return DIKI_BRACKET_RIGHT;
          case 39:  return DIKI_SEMICOLON;
          case 40:  return DIKI_QUOTE_RIGHT;
          case 41:  return DIKI_QUOTE_LEFT;
          case 43:  return DIKI_BACKSLASH;
          case 51:  return DIKI_COMMA;
          case 52:  return DIKI_PERIOD;
          case 53:  return DIKI_SLASH;
          case 54:  return DIKI_SHIFT_R;
          case 97:  return DIKI_CONTROL_R;
          case 100: return DIKI_ALT_R;
          case 124: return DIKI_KP_EQUAL;
          case 125: return DIKI_META_L;
          case 126: return DIKI_META_R;
          case 127: return DIKI_SUPER_R;
     }

     return DIKI_UNKNOWN;
}

static unsigned short
keyboard_read_value( const LinuxInputData *data,
                     unsigned char         table,
                     unsigned char         index )
{
     struct kbentry entry;

     entry.kb_table = table;
     entry.kb_index = index;
     entry.kb_value = 0;

     if (ioctl( data->vt_fd, KDGKBENT, &entry )) {
          D_PERROR( "Input/Linux: KDGKBENT( table %d, index %d ) failed!\n", table, index );
          return 0;
     }

     return entry.kb_value;
}

/**********************************************************************************************************************/

/*
 * Translate a Linux input keycode into a DirectFB keycode.
 */
static int
key_translate( unsigned short code )
{
     if (code < D_ARRAY_SIZE(basic_keycodes))
          return basic_keycodes[code];

     if (code >= KEY_OK)
          if (code - KEY_OK < D_ARRAY_SIZE(ext_keycodes))
               return ext_keycodes[code-KEY_OK];

     return DIKI_UNKNOWN;
}

/*
 * Translate key and button events.
 */
static bool
key_event( const struct input_event *levt,
           DFBInputEvent            *devt )
{
     int code = levt->code;

     /* Map touchscreen events to button mouse. */
     if (code == BTN_TOUCH || code == BTN_TOOL_FINGER)
          code = BTN_MOUSE;

     if ((code >= BTN_MOUSE && code < BTN_JOYSTICK) || code == BTN_TOUCH) {
          /* Ignore repeat events for buttons. */
          if (levt->value == 2)
               return false;

          devt->type   = levt->value ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
          devt->button = DIBI_FIRST + code - BTN_MOUSE;
     }
     else {
          int key = key_translate( code );

          if (key == DIKI_UNKNOWN)
               return false;

          devt->type = levt->value ? DIET_KEYPRESS : DIET_KEYRELEASE;

          if (DFB_KEY_TYPE( key ) == DIKT_IDENTIFIER) {
               devt->key_id  = key;
               devt->flags  |= DIEF_KEYID;
          }
          else {
               devt->key_symbol  = key;
               devt->flags      |= DIEF_KEYSYMBOL;
          }

          devt->key_code  = code;
          devt->flags    |= DIEF_KEYCODE;
     }

     if (levt->value == 2)
          devt->flags |= DIEF_REPEAT;

     return true;
}

/*
 * Translate relative axis events.
 */
static bool
rel_event( const LinuxInputData     *data,
           const struct input_event *levt,
           DFBInputEvent            *devt )
{
     switch (levt->code) {
          case REL_X:
               devt->axis = DIAI_X;
               devt->axisrel = levt->value * data->sensitivity / 0x100;
               break;

          case REL_Y:
               devt->axis = DIAI_Y;
               devt->axisrel = levt->value * data->sensitivity / 0x100;
               break;

          case REL_Z:
          case REL_WHEEL:
               devt->axis = DIAI_Z;
               devt->axisrel = -levt->value;
               break;

          default:
               if (levt->code > REL_MAX || levt->code > DIAI_LAST)
                    return false;
               devt->axis = levt->code;
               devt->axisrel = levt->value;
     }

     devt->type   = DIET_AXISMOTION;
     devt->flags |= DIEF_AXISREL;

     return true;
}

/*
 * Translate absolute axis events.
 */
static bool
abs_event( const struct input_event *levt,
           DFBInputEvent            *devt )
{
     switch (levt->code) {
          case ABS_X:
               devt->axis = DIAI_X;
               break;

          case ABS_Y:
               devt->axis = DIAI_Y;
               break;

          case ABS_Z:
          case ABS_WHEEL:
               devt->axis = DIAI_Z;
               break;

          default:
               if (levt->code >= ABS_PRESSURE || levt->code > DIAI_LAST)
                    return false;
               devt->axis = levt->code;
     }

     devt->type     = DIET_AXISMOTION;
     devt->flags   |= DIEF_AXISABS;
     devt->axisabs  = levt->value;

     return true;
}

/*
 * Translate a Linux input event into a DirectFB input event.
 */
static bool
translate_event( const LinuxInputData     *data,
                 const struct input_event *levt,
                 DFBInputEvent            *devt )
{
     devt->flags             = DIEF_TIMESTAMP;
     devt->timestamp.tv_sec  = levt->input_event_sec;
     devt->timestamp.tv_usec = levt->input_event_usec;

     switch (levt->type) {
          case EV_KEY:
               return key_event( levt, devt );

          case EV_REL:
               return rel_event( data, levt, devt );

          case EV_ABS:
               return abs_event( levt, devt );

          default:
               ;
     }

     return false;
}

/**********************************************************************************************************************/

static void
set_led( const LinuxInputData *data,
         int                   led,
         int                   state )
{
     struct input_event levt;
     int                res;

     D_UNUSED_P( res );

     memset( &levt, 0, sizeof(levt) );
     levt.type  = EV_LED;
     levt.code  = led;
     levt.value = state;

     res = write( data->fd, &levt, sizeof(levt) );
}

static void
flush_xy( LinuxInputData *data,
          bool            last )
{
     DFBInputEvent devt = { .type = DIET_UNKNOWN };

     if (data->dx) {
          devt.type    = DIET_AXISMOTION;
          devt.flags   = DIEF_AXISREL;
          devt.axis    = DIAI_X;
          devt.axisrel = data->dx;

          /* Signal immediately following event. */
          if (!last || data->dy)
               devt.flags |= DIEF_FOLLOW;

          dfb_input_dispatch( data->device, &devt );

          data->dx = 0;
     }

     if (data->dy) {
          devt.type    = DIET_AXISMOTION;
          devt.flags   = DIEF_AXISREL;
          devt.axis    = DIAI_Y;
          devt.axisrel = data->dy;

          /* Signal immediately following event. */
          if (!last)
               devt.flags |= DIEF_FOLLOW;

          dfb_input_dispatch( data->device, &devt );

          data->dy = 0;
     }
}

/**********************************************************************************************************************/

static void *
devinput_event_thread( DirectThread *thread,
                       void         *arg )
{
     LinuxInputData            *data = arg;
     int                        status;
     int                        fdmax;
     unsigned int               i;
     bool                       mouse_motion_compression;
     struct touchpad_fsm_state  fsm_state;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     /* Mouse motion event compression. */
     if (direct_config_has_name( "motion-compression" ) && !direct_config_has_name( "no-motion-compression" ))
          mouse_motion_compression = true;
     else
          mouse_motion_compression = false;

     fdmax = MAX( data->fd, data->quitpipe[0] );

     /* Query the touchpad min/max coordinates. */
     if (data->touchpad) {
          struct input_absinfo absinfo;

          touchpad_fsm_init( &fsm_state );

          ioctl( data->fd, EVIOCGABS( ABS_X ), &absinfo );
          fsm_state.x.min = absinfo.minimum;
          fsm_state.x.max = absinfo.maximum;

          ioctl( data->fd, EVIOCGABS( ABS_Y ), &absinfo );
          fsm_state.y.min = absinfo.minimum;
          fsm_state.y.max = absinfo.maximum;
     }

     /* Query the keys. */
     if (data->has_keys) {
          unsigned long keybit[NBITS(KEY_CNT)];
          unsigned long keystate[NBITS(KEY_CNT)];

          /* Get key bits. */
          ioctl( data->fd, EVIOCGBIT( EV_KEY, sizeof(keybit) ), keybit );

          /* Get key states. */
          ioctl( data->fd, EVIOCGKEY( sizeof(keystate) ), keystate );

          /* For each key, synthetize a press or release event depending on the key state. */
          for (i = 0; i <= KEY_CNT; i++) {
               if (test_bit( i, keybit )) {
                    const int key = key_translate( i );

                    if (DFB_KEY_TYPE( key ) == DIKT_IDENTIFIER) {
                         DFBInputEvent devt;

                         devt.type     = test_bit( i, keystate ) ? DIET_KEYPRESS : DIET_KEYRELEASE;
                         devt.flags    = DIEF_KEYID | DIEF_KEYCODE;
                         devt.key_id   = key;
                         devt.key_code = i;

                         dfb_input_dispatch( data->device, &devt );
                    }
               }
          }
     }

     while (1) {
          DFBInputEvent devt = { .type = DIET_UNKNOWN };
          fd_set        set;
          ssize_t       len;

          /* Get input event. */
          FD_ZERO( &set );
          FD_SET( data->fd, &set );
          FD_SET( data->quitpipe[0], &set );

          if (data->touchpad && timeout_is_set( &fsm_state.timeout )) {
               struct timeval time;
               gettimeofday( &time, NULL );

               if (!timeout_passed( &fsm_state.timeout, &time )) {
                    struct timeval timeout = fsm_state.timeout;

                    timeout_sub( &timeout, &time );

                    status = select( fdmax + 1, &set, NULL, NULL, &timeout );
               }
               else
                    status = 0;
          }
          else
               status = select( fdmax + 1, &set, NULL, NULL, NULL );

          if (status < 0 && errno != EINTR)
               break;

          if (status > 0 && FD_ISSET( data->quitpipe[0], &set ))
               break;

          if (status < 0)
               continue;

          /* Check timeout. */
          if (status == 0) {
               if (data->touchpad && touchpad_fsm( &fsm_state, data->touch_abs, NULL, &devt ) > 0)
                    dfb_input_dispatch( data->device, &devt );

               continue;
          }

          len = read( data->fd, input_events, sizeof(input_events) );
          if (len < 0 && errno != EINTR)
               break;

          if (len <= 0)
               continue;

          for (i = 0; i < len / sizeof(input_events[0]); i++) {
               DFBInputEvent evt = { .type = DIET_UNKNOWN };

               if (data->touchpad) {
                    status = touchpad_fsm( &fsm_state, data->touch_abs, &input_events[i], &evt );
                    if (status < 0) {
                         /* Not handled. Try the direct approach. */
                         if (!translate_event( data, &input_events[i], &evt ))
                              continue;
                    }
                    else if (status == 0) {
                         /* Handled but no further processing is necessary. */
                         continue;
                    }
               }
               else {
                    if (!translate_event( data, &input_events[i], &evt ))
                         continue;
               }

               /* Flush previous event with DIEF_FOLLOW. */
               if (devt.type != DIET_UNKNOWN) {
                    flush_xy( data, false );

                    /* Signal immediately following event. */
                    devt.flags |= DIEF_FOLLOW;

                    dfb_input_dispatch( data->device, &devt );

                    if (data->has_leds && (devt.locks != data->locks)) {
                         set_led( data, LED_SCROLLL, devt.locks & DILS_SCROLL );
                         set_led( data, LED_NUML,    devt.locks & DILS_NUM );
                         set_led( data, LED_CAPSL,   devt.locks & DILS_CAPS );
                         data->locks = devt.locks;
                    }

                    devt.type  = DIET_UNKNOWN;
                    devt.flags = DIEF_NONE;
               }

               devt = evt;

               if (D_FLAGS_IS_SET( devt.flags, DIEF_AXISREL ) &&
                   devt.type == DIET_AXISMOTION               &&
                   mouse_motion_compression) {
                    switch (devt.axis) {
                         case DIAI_X:
                              data->dx += devt.axisrel;
                              continue;

                         case DIAI_Y:
                              data->dy += devt.axisrel;
                              continue;

                         default:
                              break;
                    }
               }
          }

          /* Flush last event without DIEF_FOLLOW. */
          if (devt.type != DIET_UNKNOWN) {
               flush_xy( data, false );

               /* Event is dispatched. */
               dfb_input_dispatch( data->device, &devt );

               if (data->has_leds && (devt.locks != data->locks)) {
                    set_led( data, LED_SCROLLL, devt.locks & DILS_SCROLL );
                    set_led( data, LED_NUML,    devt.locks & DILS_NUM );
                    set_led( data, LED_CAPSL,   devt.locks & DILS_CAPS );
                    data->locks = devt.locks;
               }
          }
          else
               flush_xy( data, true );
     }

     D_DEBUG_AT( Linux_Input, "DevInput Event thread terminated\n" );

     return NULL;
}

static void
get_device_info( int              fd,
                 InputDeviceInfo *device_info,
                 bool            *touchpad )
{
     int             i;
     unsigned int    num_keys     = 0;
     unsigned int    num_ext_keys = 0;
     unsigned int    num_buttons  = 0;
     unsigned int    num_rels     = 0;
     unsigned int    num_abs      = 0;
     unsigned long   evbit[NBITS(EV_CNT)];
     unsigned long   keybit[NBITS(KEY_CNT)];
     unsigned long   relbit[NBITS(REL_CNT)];
     unsigned long   absbit[NBITS(ABS_CNT)];
     struct input_id devinfo;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     device_info->desc.caps = 0;

     /* Get device name. */
     ioctl( fd, EVIOCGNAME( DFB_INPUT_DEVICE_DESC_NAME_LENGTH - 1 ), device_info->desc.name );

     D_DEBUG_AT( Linux_Input, "  -> name '%s'\n", device_info->desc.name );

     /* Set device vendor. */
     snprintf( device_info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Linux" );

     /* Get event type bits. */
     ioctl( fd, EVIOCGBIT( 0, sizeof(evbit) ), evbit );

     if (test_bit( EV_KEY, evbit )) {
          /* Get keyboard bits. */
          ioctl( fd, EVIOCGBIT( EV_KEY, sizeof(keybit) ), keybit );

          /* Count typical keyboard keys only. */
          for (i = KEY_Q; i <= KEY_M; i++)
               if (test_bit( i, keybit ))
                    num_keys++;

          /* This might be a keyboard with just cursor keys (typically on front panels), handle as remote control. */
          if (!num_keys)
               for (i = KEY_HOME; i <= KEY_PAGEDOWN; i++)
                    if (test_bit( i, keybit ))
                         num_ext_keys++;

          for (i = KEY_OK; i < KEY_CNT; i++)
               if (test_bit( i, keybit ))
                    num_ext_keys++;

          for (i = BTN_MOUSE; i < BTN_JOYSTICK; i++)
               if (test_bit( i, keybit ))
                    num_buttons++;

          if (num_keys || num_ext_keys)
               device_info->desc.caps |= DICAPS_KEYS;
     }

     if (test_bit( EV_REL, evbit )) {
          /* Get bits for relative axes. */
          ioctl( fd, EVIOCGBIT( EV_REL, sizeof(relbit) ), relbit );

          for (i = 0; i < REL_CNT; i++)
               if (test_bit( i, relbit ))
                    num_rels++;
     }

     if (test_bit( EV_ABS, evbit )) {
          /* Get bits for absolute axes. */
          ioctl( fd, EVIOCGBIT( EV_ABS, sizeof(absbit) ), absbit );

          for (i = 0; i < ABS_PRESSURE; i++)
               if (test_bit( i, absbit ))
                    num_abs++;
     }

     /* Touchpad */
     if (test_bit( EV_KEY, evbit ) &&
         test_bit( BTN_TOUCH, keybit ) &&
         test_bit( BTN_TOOL_FINGER, keybit) &&
         test_bit( EV_ABS, evbit ) &&
         test_bit( ABS_X, absbit ) &&
         test_bit( ABS_Y, absbit ) &&
         test_bit( ABS_PRESSURE, absbit ))
          *touchpad = true;
     else
          *touchpad = false;

     device_info->desc.type = DIDTF_NONE;

     /* Mouse, Touchscreen or Joystick */
     if ((test_bit( EV_KEY, evbit ) && (test_bit( BTN_TOUCH, keybit ) || test_bit( BTN_TOOL_FINGER, keybit ))) ||
         ((num_rels >= 2 && num_buttons) || (num_abs == 2 && num_buttons == 1)))
          device_info->desc.type |= DIDTF_MOUSE;
     else if (num_abs && num_buttons)
          device_info->desc.type |= DIDTF_JOYSTICK;

     /* Keyboard */
     if (num_keys > 20) {
          device_info->desc.type |= DIDTF_KEYBOARD;

          device_info->desc.min_keycode = 0;
          device_info->desc.max_keycode = 127;
     }
     else
          device_info->desc.min_keycode = device_info->desc.max_keycode = 0;

     /* Remote Control */
     if (num_ext_keys) {
          device_info->desc.type |= DIDTF_REMOTE;
     }

     /* Buttons */
     if (num_buttons) {
          device_info->desc.caps       |= DICAPS_BUTTONS;
          device_info->desc.max_button  = DIBI_FIRST + num_buttons - 1;
     }
     else
          device_info->desc.max_button = 0;

     /* Axes */
     if (num_rels || num_abs) {
          device_info->desc.caps     |= DICAPS_AXES;
          device_info->desc.max_axis  = DIAI_FIRST + MAX( num_rels, num_abs ) - 1;
     }
     else
          device_info->desc.max_axis = 0;

     /* Primary input device */
     if (device_info->desc.type & DIDTF_KEYBOARD)
          device_info->prefered_id = DIDID_KEYBOARD;
     else if (device_info->desc.type & DIDTF_REMOTE)
          device_info->prefered_id = DIDID_REMOTE;
     else if (device_info->desc.type & DIDTF_JOYSTICK)
          device_info->prefered_id = DIDID_JOYSTICK;
     else if (device_info->desc.type & DIDTF_MOUSE)
          device_info->prefered_id = DIDID_MOUSE;
     else
          device_info->prefered_id = DIDID_ANY;

     /* Get VID and PID information. */
     ioctl( fd, EVIOCGID, &devinfo );

     device_info->desc.vendor_id  = devinfo.vendor;
     device_info->desc.product_id = devinfo.product;

     D_DEBUG_AT( Linux_Input, "  -> ids %d/%d\n", device_info->desc.vendor_id, device_info->desc.product_id );
}

static bool
check_device( const char *device )
{
     int err, fd;
     InputDeviceInfo device_info;
     bool            touchpad;
     bool            linux_input_grab;
     bool            linux_input_ir_only;

     D_DEBUG_AT( Linux_Input, "%s( '%s' )\n", __FUNCTION__, device );

     /* Check if we are able to open the device. */
     fd = open( device, O_RDWR );
     if (fd < 0) {
          D_DEBUG_AT( Linux_Input, "  -> open failed!\n" );
          return false;
     }

     /* Grab device. */
     if (direct_config_has_name( "linux-input-grab" ) && !direct_config_has_name( "no-linux-input-grab" ))
          linux_input_grab = true;
     else
          linux_input_grab = false;

     /* Ignore non-IR device. */
     if (direct_config_has_name( "linux-input-ir-only" ) && !direct_config_has_name( "no-linux-input-ir-only" ))
          linux_input_ir_only = true;
     else
          linux_input_ir_only = false;

     if (linux_input_grab) {
          err = ioctl( fd, EVIOCGRAB, 1 );
          if (err) {
               D_PERROR( "Input/Linux: Could not grab device!\n" );
               close( fd );
               return false;
          }
     }

     /* Get device information. */
     get_device_info( fd, &device_info, &touchpad );

     if (linux_input_grab)
          ioctl( fd, EVIOCGRAB, 0 );

     close( fd );

     if (!device_info.desc.caps) {
         D_DEBUG_AT( Linux_Input, "  -> no caps!\n" );
         return false;
     }

     if (!linux_input_ir_only || (device_info.desc.type & DIDTF_REMOTE))
          return true;

     return false;
}

/**********************************************************************************************************************/

static int
driver_get_available()
{
     const char   *value;
     FusionVector  linux_input_devices;
     int           i;
     char         *skipdev;
     char          buf[32];

     if (num_devices) {
          for (i = 0; i < num_devices; i++) {
               D_FREE( device_names[i] );
               device_names[i] = NULL;
          }

          num_devices = 0;

          return 0;
     }

     /* Use the devices specified in the configuration. */
     if ((value = direct_config_get_value( "linux-input-devices" ))) {
          const char *device;

          fusion_vector_init( &linux_input_devices, 2, NULL );

          config_values_parse( &linux_input_devices, value );

          fusion_vector_foreach (device, i, linux_input_devices) {
               if (num_devices >= MAX_LINUX_INPUT_DEVICES)
                    break;

               /* Update the device_names and device_nums array entries too. */
               if (check_device( device )) {
                    D_ASSERT( device_names[num_devices] == NULL );
                    device_names[num_devices] = D_STRDUP( device );
                    device_nums[num_devices] = i;
                    num_devices++;
               }
          }

          config_values_free( &linux_input_devices );

          return num_devices;
     }

     /* No devices specified. Try to guess some, set SKIP_INPUT_DEVICE to skip checking the specified input device. */
     skipdev = getenv( "SKIP_INPUT_DEVICE" );

     for (i = 0; i < MAX_LINUX_INPUT_DEVICES; i++) {
          snprintf( buf, sizeof(buf), "/dev/input/event%d", i );

          /* Initialize device_names and device_nums array entries. */
          device_names[i] = NULL;
          device_nums[i]  = MAX_LINUX_INPUT_DEVICES;

          if (skipdev && !strcmp( skipdev, buf ))
               continue;

          /* Update the device_names and device_nums array entries too. */
          if (check_device( buf )) {
               D_ASSERT( device_names[num_devices] == NULL );
               device_names[num_devices] = D_STRDUP( buf );
               device_nums[num_devices] = i;
               num_devices++;
          }
     }

     return num_devices;
}

static void
driver_get_info( InputDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 1;

     snprintf( driver_info->name,   DFB_INPUT_DRIVER_INFO_NAME_LENGTH,   "Linux Input" );
     snprintf( driver_info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *device_info,
                    void            **driver_data )
{
     LinuxInputData *data;
     int             err, fd;
     bool            linux_input_grab;
     unsigned long   ledbit[NBITS(LED_CNT)];
     bool            touchpad;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     /* Open device. */
     fd = open( device_names[number], O_RDWR );
     if (fd < 0) {
          D_PERROR( "Input/Linux: Could not open device '%s'!\n", device_names[number] );
          return DFB_INIT;
     }

     /* Grab device. */
     if (direct_config_has_name( "linux-input-grab" ) && !direct_config_has_name( "no-linux-input-grab" ))
          linux_input_grab = true;
     else
          linux_input_grab = false;

     if (linux_input_grab) {
          err = ioctl( fd, EVIOCGRAB, 1 );
          if (err) {
               D_PERROR( "Input/Linux: Could not grab device!\n" );
               close( fd );
               return DFB_INIT;
          }
     }

     /* Fill device information. */
     get_device_info( fd, device_info, &touchpad );

     /* Allocate and fill private data. */
     data = D_CALLOC( 1, sizeof(LinuxInputData) );
     if (!data) {
          if (linux_input_grab)
               ioctl( fd, EVIOCGRAB, 0 );
          close( fd );
          return D_OOM();
     }

     data->device   = device;
     data->index    = number;
     data->fd       = fd;
     data->grab     = linux_input_grab;
     data->has_keys = (device_info->desc.caps & DICAPS_KEYS) != 0;

     /* Check if the device has LEDs. */
     err = ioctl( fd, EVIOCGBIT( EV_LED, sizeof(ledbit) ), ledbit );
     if (err < 0)
          D_PERROR( "Input/Linux: Could not get LEDs bits!" );
     else
          data->has_leds = test_bit( LED_SCROLLL, ledbit ) ||
                           test_bit( LED_NUML,    ledbit ) ||
                           test_bit( LED_CAPSL,   ledbit );

     if (data->has_leds) {
          /* Get LEDs state. */
          err = ioctl( fd, EVIOCGLED( sizeof(data->led_state) ), data->led_state );
          if (err < 0) {
               D_PERROR( "Input/Linux: Could not get LEDs state!" );
               goto error;
          }

          /* Turn off LEDs. */
          set_led( data, LED_SCROLLL, 0 );
          set_led( data, LED_NUML,    0 );
          set_led( data, LED_CAPSL,   0 );
     }

     data->touchpad = touchpad;

     if (data->touchpad) {
         if (direct_config_has_name( "linux-input-touch-abs" ) && !direct_config_has_name( "no-linux-input-touch-abs" ))
              data->touch_abs = true;
     }

     data->sensitivity = 0x100;

     if (device_info->desc.min_keycode >= 0 && device_info->desc.max_keycode > device_info->desc.min_keycode) {
          data->vt_fd = open( "/dev/tty0", O_RDWR | O_NOCTTY );

          if (data->vt_fd < 0)
               D_WARN( "no keymap support" );
     }

     /* Open a pipe to awake the devinput event thread when we want to quit. */
     err = pipe( data->quitpipe );
     if (err < 0) {
          D_PERROR( "Input/Linux: Could not open quit pipe!" );
          goto error;
     }

     /* Start devinput event thread. */
     data->thread = direct_thread_create( DTT_INPUT, devinput_event_thread, data, "DevInput Event" );

     *driver_data = data;

     return DFB_OK;

error:
     if (data->grab)
          ioctl( fd, EVIOCGRAB, 0 );
     if (data->vt_fd >= 0)
          close( data->vt_fd );
     close( fd );
     D_FREE( data );

     return DFB_INIT;
}

static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     LinuxInputData              *data = driver_data;
     int                          orig_mode;
     int                          code = entry->code;
     unsigned short               value;
     DFBInputDeviceKeyIdentifier  identifier;

     if (data->vt_fd < 0)
          return DFB_UNSUPPORTED;

     /* Save keyboard mode in order to restore it later. */
     if (ioctl( data->vt_fd, KDGKBMODE, &orig_mode ) < 0) {
          D_PERROR( "Input/Linux: KDGKBMODE failed!\n" );
          return DFB_INIT;
     }

     /* Switch to unicode mode to get the full keymap. */
     if (ioctl( data->vt_fd, KDSKBMODE, K_UNICODE ) < 0) {
          D_PERROR( "Input/Linux: K_UNICODE failed!\n" );
          return DFB_INIT;
     }

     /* Fetch the base level. */
     value = keyboard_read_value( driver_data, K_NORMTAB, code );

     /* Get the identifier for basic mapping. */
     identifier = keyboard_get_identifier( code, value );

     /* CapsLock is effective. */
     if (KTYP( value ) == KT_LETTER)
          entry->locks |= DILS_CAPS;

     /* NumLock is effective. */
     if (identifier >= DIKI_KP_DECIMAL && identifier <= DIKI_KP_9)
          entry->locks |= DILS_NUM;

     /* Write identifier to entry. */
     entry->identifier = identifier;

     /* Write base level symbol to entry. */
     entry->symbols[DIKSI_BASE] = keyboard_get_symbol( code, value, DIKSI_BASE );

     /* Fetch the shifted base level. */
     value = keyboard_read_value( driver_data, K_SHIFTTAB, entry->code );

     /* Write shifted base level symbol to entry. */
     entry->symbols[DIKSI_BASE_SHIFT] = keyboard_get_symbol( code, value, DIKSI_BASE_SHIFT );

     /* Fetch the alternative level. */
     value = keyboard_read_value( driver_data, K_ALTTAB, entry->code );

     /* Write alternative level symbol to entry. */
     entry->symbols[DIKSI_ALT] = keyboard_get_symbol( code, value, DIKSI_ALT );

     /* Fetch the shifted alternative level. */
     value = keyboard_read_value( driver_data, K_ALTSHIFTTAB, entry->code );

     /* Write shifted alternative level symbol to entry. */
     entry->symbols[DIKSI_ALT_SHIFT] = keyboard_get_symbol( code, value, DIKSI_ALT_SHIFT );

     /* Switch back to original keyboard mode. */
     if (ioctl( data->vt_fd, KDSKBMODE, orig_mode ) < 0) {
          D_PERROR( "Input/Linux: KDSKBMODE failed!\n" );
          return DFB_INIT;
     }

     return DFB_OK;
}

static void
driver_close_device( void *driver_data )
{
     LinuxInputData *data = driver_data;
     ssize_t         res;

     D_UNUSED_P( res );

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( data != NULL );

     /* Write to the quit pipe to terminate the devinput event thread. */
     res = write( data->quitpipe[1], " ", 1 );

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     close( data->quitpipe[0] );
     close( data->quitpipe[1] );

     /* Restore LEDs state. */
     if (data->has_leds) {
          set_led( data, LED_SCROLLL, test_bit( LED_SCROLLL, data->led_state ) );
          set_led( data, LED_NUML,    test_bit( LED_NUML,    data->led_state ) );
          set_led( data, LED_CAPSL,   test_bit( LED_CAPSL,   data->led_state ) );
     }

     if (data->grab)
          ioctl( data->fd, EVIOCGRAB, 0 );

     if (data->vt_fd >= 0)
          close( data->vt_fd );

     close( data->fd );

     D_FREE( data );
}

/**********************************************************************************************************************
 ********************************* Hot-plug functions *****************************************************************
 **********************************************************************************************************************/

typedef struct {
     CoreDFB *core;
     void    *driver;
} HotplugThreadData;

/* Socket file descriptor for getting udev events. */
static int              socket_fd = 0;

/* Thread for managing devinput hot-plug. */
static DirectThread    *hotplug_thread = NULL;

/* Pipe file descriptor for terminating the devinput hot-plug thread. */
static int              hotplug_quitpipe[2];

/* Mutex for handling the driver suspended. */
static pthread_mutex_t  driver_suspended_lock;

/* Flag for indicating that the driver is suspended. */
static bool             driver_suspended = false;

/*
 * Register /dev/input/eventX device node into the driver. Called when a new device node is created.
 */
static DFBResult
register_device_node( int  event_num,
                      int *index )
{
     int  i;
     char buf[32];

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( index != NULL );

     for (i = 0; i < MAX_LINUX_INPUT_DEVICES; i++) {
          if (device_nums[i] == MAX_LINUX_INPUT_DEVICES) {
               device_nums[i] = event_num;
               *index = i;
               num_devices++;

               snprintf( buf, sizeof(buf), "/dev/input/event%d", event_num );

               D_ASSERT( device_names[i] == NULL );
               device_names[i] = D_STRDUP( buf );

               return DFB_OK;
          }
     }

     /* Too many input devices plugged in to be handled. */
     D_DEBUG_AT( Linux_Input, "  -> the amount of devices registered exceeds the limit %d\n", MAX_LINUX_INPUT_DEVICES );

     return DFB_UNSUPPORTED;
}

/*
 * Unregister /dev/input/eventX device node from the driver. Called when a new device node is removed.
 */
static DFBResult
unregister_device_node( int  event_num,
                        int *index )
{
     int i;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( index != NULL );

     for (i = 0; i < num_devices; i++) {
          if (device_nums[i] == event_num) {
               device_nums[i] = MAX_LINUX_INPUT_DEVICES;
               num_devices--;
               *index = i;

               D_FREE( device_names[i] );
               device_names[i] = NULL;

               return DFB_OK;
          }
     }

     return DFB_UNSUPPORTED;
}

static void *
devinput_hotplug_thread( DirectThread *thread,
                         void         *arg )
{
     HotplugThreadData  *data = arg;
     int                 status;
     int                 fdmax;
     struct sockaddr_un  sock_addr;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( data != NULL );
     D_ASSERT( data->core != NULL );
     D_ASSERT( data->driver != NULL );

     /* Open and bind the socket /org/kernel/udev/monitor */
     socket_fd = socket( AF_UNIX, SOCK_DGRAM, 0 );
     if (socket_fd == -1)
          goto error;

     memset( &sock_addr, 0, sizeof(sock_addr) );
     sock_addr.sun_family = AF_UNIX;
     strncpy( &sock_addr.sun_path[1], "/org/kernel/udev/monitor", sizeof(sock_addr.sun_path) - 1 );

     status = bind( socket_fd, (struct sockaddr*) &sock_addr,
                    sizeof(sock_addr.sun_family) + 1 + strlen( &sock_addr.sun_path[1] ) );
     if (status < 0)
          goto error;

     fdmax = MAX( socket_fd, hotplug_quitpipe[0] );

     while (1) {
          DFBResult  ret;
          fd_set     set;
          char       udev_event[1024];
          ssize_t    len;
          char      *pos;
          char      *event_content;
          int        device_num, index;

          /* Get udev event. */
          FD_ZERO( &set );
          FD_SET( socket_fd, &set );
          FD_SET( hotplug_quitpipe[0], &set );

          status = select( fdmax + 1, &set, NULL, NULL, NULL );
          if (status < 0 && errno != EINTR)
               break;

          if (FD_ISSET( hotplug_quitpipe[0], &set ))
               break;

          if (FD_ISSET( socket_fd, &set )) {
               len = recv( socket_fd, udev_event, sizeof(udev_event), 0 );
               if (len <= 0) {
                    D_DEBUG_AT( Linux_Input, "Error receiving uevent message\n" );
                    continue;
               }
          }

          /* Analyze udev event. */
          pos = strchr( udev_event, '@' );
          if (pos == NULL)
               continue;

          /* Replace '@' with '\0' to separate event type and event content. */
          *pos = '\0';

          event_content = pos + 1;

          pos = strstr( event_content, "/event" );
          if (pos == NULL)
               continue;

          /* Get input device number. */
          device_num = atoi( pos + 6 );

          /* Attempt to lock the driver suspended mutex. */
          pthread_mutex_lock( &driver_suspended_lock );
          if (driver_suspended) {
               /* Release the lock and stop udev event handling. */
               D_DEBUG_AT( Linux_Input, "Driver is suspended, no udev processing\n" );
               pthread_mutex_unlock( &driver_suspended_lock );
               continue;
          }

          /* Handle udev event since the driver is not suspended. */
          if (!strcmp( udev_event, "add" )) {
               D_DEBUG_AT( Linux_Input, "Device node /dev/input/event%d is created by udev\n", device_num );

               ret = register_device_node( device_num, &index );
               if (ret == DFB_OK) {
                    /* Handle the input device node creation. */
                    ret = dfb_input_create_device( index, data->core, data->driver );
                    if (ret != DFB_OK) {
                         D_DEBUG_AT( Linux_Input, "Failed to create the device for /dev/input/event%d\n", device_num );
                    }
               }
          }
          else if (!strcmp( udev_event, "remove" )) {
               D_DEBUG_AT( Linux_Input, "Device node /dev/input/event%d is removed by udev\n", device_num );

               ret = unregister_device_node( device_num, &index );
               if (ret == DFB_OK) {
                    /* Handle the input device node removal. */
                    ret = dfb_input_remove_device( index, data->driver );
                    if (ret != DFB_OK) {
                         D_DEBUG_AT( Linux_Input, "Failed to remove the device for /dev/input/event%d\n", device_num );
                    }
               }
          }

          /* udev event handling is complete so release the lock. */
          pthread_mutex_unlock( &driver_suspended_lock );
     }

     D_FREE( data );

     D_DEBUG_AT( Linux_Input, "DevInput Hotplug thread terminated\n" );

     return NULL;

error:
     D_DEBUG_AT( Linux_Input, "Failed to open/bind udev socket\n" );

     if (socket_fd > 0) {
          close( socket_fd );
          socket_fd = 0;
     }

     return NULL;
}

static DFBResult
driver_suspend()
{
     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     if (pthread_mutex_lock( &driver_suspended_lock ))
          return DFB_FAILURE;

     /* Enter the suspended state by setting the driver_suspended flag to prevent handling of hot-plug events. */
     driver_suspended = true;

     pthread_mutex_unlock( &driver_suspended_lock );

     return DFB_OK;
}

static DFBResult
driver_resume()
{
     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     if (pthread_mutex_lock( &driver_suspended_lock) )
          return DFB_FAILURE;

     /* Leave the suspended state by clearing the driver_suspended flag which will allow hot-plug events to be handled
        again. */
     driver_suspended = false;

     pthread_mutex_unlock( &driver_suspended_lock );

     return DFB_OK;
}

static DFBResult
is_created( int   index,
            void *driver_data )
{
     LinuxInputData *data = driver_data;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( data != NULL );

     if (index < 0 || index >= MAX_LINUX_INPUT_DEVICES) {
          return DFB_UNSUPPORTED;
     }

     /* Check if the index is associated with an entry in the device_nums and device_names arrays. */
     if (index != data->index) {
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static InputDriverCapability
get_capability()
{
     InputDriverCapability capabilities = IDC_NONE;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );
     D_DEBUG_AT( Linux_Input, "  -> returning HOTPLUG\n" );

     capabilities |= IDC_HOTPLUG;

     return capabilities;
}

static DFBResult
launch_hotplug( CoreDFB *core,
                void    *input_driver )
{
     DFBResult          ret;
     HotplugThreadData *data;
     int                err;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_ASSERT( input_driver != NULL );
     D_ASSERT( hotplug_thread == NULL );

     data = D_CALLOC( 1, sizeof(HotplugThreadData) );
     if (!data) {
          ret = D_OOM();
          goto error;
     }

     data->core   = core;
     data->driver = input_driver;

     /* Open a pipe to awake the devinput hot-plug thread when we want to quit. */
     err = pipe( hotplug_quitpipe );
     if (err < 0) {
          D_PERROR( "Input/Linux: Could not open quit pipe for hot-plug!" );
          D_FREE( data );
          ret = DFB_INIT;
          goto error;
     }

     /* Initialize a mutex used to communicate with the devinput hot-plug thread when the driver is suspended. */
     pthread_mutex_init( &driver_suspended_lock, NULL );

     /* Start devinput hot-plug thread. */
     hotplug_thread = direct_thread_create( DTT_INPUT, devinput_hotplug_thread, data, "DevInput Hotplug" );
     if (!hotplug_thread) {
          pthread_mutex_destroy( &driver_suspended_lock );
          D_FREE( data );
          ret = DFB_UNSUPPORTED;
          goto error;
     }

     D_DEBUG_AT( Linux_Input, "  -> hot-plug detection enabled\n" );

     return DFB_OK;

error:
     D_DEBUG_AT( Linux_Input, "  -> failed to enable hot-plug detection\n" );

     return ret;
}

static DFBResult
stop_hotplug()
{
     int     err;
     ssize_t res;

     D_UNUSED_P( res );

     D_ASSERT( hotplug_thread != NULL );

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     /* The devinput hot-plug thread is not created. */
     if (!hotplug_thread)
          return DFB_OK;

     /* Write to the hot-plug quit pipe to terminate the devinput hot-plug thread. */
     res = write( hotplug_quitpipe[1], " ", 1 );

     direct_thread_join( hotplug_thread );
     direct_thread_destroy( hotplug_thread );

     close( hotplug_quitpipe[0] );
     close( hotplug_quitpipe[1] );

     hotplug_thread = NULL;

     /* Destroy the suspended mutex. */
     pthread_mutex_destroy( &driver_suspended_lock );

     /* Shutdown the connection of the socket. */
     if (socket_fd > 0) {
          err = shutdown( socket_fd, SHUT_RDWR );
          if (err < 0) {
               D_PERROR( "Input/Linux: Failed to shutdown socket!\n" );
               return DFB_FAILURE;
          }

          close( socket_fd );
          socket_fd = 0;
     }

     return DFB_OK;
}

/**********************************************************************************************************************
 ********************************* Axis info function *****************************************************************
 **********************************************************************************************************************/

static DFBResult
driver_get_axis_info( CoreInputDevice              *device,
                      void                         *driver_data,
                      DFBInputDeviceAxisIdentifier  axis,
                      InputDeviceAxisInfo          *ret_info )
{
     LinuxInputData *data = driver_data;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     if (data->touchpad && !data->touch_abs)
          return DFB_OK;

     if (axis <= ABS_PRESSURE && axis < DIAI_LAST) {
          unsigned long absbit[NBITS(ABS_CNT)];

          /* Check if we have an absolute axis. */
          ioctl( data->fd, EVIOCGBIT( EV_ABS, sizeof(absbit) ), absbit );

          if (test_bit (axis, absbit)) {
               struct input_absinfo absinfo;

               if (ioctl( data->fd, EVIOCGABS( axis ), &absinfo ) == 0 &&
                   (absinfo.minimum || absinfo.maximum)) {
                    ret_info->flags   |= IDAIF_ABS_MIN | IDAIF_ABS_MAX;
                    ret_info->abs_min  = absinfo.minimum;
                    ret_info->abs_max  = absinfo.maximum;
               }
          }
     }

     return DFB_OK;
}

/**********************************************************************************************************************
 ********************************* Set configuration function *********************************************************
 **********************************************************************************************************************/

static DFBResult
driver_set_configuration( CoreInputDevice            *device,
                          void                       *driver_data,
                          const DFBInputDeviceConfig *config )
{
     LinuxInputData *data = driver_data;

     D_DEBUG_AT( Linux_Input, "%s()\n", __FUNCTION__ );

     if (config->flags & DIDCONF_SENSITIVITY)
          data->sensitivity = config->sensitivity;

     return DFB_OK;
}

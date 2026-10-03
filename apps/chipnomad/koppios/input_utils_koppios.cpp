/*
 * Key names and the default mapping for koppi-os.
 *
 * The eight buttons ChipNomad needs are mapped the way its desktop build maps
 * them (platforms/sdl2 + src/corelib/keymap.h), so the manual at
 * chipnomad.org/manual applies here unchanged:
 *
 *   d-pad          arrow keys
 *   EDIT  (A)      X
 *   OPT   (B)      Z
 *   PLAY  (START)  space
 *   SHIFT (SELECT) left shift
 *
 * Remapping is the key-mapping screen's job, and what it stores are the
 * scancodes this file names.
 */
#include <stdio.h>

#include "input_utils_koppios.h"
#include "app_settings.h"

/* Scancode set 1 make codes, as the kernel's getscan reports them. */
enum {
    SC_ESCAPE = 0x01,
    SC_1 = 0x02, SC_2 = 0x03, SC_3 = 0x04, SC_4 = 0x05, SC_5 = 0x06,
    SC_6 = 0x07, SC_7 = 0x08, SC_8 = 0x09, SC_9 = 0x0A, SC_0 = 0x0B,
    SC_MINUS = 0x0C, SC_EQUALS = 0x0D, SC_BACKSPACE = 0x0E, SC_TAB = 0x0F,
    SC_Q = 0x10, SC_W = 0x11, SC_E = 0x12, SC_R = 0x13, SC_T = 0x14,
    SC_Y = 0x15, SC_U = 0x16, SC_I = 0x17, SC_O = 0x18, SC_P = 0x19,
    SC_LBRACKET = 0x1A, SC_RBRACKET = 0x1B, SC_ENTER = 0x1C, SC_LCTRL = 0x1D,
    SC_A = 0x1E, SC_S = 0x1F, SC_D = 0x20, SC_F = 0x21, SC_G = 0x22,
    SC_H = 0x23, SC_J = 0x24, SC_K = 0x25, SC_L = 0x26,
    SC_SEMICOLON = 0x27, SC_APOSTROPHE = 0x28, SC_GRAVE = 0x29,
    SC_LSHIFT = 0x2A, SC_BACKSLASH = 0x2B,
    SC_Z = 0x2C, SC_X = 0x2D, SC_C = 0x2E, SC_V = 0x2F, SC_B = 0x30,
    SC_N = 0x31, SC_M = 0x32, SC_COMMA = 0x33, SC_PERIOD = 0x34, SC_SLASH = 0x35,
    SC_RSHIFT = 0x36, SC_KP_MULTIPLY = 0x37, SC_LALT = 0x38, SC_SPACE = 0x39,
    SC_CAPSLOCK = 0x3A,
    SC_F1 = 0x3B, SC_F2 = 0x3C, SC_F3 = 0x3D, SC_F4 = 0x3E, SC_F5 = 0x3F,
    SC_F6 = 0x40, SC_F7 = 0x41, SC_F8 = 0x42, SC_F9 = 0x43, SC_F10 = 0x44,
    SC_F11 = 0x57, SC_F12 = 0x58,

    /* 0xE0-prefixed */
    SC_UP    = 0x48 | KOPPIOS_KEY_E0,
    SC_DOWN  = 0x50 | KOPPIOS_KEY_E0,
    SC_LEFT  = 0x4B | KOPPIOS_KEY_E0,
    SC_RIGHT = 0x4D | KOPPIOS_KEY_E0,
    SC_HOME  = 0x47 | KOPPIOS_KEY_E0,
    SC_END   = 0x4F | KOPPIOS_KEY_E0,
    SC_PGUP  = 0x49 | KOPPIOS_KEY_E0,
    SC_PGDN  = 0x51 | KOPPIOS_KEY_E0,
    SC_INSERT = 0x52 | KOPPIOS_KEY_E0,
    SC_DELETE = 0x53 | KOPPIOS_KEY_E0,
    SC_RCTRL  = 0x1D | KOPPIOS_KEY_E0,
    SC_RALT   = 0x38 | KOPPIOS_KEY_E0,
};

static InputCode kb(int code) {
    InputCode c;
    c.deviceType = InputDeviceType::keyboard;
    c.code = code;
    return c;
}

static InputCode none() {
    InputCode c;
    c.deviceType = InputDeviceType::none;
    c.code = 0;
    return c;
}

void InputUtilsKoppiOS::initDefaultKeyMapping(AppSettings& settings) {
    settings.keyMapping.keyUp[0]    = kb(SC_UP);
    settings.keyMapping.keyDown[0]  = kb(SC_DOWN);
    settings.keyMapping.keyLeft[0]  = kb(SC_LEFT);
    settings.keyMapping.keyRight[0] = kb(SC_RIGHT);
    settings.keyMapping.keyEdit[0]  = kb(SC_X);
    settings.keyMapping.keyOpt[0]   = kb(SC_Z);
    settings.keyMapping.keyPlay[0]  = kb(SC_SPACE);
    settings.keyMapping.keyShift[0] = kb(SC_LSHIFT);

    /*
     * A second binding each for the keys that are awkward on a laptop: the
     * right-hand modifiers, and Enter for PLAY. Slot 2 stays free for the
     * user's own, which is what the key-mapping screen writes.
     */
    settings.keyMapping.keyUp[1]    = none();
    settings.keyMapping.keyDown[1]  = none();
    settings.keyMapping.keyLeft[1]  = none();
    settings.keyMapping.keyRight[1] = none();
    settings.keyMapping.keyEdit[1]  = kb(SC_ENTER);
    settings.keyMapping.keyOpt[1]   = kb(SC_BACKSPACE);
    settings.keyMapping.keyPlay[1]  = none();
    settings.keyMapping.keyShift[1] = kb(SC_RSHIFT);

    settings.keyMapping.keyUp[2]    = none();
    settings.keyMapping.keyDown[2]  = none();
    settings.keyMapping.keyLeft[2]  = none();
    settings.keyMapping.keyRight[2] = none();
    settings.keyMapping.keyEdit[2]  = none();
    settings.keyMapping.keyOpt[2]   = none();
    settings.keyMapping.keyPlay[2]  = none();
    settings.keyMapping.keyShift[2] = none();
}

const char* InputUtilsKoppiOS::getKeyName(InputCode input) {
    if (input.deviceType == InputDeviceType::none)
        return "---";
    if (input.deviceType != InputDeviceType::keyboard)
        return "???";

    /* Short enough for the key-mapping screen's 7-character field. */
    switch (input.code) {
    case SC_ESCAPE:     return "Escape";
    case SC_1:          return "1";
    case SC_2:          return "2";
    case SC_3:          return "3";
    case SC_4:          return "4";
    case SC_5:          return "5";
    case SC_6:          return "6";
    case SC_7:          return "7";
    case SC_8:          return "8";
    case SC_9:          return "9";
    case SC_0:          return "0";
    case SC_MINUS:      return "-";
    case SC_EQUALS:     return "=";
    case SC_BACKSPACE:  return "BkSpace";
    case SC_TAB:        return "Tab";
    case SC_Q:          return "Q";
    case SC_W:          return "W";
    case SC_E:          return "E";
    case SC_R:          return "R";
    case SC_T:          return "T";
    case SC_Y:          return "Y";
    case SC_U:          return "U";
    case SC_I:          return "I";
    case SC_O:          return "O";
    case SC_P:          return "P";
    case SC_LBRACKET:   return "[";
    case SC_RBRACKET:   return "]";
    case SC_ENTER:      return "Return";
    case SC_LCTRL:      return "LCtrl";
    case SC_A:          return "A";
    case SC_S:          return "S";
    case SC_D:          return "D";
    case SC_F:          return "F";
    case SC_G:          return "G";
    case SC_H:          return "H";
    case SC_J:          return "J";
    case SC_K:          return "K";
    case SC_L:          return "L";
    case SC_SEMICOLON:  return ";";
    case SC_APOSTROPHE: return "'";
    case SC_GRAVE:      return "`";
    case SC_LSHIFT:     return "LShift";
    case SC_BACKSLASH:  return "\\";
    case SC_Z:          return "Z";
    case SC_X:          return "X";
    case SC_C:          return "C";
    case SC_V:          return "V";
    case SC_B:          return "B";
    case SC_N:          return "N";
    case SC_M:          return "M";
    case SC_COMMA:      return ",";
    case SC_PERIOD:     return ".";
    case SC_SLASH:      return "/";
    case SC_RSHIFT:     return "RShift";
    case SC_KP_MULTIPLY: return "KP*";
    case SC_LALT:       return "LAlt";
    case SC_SPACE:      return "Space";
    case SC_CAPSLOCK:   return "CapsLk";
    case SC_F1:         return "F1";
    case SC_F2:         return "F2";
    case SC_F3:         return "F3";
    case SC_F4:         return "F4";
    case SC_F5:         return "F5";
    case SC_F6:         return "F6";
    case SC_F7:         return "F7";
    case SC_F8:         return "F8";
    case SC_F9:         return "F9";
    case SC_F10:        return "F10";
    case SC_F11:        return "F11";
    case SC_F12:        return "F12";
    case SC_UP:         return "Up";
    case SC_DOWN:       return "Down";
    case SC_LEFT:       return "Left";
    case SC_RIGHT:      return "Right";
    case SC_HOME:       return "Home";
    case SC_END:        return "End";
    case SC_PGUP:       return "PgUp";
    case SC_PGDN:       return "PgDn";
    case SC_INSERT:     return "Insert";
    case SC_DELETE:     return "Delete";
    case SC_RCTRL:      return "RCtrl";
    case SC_RALT:       return "RAlt";
    default: {
        /* An unnamed key still has to show as something the user can tell
         * apart from the next one. */
        static char buf[8];
        snprintf(buf, sizeof buf, "SC%03X", (unsigned) input.code);
        return buf;
    }
    }
}

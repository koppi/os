/*
 * InputUtils for raw PS/2 scancodes (syscall 26).
 */
#ifndef __INPUT_UTILS_KOPPIOS_H__
#define __INPUT_UTILS_KOPPIOS_H__

#include "input_utils.h"

/*
 * This port's keyboard code space. The kernel's getscan hands back a set-1
 * make/break code plus a flag saying whether it arrived with the 0xE0 prefix
 * (keyboard.h's KBD_RAW_*); the grey keys' codes collide with the numeric
 * keypad's, so the prefix has to stay part of the identity. Codes stored in
 * a key mapping -- and therefore written to chipnomad.ini -- are therefore
 * `scancode | KOPPIOS_KEY_E0` for those, plain scancodes for the rest.
 */
#define KOPPIOS_KEY_E0 0x100

class InputUtilsKoppiOS : public InputUtils {
  public:
    void initDefaultKeyMapping(AppSettings& settings) override;
    const char* getKeyName(InputCode input) override;
};

#endif // __INPUT_UTILS_KOPPIOS_H__

#pragma once

#include <io.h>
#include <types.h>

void keyboard_init();
uint8_t keyboard_enabled();
void keyboard_read_key();
char keyboard_get_lastkey();
void keyboard_invalidate_lastkey();
char getchar();
void gets(char *str, size_t size);


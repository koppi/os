#pragma once

#include <stdint.h>

void ac97_init(void);
int ac97_present(void);
uint32_t ac97_get_base(void);
void ac97_play_buffer(uint8_t* buf, uint32_t len);
void ac97_irq_handler();

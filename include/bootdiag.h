/**
 * @file bootdiag.h
 * @brief Early-boot progress beacon (see @ref bootdiag_sub).
 */
#pragma once

/** @brief Beep @p n low-pitched tones at a checkpoint sub-step when `bootdiag`
 *         is on the boot line; a no-op otherwise. Used to bisect a step that
 *         is too coarse for the numbered checkpoints (e.g. vmm_init). */
void bootdiag_sub(int n);

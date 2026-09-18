/**
 * @file graphics.h
 * @brief The microui-based desktop: the draw thread's per-frame entry point
 *        and the on-screen log buffer.
 */
#pragma once

/** @brief Draw the desktop windows and widgets for this frame. */
void paint_desktop();
/** @brief Initialise the microui context and its text-metrics callbacks. */
void mu();
/** @brief Append @p text to the on-screen scroll-back log. */
void write_log(char *text);

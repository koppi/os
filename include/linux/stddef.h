/**
 * @file stddef.h
 * @brief Freestanding @c stddef.h — just @c offsetof.
 */
#pragma once

/** @brief Byte offset of @c MEMBER within @c TYPE. */
#define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)

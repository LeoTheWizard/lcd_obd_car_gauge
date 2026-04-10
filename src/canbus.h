/**
 * @file canbus.h
 * @author Leo Walker
 * @date April 2026
 * @ref canbus.c for implementation details.
 */

#pragma once

// Global Miles Per Gallon variable, updated by core 1 and displayed on core 0.
extern float mpg;

/**
 * @brief Core 1 entry point.
 */
void core1_entry();
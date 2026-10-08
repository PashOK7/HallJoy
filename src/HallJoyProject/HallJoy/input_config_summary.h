#pragma once
#include <cstddef>

// Aggregate, key-free summary of the input configuration for the support log:
// global curve settings and how many bound keys have an inverted curve (an
// inverted key outputs full value at rest: "trigger always pressed", "stick
// moves the wrong way"). Counts only; never key codes or input values.
void InputConfig_Summary(char* text, std::size_t capacity) noexcept;

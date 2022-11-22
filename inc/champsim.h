#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <cstdint>
#include <exception>

namespace champsim
{
struct deadlock : public std::exception {
  const uint32_t which;
  explicit deadlock(uint32_t cpu) : which(cpu) {}
};

//#define DEBUG_PRINT
#ifdef DEBUG_PRINT
constexpr bool debug_print = true;
#else
constexpr bool debug_print = false;
#endif
} // namespace champsim

#endif

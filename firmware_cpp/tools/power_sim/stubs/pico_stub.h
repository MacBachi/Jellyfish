// Host-side stand-ins for the Pico SDK calls the LED, PWM and canvas code touches.
// Everything is a no-op except the clock, which power_sim.cpp advances by hand.
#pragma once
#include <cstdint>
#include <cstddef>
typedef unsigned int uint;
typedef int PIO;
#define pio0 0
#define pio1 1
#define pio2 2
#define NUM_PIOS 3
struct pio_sm_config { int dummy; };
struct pio_program { int dummy; };
extern const pio_program ws2812_program;
extern uint64_t g_now_us;
inline uint64_t time_us_64() { return g_now_us; }
inline void sleep_us(uint64_t) {}
inline void sleep_ms(uint64_t) {}
inline void pio_sm_put_blocking(PIO, uint, uint32_t) {}
inline uint pio_add_program(PIO, const pio_program*) { return 0; }
inline uint pio_get_index(PIO p) { return (uint)p; }
inline void pio_sm_claim(PIO, uint) {}
inline void pio_gpio_init(PIO, uint) {}
inline void pio_sm_set_consecutive_pindirs(PIO, uint, uint, uint, bool) {}
inline pio_sm_config ws2812_program_get_default_config(uint) { return {}; }
inline void sm_config_set_sideset_pins(pio_sm_config*, uint) {}
inline void sm_config_set_out_shift(pio_sm_config*, bool, bool, uint) {}
#define PIO_FIFO_JOIN_TX 1
inline void sm_config_set_fifo_join(pio_sm_config*, int) {}
#define clk_sys 5
inline uint32_t clock_get_hz(int) { return 150000000; }
inline void sm_config_set_clkdiv(pio_sm_config*, float) {}
inline void pio_sm_init(PIO, uint, uint, pio_sm_config*) {}
inline void pio_sm_set_enabled(PIO, uint, bool) {}
#define GPIO_FUNC_PWM 4
inline void gpio_set_function(uint, int) {}
inline uint pwm_gpio_to_slice_num(uint g) { return g / 2; }
inline void pwm_set_wrap(uint, uint16_t) {}
inline void pwm_set_enabled(uint, bool) {}
inline void pwm_set_gpio_level(uint, uint16_t) {}
inline void __dmb() {}

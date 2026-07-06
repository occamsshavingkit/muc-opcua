/* scripts/size_measure_startup.c — minimal Cortex-M0+ startup for linked-ELF
 * size measurement. Provides the vector table, Reset_Handler, and section
 * initialization so the linker can produce a complete ELF. */

#include <stdint.h>

extern int main(void);
void Reset_Handler(void);

extern uint8_t _sdata;
extern uint8_t _edata;
extern uint8_t _etext;
extern uint8_t _sbss;
extern uint8_t _ebss;
extern uint8_t _estack;

__attribute__((section(".vectors"), used)) void (*const vector_table[])(void) = {
    (void (*)(void))((uintptr_t)&_estack),
    Reset_Handler,
};

void Reset_Handler(void) {
    uint8_t *src = &_etext;
    uint8_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }
    main();
    while (1) {
    }
}

void _close(void) {}
void _lseek(void) {}
void _read(void) {}
void _write(void) {}
void _isatty(void) {}
void _fstat(void) {}
void _getpid(void) {}
void _kill(void) {}

#include <stdio.h>

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "hardware/gpio.h"

#include "eeprom.pio.h"

uint8_t __not_in_flash("eeprom") eeprom[32768];

int main(void) {
#if PICO_SCANVIDEO_48MHZ
    set_sys_clock_48mhz();
    // Re init uart now that clk_peri has changed
    setup_default_uart();
#endif
    stdio_init_all();

    sleep_ms(2000);

    printf("eeprom starting\n");

#if 0
    for (uint i=0; i<29; i++) {     // skip last 3 pins
        if (i >= 23 && i <= 25)
            continue;               // skip 3 pins with internal use
        gpio_init(i);
        gpio_set_dir(i, GPIO_IN);
    }

    while (true) {
        for (uint i=0; i<29; i++) {     // skip last 3 pins
            if (i >= 23 && i <= 25)
                continue;               // skip 3 pins with internal use
            printf("%u: %d\n", i, gpio_get(i));
        }
        sleep_ms(1000);
    }
    return 0;
#endif


    PIO pio = pio1;
    uint offset = pio_add_program(pio, &eeprom_program);
    uint sm = pio_claim_unused_sm(pio, true);
    eeprom_program_init(pio, sm, offset);

    /* TODO*/
    // bi_decl_if_func_used(bi_pin_mask_with_name(0xff << 14, "Input data bus"));
    // bi_decl_if_func_used(bi_1pin_with_name(14+8, "Input clock"));

    #define OPADR(op) ((op >> 8) & 0x7fff)
    #define OPDATA(op) ((op >> 23) & 0xff)
//    #define PUTDATA(v) ((v << 8) | 0xff)

    for (uint16_t i=0; i<32768; i++)
        eeprom[i] = (i & 0xff) + (i >> 8);

    printf("eeprom initialized\n");

#define EEPROM_DEBUG

    uint32_t op, op2;
    uint16_t adr;
    while (true) {
        op = pio_sm_get_blocking(pio, sm);
        adr = OPADR(op);

        if (!(op & 0b00100)) {
            // /R is low host is reading from us
//            pio_sm_put_blocking(pio, sm, PUTDATA(eeprom[adr]));
            // don't need to block on SM request
            pio_sm_put(pio, sm, eeprom[adr]);
#ifdef EEPROM_DEBUG
            printf("read %hhx\n",  eeprom[adr]);
#endif
        } else {
            // host is writing to us, after waiting for rising /W so /W=/R=1
            eeprom[adr] = OPDATA(op);
#ifdef EEPROM_DEBUG
            printf("wrote %hhx\n", OPDATA(op));
#endif

        }
#ifdef EEPROM_DEBUG
        printf("completed op %x: /R %d /W %d /CS %d adr %hx data %hhx\n",
            op, (op >> 2) & 1, (op >> 3) & 1, (op >> 4) & 1, adr, (uint8_t)OPDATA(op));
#endif
    }
}
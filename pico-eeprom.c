#include <stdio.h>

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "hardware/gpio.h"

#include "eeprom.pio.h"

// uncomment to watch (slow!) read/write operations via the USB terminal
// #define EEPROM_DEBUG

// macros to extract the address and data parts from the 32-bit pin mapping
#define OPADR(op) ((op >> 8) & 0x7fff)
#define OPDATA(op) ((op >> 23) & 0xff)

// access times slow from tens of ns to several microseconds if memory is in flash!
uint8_t __not_in_flash("eeprom") eeprom[32768];


int main(void) {

#ifdef EEPROM_DEBUG
    stdio_init_all();
    sleep_ms(2000);
    printf("eeprom starting\n");
#endif

    // set up the PIO
    PIO pio = pio1;
    uint offset = pio_add_program(pio, &eeprom_program);
    uint sm = pio_claim_unused_sm(pio, true);
    eeprom_program_init(pio, sm, offset);

    // declare pins for picotool
    bi_decl_if_func_used(bi_pin_mask_with_name(0x7fff, "A0-A14"));
    bi_decl_if_func_used(bi_pin_mask_with_name(0xff << 15, "D0-D7"));
    bi_decl_if_func_used(bi_3pins_with_names(26, "/W", 27, "/R", 28, "/CS"));

    // initialize the eeprom with dummy data
    for (uint16_t i=0; i<32768; i++)
        eeprom[i] = (i & 0xff) + (i >> 8);

#ifdef EEPROM_DEBUG
    printf("eeprom initialized\n");
#endif

    uint32_t op;
    uint16_t adr;

    while (true) {
        /*
        The PIO sends us a 32-bit word with the GPIO state
        when it sees a valid read or write operation.
        Note that the write operation waits for a rising /W edge
        to assure the address and data is valid.
        So the two states we'll see here are:

        read:   /CS = 0, /R = 0, /W = 1
        write:  /CS = 0, /R = 1, /W = 1 (after the rising edge)
        */
        op = pio_sm_get_blocking(pio, sm);
        adr = OPADR(op);

        // Test the /R flag to distinguish a read from a write
        // Since reading is slower (requiring a round trip)
        // save a cycle by handling it first (branch not taken)
        if (!(op & 0b00100)) {
            // Fetch the data value and return to the PIO to send out
            // Nb. we don't need _blocking since the SM is waiting
            // which also saves some cycles
            pio_sm_put(pio, sm, eeprom[adr]);
#ifdef EEPROM_DEBUG
            printf("read %hhx\n",  eeprom[adr]);
#endif
        } else {
            // Otherwise we just need to save the data value and move on
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
## Emulate an Atmel AT28C256 256K (32K x 8) EEPROM with the Pico

This is a simple Pico PIO project that emulates a 
32Kb static RAM like the 
[Atmel AT28C256 EEPROM](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0006.pdf).
At the normal 125MHz Pico clock rate it can handle a read request in about 80ns,
and faster for writing.

When accessed in one half of a two-phase clock cycle like the 65c02,
this suggests it could handle a clock period of 160+ns or six clock cycles per microsecond, i.e. 6Mhz or slower.

### How it works

All 26 exposed GPIO pins are used to present a 15-bit address bus,
an 8-bit data bus, and three control signals /CS, /R and /W.
Note that the Pico operates at 3.3V logic levels.  While in practice
it might work with 5V inputs (make sure the Pico is always powered first!)
it's probably safest to use a bus transceiver like the 74HC245 
and/or 2:1 resistor pairs for level shifting.

                           /CS   /R /W    D7        ...         D1
        +----------------------------------------------------------+
        |  +  +  -  E  +  A 28 - 27 26  R 22 - 21 20 19 18 - 17 16 |
    +-------+                     <-- in (24)                 <--+ |
    |  USB  |                                                    | |
    +-------+                                                  out |
        |  0  1  -  2  3  4  5 -  6  7  8  9 - 10 11 12 13 - 14 15 |
        +----------------------------------------------------------+
          A0                      ...                       A14 D0

The PIO code makes some assumptions about the
unexposed pins.  It maps input pins based starting from GPIO24 so 
the least significant bits correspond to GPIO24, the VBUS sense pin; GPIO25
the user LED; and then the three control pins.
Since the LED is low and VBUS is high (assuming we're powered by USB)
the first five bits are /CS (bit 4), /R, /W, 0, 1 (bit 0).
This lets the PIO directly map each control state to a block of four instructions
using `MOV PC, ISR`:

 /CS | /R  | /W  | LED | VBUS | Offset | State
 --- | --- | --- | --- | --- | --- | ---
  0  |  0  |  0  |  0  |  1  |  1  | No-op
  0  |  0  |  1  |  0  |  1  |  5  | Read
  0  |  1  |  0  |  0  |  1  |  9  | Write
  0  |  1  |  1  |  0  |  1  | 13  | No-op
  1  |  x  |  x  |  0  |  1  | 17, 21, 25, 29 | No-op

Note: If you're not powering with USB (so GPIO24 is low), simply remove the 
first NOP instruction from `eeprom.pio`.

The resulting code is very fast.  Obviously the PIO state machine
is very efficient at watching for a state change, requesting
a read or write from the CPU within 3 or 4 cycles.   But the 
assembler also generates beautifully efficient code on the CPU side.
Looking at the end of the `<main>` routine in `build/pico-eeprom.dis` 
we see the core loop only needs a handful of 
instructions to handle either case.  It's impressive that the
read roundtrip&mdash;where PIO sees /CS and /R low, sends the address to the CPU,
awaits the value, and finally writes the output pins&mdash;only takes about ten cycles, i.e. 80ns at 125MHz.

    100002d4 <main>:
    ...
    10000384:	4465      	add	r5, ip

    ; while (true) {
    ;   (block on Rx fifo)
    10000386:	684b      	ldr	r3, [r1, #4]
    10000388:	421a      	tst	r2, r3
    1000038a:	d1fc      	bne.n	10000386 <main+0xb2>
    ;   r3 is op, the input pin state
    1000038c:	6a2b      	ldr	r3, [r5, #32]
    ;   r0 is adr, clever translation of OPADR to left + right shift
    1000038e:	0258      	lsls	r0, r3, #9
    10000390:	0c40      	lsrs	r0, r0, #17
    ;   if (read) {
    10000392:	421e      	tst	r6, r3
    10000394:	d102      	bne.n	1000039c <main+0xc8>
    ;     fetch  eeprom[adr]
    10000396:	5c23      	ldrb	r3, [r4, r0]
    ;     put it to Tx fifo
    10000398:	612b      	str	r3, [r5, #16]
    1000039a:	e7f4      	b.n	10000386 <main+0xb2>
    ;   } else {
    ;     shift input word so data byte is aligned in low 8 bits
    1000039c:	0ddb      	lsrs	r3, r3, #23
    ;     store in eeprom[adr]
    1000039e:	5423      	strb	r3, [r4, r0]
    100003a0:	e7f1      	b.n	10000386 <main+0xb2>
    ; } 

    100003a2:	46c0      	nop			@ (mov r8, r8)
    100003a4:	10001afc 	.word	0x10001afc
    ...

### Building

Build the project as usual:

    cmake -B build -DPICO_SDK_PATH=../pico-sdk
    cd build
    make

Verify the pin setup with picotool:

    ../picotool/build/picotool info --pins build/pico-eeprom.uf2
    File build/pico-eeprom.uf2 family ID 'rp2040':

    Fixed Pin Information
    0-14:   A0-A14
    15-22:  D0-D7
    26:     /W
    27:     /R
    28:     /CS
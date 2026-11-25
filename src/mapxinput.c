#include "usbpad.h"

#include <stddef.h>
#include <stdint.h>

/* XInput (Xbox 360 wired protocol), input report:
 *   [0] 00 (type) [1] 14 (length)
 *   [2..3] buttons: up down left right Start Back L3 R3, LB RB Guide - A B X Y
 *   [4] LT [5] RT (0..255)
 *   [6..13] LX LY RX RY, int16 little-endian, centred on 0, y grows upwards
 * Placed by position, as on a DualSense: A cross, B circle, X square, Y triangle.
 * The Ultimate 2C sends nothing for L4/R4 in this mode.
 *
 * `b` is the report from its byte [2] on: 12 bytes. */
static void parse_x360(const unsigned char *b, pad_state *st)
{
    static const struct { uint16_t bit; uint32_t pad; } map[] = {
        { 1u << 0, PAD_UP },       { 1u << 1, PAD_DOWN },    { 1u << 2, PAD_LEFT },
        { 1u << 3, PAD_RIGHT },    { 1u << 4, PAD_OPTIONS }, { 1u << 5, PAD_CREATE },
        { 1u << 6, PAD_L3 },       { 1u << 7, PAD_R3 },      { 1u << 8, PAD_L1 },
        { 1u << 9, PAD_R1 },       { 1u << 10, PAD_PS },     { 1u << 12, PAD_CROSS },
        { 1u << 13, PAD_CIRCLE },  { 1u << 14, PAD_SQUARE }, { 1u << 15, PAD_TRIANGLE },
    };
    unsigned bt;
    size_t i;

    pad_state_reset(st);
    bt = (unsigned)b[0] | (unsigned)b[1] << 8;
    for (i = 0; i < sizeof map / sizeof map[0]; i++)
        if (bt & map[i].bit) st->buttons |= map[i].pad;
    st->l2 = b[2];
    st->r2 = b[3];
    /* -32768..32767 to 0..255; y flipped, as the DualSense grows downwards */
    st->lx = (uint8_t)(((int16_t)(b[4] | b[5] << 8) + 32768) >> 8);
    st->ly = (uint8_t)((32767 - (int16_t)(b[6] | b[7] << 8)) >> 8);
    st->rx = (uint8_t)(((int16_t)(b[8] | b[9] << 8) + 32768) >> 8);
    st->ry = (uint8_t)((32767 - (int16_t)(b[10] | b[11] << 8)) >> 8);
    if (st->l2 > 24) st->buttons |= PAD_L2;
    if (st->r2 > 24) st->buttons |= PAD_R2;
}

int usbpad_parse_xinput(const unsigned char *r, int n, pad_state *st)
{
    if (n < 14 || r[0] != 0x00 || r[1] < 0x14) return 0;
    parse_x360(r + 2, st);
    return 1;
}

/* Xbox 360 Wireless Receiver, one controller's interface (as Linux's xpad
 * reads it):
 *   08 80 / 08 00     a controller joined / left
 *   00 01 .. ..       input: the wired report from its byte [2] on, at [6]
 *   anything else     link status, the controller's announce, battery: dropped
 * A wheel (the Racing Wheel, subtype 02) reports like a pad: steering on LX,
 * brake on LT, gas on RT. */
int usbpad_parse_xinput_wireless(const unsigned char *r, int n, pad_state *st)
{
    if (n >= 2 && r[0] == 0x08) return r[1] & 0x80 ? XW_JOINED : XW_LEFT;
    if (n < 18 || r[0] != 0x00 || r[1] != 0x01) return XW_NONE;
    parse_x360(r + 6, st);
    return XW_INPUT;
}

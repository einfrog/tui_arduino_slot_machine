#include "animations.h"

// Shift symbol left/right by N pixels (0–7)
void shiftSymbolHorizontal(const uint8_t in[8], uint8_t out[8], int shift) {
    for (int r = 0; r < 8; r++) {
        uint8_t row = in[r];
        // rotate left by shift (wrap around)
        out[r] = (row << shift) | (row >> (8 - shift));
    }
}

// In animations.cpp - better handling of vertical shift
void shiftSymbolVertical(const uint8_t in[8], uint8_t out[8], int shift) {
    // Normalize shift to 0-7 range
    shift = ((shift % 8) + 8) % 8;
    
    for (int r = 0; r < 8; r++) {
        out[r] = in[(r - shift + 8) % 8];
    }
}

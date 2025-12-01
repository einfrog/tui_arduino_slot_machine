#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <stdint.h>

void shiftSymbolHorizontal(const uint8_t in[8], uint8_t out[8], int shift);
void shiftSymbolVertical(const uint8_t in[8], uint8_t out[8], int shift);

#endif

#pragma once

#include <cstdint>
#include <string>

#include <stdio.h>

uint16_t convertBigEndianUInt16(uint16_t value);
uint32_t convertBigEndianUInt32(uint32_t value);

bool writePackedString(const std::string& str, FILE *stream);
bool writePackedUInt16(uint16_t value, FILE *stream);
bool writePackedUInt32(uint32_t value, FILE *stream);

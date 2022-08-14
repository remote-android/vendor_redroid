#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "data.h"

uint16_t convertBigEndianUInt16(uint16_t value)
{
	union { uint16_t value; unsigned char data[2]; } aux = { 0x4142 };

	if (aux.data[0] == 0x41)
	{
		return value;
	}

	aux.data[0] = (value >> 8) & 0xff;
	aux.data[1] = value & 0xff;

	return aux.value;
}

uint32_t convertBigEndianUInt32(uint32_t value)
{
	union { uint32_t value; unsigned char data[4]; } aux = { 0x41424344 };

	if (aux.data[0] == 0x41)
	{
		return value;
	}

	aux.data[0] = (value >> 24) & 0xff;
	aux.data[1] = (value >> 16) & 0xff;
	aux.data[2] = (value >> 8) & 0xff;
	aux.data[3] = value & 0xff;

	return aux.value;
}

bool writePackedString(const std::string& str, FILE *stream)
{
    const char *string = str.c_str();
	size_t stringLength = strlen(string);

	if (!writePackedUInt16(stringLength, stream))
	{
		return false;
	}

	return fwrite(string, stringLength, 1, stream) == 1;
}

bool writePackedUInt16(uint16_t value, FILE *stream)
{
	uint16_t buffer = convertBigEndianUInt16(value);
	return fwrite(&buffer, sizeof buffer, 1, stream) == 1;
}

bool writePackedUInt32(uint32_t value, FILE *stream)
{
	uint32_t buffer = convertBigEndianUInt32(value);
	return fwrite(&buffer, sizeof buffer, 1, stream) == 1;
}


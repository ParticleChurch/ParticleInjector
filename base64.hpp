/*
 * Base64 encoding/decoding (RFC1341)
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#pragma once

#include <stdio.h>
#include <string>

namespace base64
{
	constexpr unsigned char base64_table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	extern unsigned char* encode(const unsigned char* src, size_t len, size_t* out_len);
	extern unsigned char* decode(const unsigned char* src, size_t len, size_t* out_len);
}
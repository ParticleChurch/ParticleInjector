#pragma once
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <random>
#include <chrono>
#include <fstream>
#include <string>

/*

The algorithm (it has been altered slightly, read the code):

	An encrypted file is split into 2 parts, the header and the file contents.
	The header comes first, and is sizeof(Header) bytes long. See ParseHeader for comments on how it is stored.

	The file contents are split into 256 byte "chunks" and each chunk has 2 parts: its prefix and its data (in that order)
	the prefix contains 4 uint8_t's which are its: ByteRotation, BitRotation, CutLocation, and Inversion (in that order)
	the remaining 252 bytes are the data itself.

	to encrypt, follow these steps. to decrypt, follow them in reverse.
	 1. If Inversion is even, then for every byte in the data,
		add (Header::ByteOffset + ChunkIndex) % max(Header::ByteModulo, Inversion) allow it to overflow
		note: if max(Header::ByteModulo, Inversion) == 0, then modulo by 69 instead (haha funny number right hahahaha)
	 2. Bit rotate each byte by (BitRotation % 8) to the right (here's the formula for such a rotation)
		rotated = (original << BitRotation) | (original >> (8 - BitRotation));
	 3. Rotate the list of bytes to the right by ByteRotation, for example if ByteRotation = 1
		{a,b,c,d} => {d,a,b,c}
	 4. "Cut the deck" of bytes at index CutLocation, for example, if CutLocation = 4:
		{a,b,c,d,e,f,g} => {e,f,g,a,b,c,d}

	assuming that the file size was not perfectly divisible by 252, there are some remaining bytes
	upon encryption, these bytes are shifted to the right to be at the end of their chunk,
	then the empty spots are filled randomly. The number of bytes that were randomized is stored in Header::EndPadding,
	then the chunk is processed as normal

*/

namespace Encryption
{
	constexpr bool debug = false;
	constexpr size_t minHeaderSize = 30;
	constexpr size_t maxHeaderSize = 285;

	extern uint8_t random();

	struct Header
	{
		uint8_t byteModulo;
		uint8_t endPadding;
		uint8_t byteOffset;
		uint8_t nRandomBytes;

		size_t size; // stores a number on interval [30, 285]
		uint8_t* generateBytes();

		bool isValid;
		std::string parseError;
	};

	extern Header generateHeader();
	extern Header parseHeader(uint8_t* bytes, size_t nBytes);
	/*
		The header is stored in the following format, each line represents one byte:
			1	random number
			2	random number
			3	Header::EndPadding
			4	random number
			5	'P'
			6	'A'
			7	'R'
			8	'T'
			9	'I'
			10	'C'
			11	'L'
			12	'E'
			13	NULL
			14	random number
			15	'C'
			16	'H'
			17	'U'
			18	'R'
			19	'C'
			20	'H'
			21	Header::ByteOffset
			22	random number
			23	random number
			24	NULL
			25	NRandomBytes
				[the next NRandomBytes are random]
			26 + NRandomBytes	random number
			27 + NRandomBytes	Header::ByteModulo
			28 + NRandomBytes	random number
			29 + NRandomBytes	NULL
			30 + NRandomBytes	random number
		this means that the header is anywhere from 30 to (30+255=) 285 bytes
	*/

	extern void encryptChunk(Header& header, size_t chunkIndex, uint8_t* original/* size = 252 */, uint8_t* output/* size = 265 */);
	extern void decryptChunk(Header& header, size_t chunkIndex, uint8_t* original/* size = 256 */, uint8_t* output/* size = 252 */);

	extern bool encryptFile(const char* fileInput, const char* fileOutput);
	extern bool decryptFile(const char* fileInput, const char* fileOutput);

	extern uint64_t getDecryptedSize(Header header, uint64_t encryptedSize);
}
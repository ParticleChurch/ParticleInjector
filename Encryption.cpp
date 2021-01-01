#include "Encryption.hpp"

uint8_t Encryption::random()
{
	static std::uniform_int_distribution<uint16_t> distribution(0, 0xFFFF);
	static std::mt19937 rng;
	static bool seeded = false;
	if (!seeded)
	{
		auto now = std::chrono::system_clock::now().time_since_epoch();
		auto t = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
		t = t % 0xffffffff;
		rng.seed((unsigned int)t);
		seeded = true;
	}

	return (uint8_t)(distribution(rng) % 0xFF);
}

uint8_t* Encryption::Header::generateBytes()
{
	if (this->size < 30 || this->size > 285)
		return nullptr;

	uint8_t* bytes = (uint8_t*)malloc(this->size);
	if (!bytes)
		return nullptr;

	for (size_t i = 0; i < this->size; i++)
		bytes[i] = random();

	bytes[2] = this->endPadding;
	bytes[12] = NULL;
	bytes[20] = this->byteOffset;
	bytes[23] = NULL;
	bytes[24] = (uint8_t)(this->size - (size_t)30); // will not overflow, see check above
	bytes[26 + bytes[24]] = this->byteModulo;
	bytes[28 + bytes[24]] = NULL;

	bytes[4] = 'P';
	bytes[5] = 'A';
	bytes[6] = 'R';
	bytes[7] = 'T';
	bytes[8] = 'I';
	bytes[9] = 'C';
	bytes[10] = 'L';
	bytes[11] = 'E';

	bytes[14] = 'C';
	bytes[15] = 'H';
	bytes[16] = 'U';
	bytes[17] = 'R';
	bytes[18] = 'C';
	bytes[19] = 'H';

	return bytes;
};

Encryption::Header Encryption::generateHeader()
{
	Header head{};

	head.byteModulo = random();
	head.endPadding = random();
	head.byteOffset = random();
	head.nRandomBytes = random();
	head.size = 30 + head.nRandomBytes;
	head.isValid = true;

	return head;
}

Encryption::Header Encryption::parseHeader(uint8_t* bytes, size_t nBytes)
{
	Header head{};

	if (nBytes < minHeaderSize)
	{
		head.isValid = false;
		head.parseError = "Expected at least " + std::to_string(minHeaderSize) + " bytes, but got " + std::to_string(nBytes) + " bytes";
		return head;
	}

	bool PARTICLECHURCH =
		bytes[4] == 'P' &&
		bytes[5] == 'A' &&
		bytes[6] == 'R' &&
		bytes[7] == 'T' &&
		bytes[8] == 'I' &&
		bytes[9] == 'C' &&
		bytes[10] == 'L' &&
		bytes[11] == 'E' &&

		bytes[14] == 'C' &&
		bytes[15] == 'H' &&
		bytes[16] == 'U' &&
		bytes[17] == 'R' &&
		bytes[18] == 'C' &&
		bytes[19] == 'H';

	if (!PARTICLECHURCH)
	{
		head.isValid = false;
		head.parseError = "PARTICLECHURCH not found, actually got: " + std::string((char*)bytes, nBytes);
		return head;
	}

	if (bytes[12] || bytes[23])
	{
		head.isValid = false;
		head.parseError = "nonzero NULLS";
		return head;
	}

	head.nRandomBytes = bytes[24];
	head.size = 25 + head.nRandomBytes + 5;

	if (nBytes < head.size)
	{
		head.isValid = false;
		head.parseError = "bytes sent is less than bytes required";
		return head;
	}

	head.endPadding = bytes[2];
	head.byteOffset = bytes[20];
	head.byteModulo = bytes[26 + head.nRandomBytes];
	head.isValid = true;
	head.parseError = "none";

	return head;
}

void Encryption::encryptChunk(Header& header, size_t chunkIndex, uint8_t* decrypted/* size = 252 */, uint8_t* encrypted/* size = 265 */)
{
	uint8_t byteRotation = 1 + (encrypted[0] = random()) % 251;
	uint8_t bitRotation = (encrypted[1] = random());
	uint8_t cutLocation = 10 + (encrypted[2] = random()) % 200;
	uint8_t inversion = 10 + (encrypted[3] = random()) % 200;

	for (uint8_t i = 0; i < 252; i++)
	{
		uint8_t r = ((uint16_t)byteRotation + (uint16_t)i) % 252;
		int16_t p = r - cutLocation;
		while (p < 0) p += 252;

		uint8_t br = 1 + ((size_t)i + (size_t)bitRotation + chunkIndex) % 7; // [1, 7]
		uint8_t v = (decrypted[i] >> br) | (decrypted[i] << (8 - br));

		if (!(inversion % 2) || !(inversion % 3) || !(inversion % 5))
		{
			uint8_t add = ((uint16_t)header.byteOffset + (uint16_t)chunkIndex) % std::max(header.byteModulo, inversion);
			v = ((uint16_t)v + (uint16_t)add) % 256;
		}

		encrypted[4 + p] = v;
	}
}

void Encryption::decryptChunk(Header& header, size_t chunkIndex, uint8_t* encrypted/* size = 256 */, uint8_t* decrypted/* size = 252 */)
{
	uint8_t byteRotation = 1 + encrypted[0] % 251;
	uint8_t bitRotation = encrypted[1];
	uint8_t cutLocation = 10 + encrypted[2] % 200;
	uint8_t inversion = 10 + encrypted[3] % 200;

	for (uint8_t i = 0; i < 252; i++)
	{
		uint8_t r = ((uint16_t)byteRotation + (uint16_t)i) % 252;
		int16_t p = r - cutLocation;
		while (p < 0) p += 252;

		uint8_t br = 1 + ((size_t)i + (size_t)bitRotation + chunkIndex) % 7; // [1, 7]

		uint8_t v = encrypted[4 + p];

		if (!(inversion % 2) || !(inversion % 3) || !(inversion % 5))
		{
			uint8_t add = ((uint16_t)header.byteOffset + (uint16_t)chunkIndex) % std::max(header.byteModulo, inversion);
			add = (256 - add);
			v = ((uint16_t)v + (uint16_t)add) % 256;
		}

		decrypted[i] = (v << br) | (v >> (8 - br));
	}
}

bool Encryption::encryptFile(const char* fileInput, const char* fileOutput)
{
	if (debug)
	{
		std::cout << "========== Encrypting ==========" << std::endl;
		std::cout << "Input: (decrypted) " << fileInput << std::endl;
		std::cout << "Output: (encrypted) " << fileOutput << std::endl;
	}

	std::ifstream inFile;
	inFile.open(fileInput, std::ios::in | std::ios::binary);
	if (!inFile.is_open())
	{
		if (debug)
			std::cout << "Failed to open input file" << std::endl;
		return false;
	}
	inFile.seekg(0, std::ios::end);
	uint64_t inFileSize = inFile.tellg(); // overflows files with size > 16,777,216 terabytes, i think we're good
	inFile.clear();
	inFile.seekg(0);
	if (debug)
		std::cout << "Input file size: " << inFileSize << " bytes" << std::endl;

	std::ofstream outFile;
	outFile.open(fileOutput, std::ios::out | std::ios::binary);
	if (!outFile.is_open())
	{
		if (debug)
			std::cout << "Failed to open output file" << std::endl;
		inFile.close();
		return false;
	}

	if (debug)
		std::cout << "Successfully opened files, now encrypting..." << std::endl;

	Header head = generateHeader();
	head.endPadding = 252 - (inFileSize % 252);
	uint8_t* headerBytes = head.generateBytes();

	if (!headerBytes)
	{
		if (debug)
			std::cout << "Failed to generate header bytes" << std::endl;
		return false;
	}

	if (debug)
		std::cout <<
		"Generated header {byteModulo = " << (int)head.byteModulo <<
		", byteOffset = " << (int)head.byteOffset <<
		", endPadding = " << (int)head.endPadding <<
		", nRandomBytes = " << (int)head.nRandomBytes << "}, writing to output file..." << std::endl;

	if (!outFile.write((char*)headerBytes, head.size))
	{
		if (debug)
			std::cout << "Failed to write header to file" << std::endl;
		return false;
	}
	free(headerBytes);

	uint8_t* inBuffer = (uint8_t*)malloc(252);
	if (!inBuffer)
	{
		if (debug)
			std::cout << "Failed to malloc 252 bytes for inBuffer" << std::endl;
		return false;
	}

	uint8_t* outBuffer = (uint8_t*)malloc(256);
	if (!outBuffer)
	{
		if (debug)
			std::cout << "Failed to malloc 256 bytes for outBuffer" << std::endl;
		return false;
	}

	size_t chunkIndex = 0;
	size_t nChunks = (size_t)ceil((double)inFileSize / 252.0);
	while (inFile.read((char*)inBuffer, 252))
	{
		if (debug)
			std::cout << "Encrypting chunk " << chunkIndex << "/" << nChunks << std::endl;

		encryptChunk(head, chunkIndex, inBuffer, outBuffer);

		if (!outFile.write((char*)outBuffer, 256))
		{
			if (debug)
				std::cout << "Failed to write 256 bytes to output file" << std::endl;

			outFile.close();
			inFile.close();
			free(inBuffer);
			free(outBuffer);
			return false;
		}
		chunkIndex++;
	}

	if ((252 - inFile.gcount()) != head.endPadding)
	{
		if (debug)
			std::cout << "endPadding discrepancy" << std::endl;

		outFile.close();
		inFile.close();
		free(inBuffer);
		free(outBuffer);
		return false;
	}

	if (head.endPadding > 0)
	{
		if (debug)
			std::cout << "Padding last chunk..." << std::endl;

		for (int i = 252 - 1; i >= head.endPadding; i--)
		{
			inBuffer[i] = inBuffer[i - head.endPadding];
		}
		for (int i = 0; i < head.endPadding; i++)
		{
			inBuffer[i] = 'V';// random();
		}

		// now encrypt as normal
		if (debug)
			std::cout << "Encrypting chunk " << chunkIndex << "/" << nChunks << std::endl;

		encryptChunk(head, chunkIndex, inBuffer, outBuffer);

		if (!outFile.write((char*)outBuffer, 256))
		{
			if (debug)
				std::cout << "Failed to write 256 bytes to output file" << std::endl;

			outFile.close();
			inFile.close();
			free(inBuffer);
			free(outBuffer);
			return false;
		}
	}

	if (debug)
		std::cout << "Done! Wrote " << (uint64_t)256 * (uint64_t)nChunks + (uint64_t)head.size << " bytes to " << fileOutput << std::endl;

	outFile.close();
	inFile.close();
	free(inBuffer);
	free(outBuffer);
	return true;
}

bool Encryption::decryptFile(const char* fileInput, const char* fileOutput)
{
	if (debug)
	{
		std::cout << "========== Decrypting ==========" << std::endl;
		std::cout << "Input: (encrypted) " << fileInput << std::endl;
		std::cout << "Output: (decrypted) " << fileOutput << std::endl;
	}

	std::ifstream inFile;
	inFile.open(fileInput, std::ios::in | std::ios::binary);
	if (!inFile.is_open())
	{
		if (debug)
			std::cout << "Failed to open input file" << std::endl;
		return false;
	}
	inFile.seekg(0, std::ios::end);
	uint64_t inFileSize = inFile.tellg(); // overflows files with size > 16,777,216 terabytes, i think we're good
	inFile.clear();
	inFile.seekg(0);
	if (debug)
		std::cout << "Input file size: " << inFileSize << " bytes" << std::endl;

	if (inFileSize < minHeaderSize)
	{
		if (debug)
			std::cout << "inFile invalid - too small" << std::endl;
		inFile.close();
		return false;
	}

	size_t headerSearchSize = (size_t)std::min(inFileSize, (uint64_t)maxHeaderSize);
	uint8_t* headerBytes = (uint8_t*)malloc(headerSearchSize);
	if (!headerBytes)
	{
		if (debug)
			std::cout << "failed to allocate header bytes" << std::endl;
		inFile.close();
		return false;
	}
	if (!inFile.read((char*)headerBytes, headerSearchSize))
	{
		if (debug)
			std::cout << "failed to read header bytes in inFile" << std::endl;
		inFile.close();
		free(headerBytes);
		return false;
	}
	Header head = parseHeader(headerBytes, headerSearchSize);
	free(headerBytes);
	if (!head.isValid)
	{
		if (debug)
			std::cout << "header is invalid" << std::endl;
		inFile.close();
		return false;
	}

	if (debug)
		std::cout <<
		"Parsed header {byteModulo = " << (int)head.byteModulo <<
		", byteOffset = " << (int)head.byteOffset <<
		", endPadding = " << (int)head.endPadding <<
		", nRandomBytes = " << (int)head.nRandomBytes << "}, writing to output file..." << std::endl;

	if ((inFileSize - head.size) % 256)
	{
		if (debug)
			std::cout << "inFileSize is not valid, expected a multiple of 256" << std::endl;
		inFile.close();
		return false;
	}

	inFile.clear();
	inFile.seekg(head.size);

	if (debug)
		std::cout << "now opening output file" << std::endl;

	std::ofstream outFile;
	outFile.open(fileOutput, std::ios::out | std::ios::binary);
	if (!outFile.is_open())
	{
		if (debug)
			std::cout << "Failed to open output file" << std::endl;
		inFile.close();
		return false;
	}

	if (debug)
		std::cout << "Successfully opened files, now decrypting..." << std::endl;

	uint8_t* inBuffer = (uint8_t*)malloc(256);
	if (!inBuffer)
	{
		if (debug)
			std::cout << "Failed to malloc 256 bytes for inBuffer" << std::endl;
		return false;
	}

	uint8_t* outBuffer = (uint8_t*)malloc(252);
	if (!outBuffer)
	{
		if (debug)
			std::cout << "Failed to malloc 252 bytes for outBuffer" << std::endl;
		return false;
	}

	size_t chunkIndex = 0;
	size_t nChunks = (size_t)((inFileSize - head.size) / 256);
	while (inFile.read((char*)inBuffer, 256))
	{
		if (debug)
			std::cout << "Decrypting chunk " << chunkIndex << "/" << nChunks << std::endl;

		decryptChunk(head, chunkIndex, inBuffer, outBuffer);

		bool isLastChunk = (chunkIndex + 1) >= nChunks;
		if (isLastChunk)
		{
			if (debug)
				std::cout << "removing padding from last chunk..." << std::endl;

			for (int i = 0; i < 252 - head.endPadding; i++)
			{
				outBuffer[i] = outBuffer[i + head.endPadding];
			}
		}

		if (!outFile.write((char*)outBuffer, isLastChunk ? 252 - head.endPadding : 252))
		{
			if (debug)
				std::cout << "Failed to write 252 bytes to output file" << std::endl;

			outFile.close();
			inFile.close();
			free(inBuffer);
			free(outBuffer);
			return false;
		}
		chunkIndex++;
	}

	if (debug)
		std::cout << "Done! Wrote " << (uint64_t)252 * (uint64_t)nChunks - (uint64_t)head.endPadding << " bytes to " << fileOutput << std::endl;

	outFile.close();
	inFile.close();
	free(inBuffer);
	free(outBuffer);
	return true;
}

uint64_t Encryption::getDecryptedSize(Header header, uint64_t encryptedSize)
{
	if (encryptedSize <= header.size) return 0;
	encryptedSize -= header.size;
	uint64_t nChunks = encryptedSize / 256; // encryptedSize *should* be a multiple of 256
	encryptedSize = nChunks * 252;
	if (encryptedSize < header.endPadding) return 0;
	return encryptedSize - header.endPadding;
}

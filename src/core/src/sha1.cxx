// //////////////////////////////////////////////////////////
// sha1.cpp
// Copyright (c) 2014,2015 Stephan Brumme. All rights reserved.
// see http://create.stephan-brumme.com/disclaimer.html
//
// Changes: Formatted according to valet's coding style.
//          (Samil Atesoglu <m.samilatesoglu@gmail.com>)

#include "sha1.hxx"

/// same as reset()
SHA1::SHA1()
{
	reset();
}

/// restart
void SHA1::reset()
{
	num_bytes = 0;
	buffer_size = 0;

	// according to RFC 1321
	hash[0] = 0x67452301;
	hash[1] = 0xefcdab89;
	hash[2] = 0x98badcfe;
	hash[3] = 0x10325476;
	hash[4] = 0xc3d2e1f0;
}

namespace
{
// mix functions for process_block()
inline uint32_t f1(uint32_t b, uint32_t c, uint32_t d)
{
	return d ^ (b & (c ^ d)); // original: f = (b & c) | ((~b) & d);
}

inline uint32_t f2(uint32_t b, uint32_t c, uint32_t d)
{
	return b ^ c ^ d;
}

inline uint32_t f3(uint32_t b, uint32_t c, uint32_t d)
{
	return (b & c) | (b & d) | (c & d);
}

inline uint32_t rotate(uint32_t a, uint32_t c)
{
	return (a << c) | (a >> (32 - c));
}

inline uint32_t swap(uint32_t x)
{
#if defined(__GNUC__) || defined(__clang__)
	return __builtin_bswap32(x);
#endif
#ifdef MSC_VER
	return _byteswap_ulong(x);
#endif

	return (x >> 24) | ((x >> 8) & 0x0000FF00) | ((x << 8) & 0x00FF0000) | (x << 24);
}
} // namespace

/// process 64 bytes
void SHA1::process_block(const void* data)
{
	// get last hash
	uint32_t a = hash[0];
	uint32_t b = hash[1];
	uint32_t c = hash[2];
	uint32_t d = hash[3];
	uint32_t e = hash[4];

	// data represented as 16x 32-bit words
	const uint32_t* input = (uint32_t*)data;
	// convert to big endian
	uint32_t words[80];
	for (int i = 0; i < 16; i++)
#if defined(__BYTE_ORDER) && (__BYTE_ORDER != 0) && (__BYTE_ORDER == __BIG_ENDIAN)
		words[i] = input[i];
#else
		words[i] = swap(input[i]);
#endif

	// extend to 80 words
	for (int i = 16; i < 80; i++)
		words[i] = rotate(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

	// first round
	for (int i = 0; i < 4; i++) {
		int offset = 5 * i;
		// clang-format off
		e += rotate(a, 5) + f1(b, c, d) + words[offset    ] + 0x5a827999; b = rotate(b, 30);
		d += rotate(e, 5) + f1(a, b, c) + words[offset + 1] + 0x5a827999; a = rotate(a, 30);
		c += rotate(d, 5) + f1(e, a, b) + words[offset + 2] + 0x5a827999; e = rotate(e, 30);
		b += rotate(c, 5) + f1(d, e, a) + words[offset + 3] + 0x5a827999; d = rotate(d, 30);
		a += rotate(b, 5) + f1(c, d, e) + words[offset + 4] + 0x5a827999; c = rotate(c, 30);
		// clang-format on
	}

	// second round
	for (int i = 4; i < 8; i++) {
		int offset = 5 * i;
		// clang-format off
		e += rotate(a, 5) + f2(b, c, d) + words[offset    ] + 0x6ed9eba1; b = rotate(b, 30);
		d += rotate(e, 5) + f2(a, b, c) + words[offset + 1] + 0x6ed9eba1; a = rotate(a, 30);
		c += rotate(d, 5) + f2(e, a, b) + words[offset + 2] + 0x6ed9eba1; e = rotate(e, 30);
		b += rotate(c, 5) + f2(d, e, a) + words[offset + 3] + 0x6ed9eba1; d = rotate(d, 30);
		a += rotate(b, 5) + f2(c, d, e) + words[offset + 4] + 0x6ed9eba1; c = rotate(c, 30);
		// clang-format on
	}

	// third round
	for (int i = 8; i < 12; i++) {
		int offset = 5 * i;
		// clang-format off
		e += rotate(a, 5) + f3(b, c, d) + words[offset    ] + 0x8f1bbcdc; b = rotate(b, 30);
		d += rotate(e, 5) + f3(a, b, c) + words[offset + 1] + 0x8f1bbcdc; a = rotate(a, 30);
		c += rotate(d, 5) + f3(e, a, b) + words[offset + 2] + 0x8f1bbcdc; e = rotate(e, 30);
		b += rotate(c, 5) + f3(d, e, a) + words[offset + 3] + 0x8f1bbcdc; d = rotate(d, 30);
		a += rotate(b, 5) + f3(c, d, e) + words[offset + 4] + 0x8f1bbcdc; c = rotate(c, 30);
		// clang-format on
	}

	// fourth round
	for (int i = 12; i < 16; i++) {
		int offset = 5 * i;
		// clang-format off
		e += rotate(a, 5) + f2(b, c, d) + words[offset    ] + 0xca62c1d6; b = rotate(b, 30);
		d += rotate(e, 5) + f2(a, b, c) + words[offset + 1] + 0xca62c1d6; a = rotate(a, 30);
		c += rotate(d, 5) + f2(e, a, b) + words[offset + 2] + 0xca62c1d6; e = rotate(e, 30);
		b += rotate(c, 5) + f2(d, e, a) + words[offset + 3] + 0xca62c1d6; d = rotate(d, 30);
		a += rotate(b, 5) + f2(c, d, e) + words[offset + 4] + 0xca62c1d6; c = rotate(c, 30);
		// clang-format on
	}

	// update hash
	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
	hash[4] += e;
}

/// add arbitrary number of bytes
void SHA1::add(const void* data, size_t num_bytes_to_add)
{
	const uint8_t* current = (const uint8_t*)data;

	if (buffer_size > 0) {
		while (num_bytes_to_add > 0 && buffer_size < BlockSize) {
			buffer[buffer_size++] = *current++;
			num_bytes_to_add--;
		}
	}

	// full buffer
	if (buffer_size == BlockSize) {
		process_block((void*)buffer);
		num_bytes += BlockSize;
		buffer_size = 0;
	}

	// no more data ?
	if (num_bytes_to_add == 0)
		return;

	// process full blocks
	while (num_bytes_to_add >= BlockSize) {
		process_block(current);
		current += BlockSize;
		num_bytes += BlockSize;
		num_bytes_to_add -= BlockSize;
	}

	// keep remaining bytes in buffer
	while (num_bytes_to_add > 0) {
		buffer[buffer_size++] = *current++;
		num_bytes_to_add--;
	}
}

/// process final block, less than 64 bytes
void SHA1::process_buffer()
{
	// the input bytes are considered as bits strings, where the first bit is the most
	// significant bit of the byte

	// - append "1" bit to message
	// - append "0" bits until message length in bit mod 512 is 448
	// - append length as 64 bit integer

	// number of bits
	size_t padded_length = buffer_size * 8;

	// plus one bit set to 1 (always appended)
	padded_length++;

	// number of bits must be (numBits % 512) = 448
	size_t lower11Bits = padded_length & 511;
	if (lower11Bits <= 448)
		padded_length += 448 - lower11Bits;
	else
		padded_length += 512 + 448 - lower11Bits;
	// convert from bits to bytes
	padded_length /= 8;

	// only needed if additional data flows over into a second block
	unsigned char extra[BlockSize];

	// append a "1" bit, 128 => binary 10000000
	if (buffer_size < BlockSize)
		buffer[buffer_size] = 128;
	else
		extra[0] = 128;

	size_t i;
	for (i = buffer_size + 1; i < BlockSize; i++)
		buffer[i] = 0;
	for (; i < padded_length; i++)
		extra[i - BlockSize] = 0;

	// add message length in bits as 64 bit number
	uint64_t msg_bits = 8 * (num_bytes + buffer_size);
	// find right position
	unsigned char* add_length;
	if (padded_length < BlockSize)
		add_length = buffer + padded_length;
	else
		add_length = extra + padded_length - BlockSize;

	// must be big endian
	*add_length++ = (unsigned char)((msg_bits >> 56) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 48) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 40) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 32) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 24) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 16) & 0xFF);
	*add_length++ = (unsigned char)((msg_bits >> 8) & 0xFF);
	*add_length = (unsigned char)(msg_bits & 0xFF);

	// process blocks
	process_block(buffer);
	// flowed over into a second block ?
	if (padded_length > BlockSize)
		process_block(extra);
}

/// return latest hash as 40 hex characters
std::string SHA1::get_hash()
{
	// compute hash (as raw bytes)
	unsigned char raw_hash[HashBytes];
	get_hash(raw_hash);

	// convert to hex string
	std::string result;
	result.reserve(2 * HashBytes);
	for (int i = 0; i < HashBytes; i++) {
		static const char dec2hex[16 + 1] = "0123456789abcdef";
		result += dec2hex[(raw_hash[i] >> 4) & 15];
		result += dec2hex[raw_hash[i] & 15];
	}

	return result;
}

/// return latest hash as bytes
void SHA1::get_hash(unsigned char buffer[SHA1::HashBytes])
{
	// save old hash if buffer is partially filled
	uint32_t old_hash[HashValues];
	for (int i = 0; i < HashValues; i++)
		old_hash[i] = hash[i];

	// process remaining bytes
	process_buffer();

	unsigned char* current = buffer;
	for (int i = 0; i < HashValues; i++) {
		*current++ = (hash[i] >> 24) & 0xFF;
		*current++ = (hash[i] >> 16) & 0xFF;
		*current++ = (hash[i] >> 8) & 0xFF;
		*current++ = hash[i] & 0xFF;

		// restore old hash
		hash[i] = old_hash[i];
	}
}

/// compute SHA1 of a memory block
std::string SHA1::operator()(const void* data, size_t num_bytes)
{
	reset();
	add(data, num_bytes);
	return get_hash();
}

/// compute SHA1 of a string, excluding final zero
std::string SHA1::operator()(const std::string& text)
{
	reset();
	add(text.c_str(), text.size());
	return get_hash();
}

#pragma once
#ifndef FLICTOOL_FLIC_H
#define FLICTOOL_FLIC_H

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "Bitmap.h"

#pragma pack(push, 1)
struct FlicHeader {
	uint32_t size;
	uint16_t magic;
	uint16_t frames;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t flags;
	uint16_t speed;
	uint32_t next;
	uint32_t frit;

	char padding[102];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct FlicFrameHeader {
	uint32_t size;
	uint16_t magic; // F1FA
	uint16_t chunks;

	char padding[8];
};
#pragma pack(pop)

enum FlicChunkType {
	FLI_COLOR = 11,
	FLI_LC = 12,
	FLI_BLACK = 13,
	FLI_BRUN = 15,
	FLI_COPY = 16,
	FLI_DTA_BRUN = 25,
	FLI_DTA_COPY = 26,
	FLI_DTA_LC = 27,
};

#pragma pack(push, 1)
struct FlicChunkHeader {
	uint32_t size;
	uint16_t type;
};
#pragma pack(pop)

struct FlicFrame {
	uint8_t *pixels;
};

struct Packet {
	char *data;
	size_t size;
};

struct SubChunk {
	uint8_t pixelSkip;
	const uint8_t *start;
	size_t length;
};

class Flic {
public:
	/**
	 * Compiles the frames found in the specified directory to create a new FLH file.
	 * \param input the input directory name
	 * \param output the output filename
	 */
	void compile(const std::string &input, const std::string &output);

	/**
	 * Decompiles the specified FLH file to create separate frames.
	 * \param input the file to decompile
	 * \param output the output directory to place frames in
	 */
	void decompile(const std::string &input, const std::string &output);
private:
	/**
	 * Writes a Flic Repeat Packet. Used in DTA_BRUN chunks.
	 * \param data pointer to the pixel to repeat
	 * \param count the amount of times to repeat the pixel
	 * \param bpp the bit depth of the Flic Animation
	 * \param packets the vector to append the resulting packet to
	 */
	void writeRepeatPacket(const uint8_t *data, int32_t count, uint8_t bpp, std::vector<Packet> &packets);

	/**
	 * Writes a Flic Copy Packet. Used in DTA_BRUN chunks.
	 * \param data pointer to the first pixel to copy
	 * \param count the amount of pixels to copy
	 * \param bpp the bit depth of the Flic Animation
	 * \param packets the vector to append the resulting packet to
	 */
	void writeCopyPacket(const uint8_t *data, int32_t count, uint8_t bpp, std::vector<Packet> &packets);

	/**
	 * Writes a Flic Delta Repeat Packet. Used in DTA_LC chunks.
	 * \param data pointer to the pixel to repeat
	 * \param count the amount of times to repeat the pixel
	 * \param bpp the bit depth of the Flic Animation
	 * \param pixelSkip the amount of pixels to skip before this packet
	 * \param packets the vector to append the resulting packet to
	 */
	void writeDeltaRepeatPacket(const uint8_t *data, int32_t count, uint8_t bpp, uint8_t pixelSkip, std::vector<Packet> &packets);

	/**
	 * Writes a Flic Delta Copy Packet. Used in DTA_LC chunks.
	 * \param data pointer to the first pixel to copy
	 * \param count the amount of pixels to copy
	 * \param bpp the bit depth of the Flic Animation
	 * \param pixelSkip the amount of pixels to skip before this packet
	 * \param packets the vector to append the resulting packet to
	 */
	void writeDeltaCopyPacket(const uint8_t *data, int32_t count, uint8_t bpp, uint8_t pixelSkip, std::vector<Packet> &packets);

	/**
	 * Encodes the specified line of pixels as DTA_BRUN packets.
	 * \param data pointer to the first pixel in the line to encode
	 * \param width the width of the line in pixels
	 * \param bpp the bit depth of the Flic Animation
	 * \param packets the vector to append the resulting packets to
	 */
	void encodeRle(const uint8_t *data, uint32_t width, uint8_t bpp, std::vector<Packet> &packets);

	/**
	 * Compares a line of pixels to the same line of the previous frame and determines which pixels have been updated.
	 * The resulting sub-chunks can then be encoded separately.
	 * \param data pointer to the first pixel in the line to encode
	 * \param oldData pointer to the first pixel in the same line of the previous frame
	 * \param width the width of the line in pixels
	 * \param bpp the bit depth of the Flic Animation
	 * \param subchunks the vector to append the resulting subchunks to
	 */
	void getSubChunks(const uint8_t *data, const uint8_t *oldData, uint32_t width, uint8_t bpp, std::vector<SubChunk> &subChunks);

	/**
	 * Encodes the specified line of pixels as DTA_LC packets.
	 * \param data pointer to the first pixel in the line to encode
	 * \param oldData pointer to the first pixel in the same line of the previous frame
	 * \param width the width of the line in pixels
	 * \param bpp the bit depth of the Flic Animation
	 * \param packets the vector to append the resulting packets to
	 */
	void encodeDeltaRle(const uint8_t *data, const uint8_t *oldData, uint32_t width, uint8_t bpp, std::vector<Packet> &packets);

	/**
	 * Creates a DTA_BRUN chunk by RLE-encoding a bitmap file.
	 * \param header the header of the Flic Animation file being created
	 * \param bmp the bitmap to encode
	 * \param os the output stream to write the resulting chunk to
	 * \returns the size of the chunk in bytes
	 */
	uint32_t createBrun(const FlicHeader &header, const Bitmap &bmp, std::ostream &os);

	/**
	 * Creates a DTA_LC chunk by comparing a bitmap file to the last frame and RLE-encoding the updated pixels.
	 * \param header the header of the Flic Animation file being created
	 * \param lastBmp the previous bitmap
	 * \param bmp the current bitmap
	 * \param os the output stream to write the resulting chunk to
	 * \returns the size of the chunk in bytes
	 */
	uint32_t createLc(const FlicHeader &header, const Bitmap &lastBmp, const Bitmap &bmp, std::ostream &os);

	/**
	 * Reads a DTA_BRUN chunk and updates the specified Flic frame.
	 * \param header the header of the Flic Animation file being created
	 * \param frame the frame to update
	 * \param is the input stream from which to read chunk data
	 */
	void readBrun(const FlicHeader &header, FlicFrame &frame, std::istream &is);

	/**
	 * Reads a DTA_LC chunk and updates the specified Flic frame.
	 * This function assumes that the frame is identical pixel-wise to the previous frame.
	 * \param header the header of the Flic Animation file being created
	 * \param frame the frame to update
	 * \param is the input stream from which to read chunk data
	 */
	void readLc(const FlicHeader &header, FlicFrame &frame, std::istream &is);

	/**
	 * Outputs a nice progress bar.
	 * \param x current progress
	 * \param n maximum progress
	 * \param w width of the bar in characters
	 */
	static inline void progressBar(uint32_t x, uint32_t n, uint32_t w);

	std::vector<FlicFrame> frames_;
};

#endif // FLICTOOL_FLIC_H
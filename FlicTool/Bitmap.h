#pragma once
#ifndef FLICTOOL_BITMAP_H
#define FLICTOOL_BITMAP_H

#include <cstdint>
#include <string>

#pragma pack(push, 1)
struct BitmapFileHeader {
	char magic[2];
	uint32_t fileSize;
	uint32_t reserved;
	uint32_t pixelOffset;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct BitmapInfoHeader {
	uint32_t infoHeaderSize;
	uint32_t width;
	uint32_t height;
	uint16_t planes;
	uint16_t bpp;
	uint32_t compression;
	uint32_t imageSize;
	uint32_t ppmX;
	uint32_t ppmY;
	uint32_t paletteColors;
	uint32_t importantColors;
};
#pragma pack(pop)

class Bitmap {
public:
	/**
	 * Default constructor.
	 */
	Bitmap();

	/**
	 * Creates a bitmap from the specified data and values.
	 * See \code create \endcode for more info.
	 * \param pixels the pixel data of the bitmap
	 * \param width the desired width of the bitmap
	 * \param height the desired height of the bitmap
	 * \param bpp the desired bit depth of the bitmap
	 */
	Bitmap(uint8_t *pixels, int width, int height, int bpp);

	/**
	 * Creates a bitmap from the specified data and values.
	 * See \code create \endcode for more info.
	 * \param pixels the pixel data of the bitmap
	 * \param width the desired width of the bitmap
	 * \param height the desired height of the bitmap
	 * \param bpp the desired bit depth of the bitmap
	 */
	void create(uint8_t *pixels, int width, int height, int bpp);

	/**
	 * Loads a bitmap from the specified file.
	 * \param path the path of the bitmap file to load
	 */
	bool load(const std::string &path);

	/**
	 * Saves the bitmap to the specified file.
	 * \param path the path of the bitmap file to save
	 */
	bool save(const std::string &path);

	/**
	 * \returns a pointer to the pixel data of this bitmap
	 */
	const uint8_t *pixels() const;

	/**
	 * \returns the width of this bitmap
	 */
	const uint32_t width() const;

	/**
	 * \returns the height of this bitmap
	 */
	const uint32_t height() const;
private:
	uint8_t *pixels_;
	BitmapFileHeader header_;
	BitmapInfoHeader infoHeader_;
};

#endif // FLICTOOL_BITMAP_H
#include <FlicTool/Bitmap.h>

#include <cstring>
#include <fstream>
#include <iostream>

Bitmap::Bitmap() {
}

Bitmap::Bitmap(uint8_t *pixels, int width, int height, int bpp) {
	create(pixels, width, height, bpp);
}

void Bitmap::create(uint8_t *pixels, int width, int height, int bpp) {
	pixels_ = pixels;

	memset(&header_, 0, sizeof(header_));
	header_.magic[0] = 'B';
	header_.magic[1] = 'M';
	header_.fileSize = 0x36 + (width * height * (bpp / 8));
	header_.pixelOffset = 0x36;

	memset(&infoHeader_, 0, sizeof(infoHeader_));
	infoHeader_.infoHeaderSize = 0x28;
	infoHeader_.width = width;
	infoHeader_.height = height;
	infoHeader_.planes = 1;
	infoHeader_.bpp = bpp;
	infoHeader_.compression = 0;
	infoHeader_.imageSize = width * height;
	infoHeader_.ppmX = 96;
	infoHeader_.ppmY = 96;
	infoHeader_.paletteColors = 0;
	infoHeader_.importantColors = 0;
}

bool Bitmap::load(const std::string &path) {
	std::ifstream ifs(path, std::ios_base::binary);

	if (!ifs.is_open()) {
		return false;
	}

	ifs.read((char*)&header_, sizeof(header_));
	ifs.read((char*)&infoHeader_, sizeof(infoHeader_));

	if (infoHeader_.bpp != 16) {
		std::cerr << "Error: Bit depths other than 16 bits are not supported at the moment.\n";
		return false;
	}

	// If the compression is 3, 12 bytes are used to specify the bitmask of a pixel
	if (infoHeader_.compression == 3) {
		uint32_t redMask, blueMask, greenMask;
		ifs.read((char*)&redMask, 4);
		ifs.read((char*)&greenMask, 4);
		ifs.read((char*)&blueMask, 4);
		if (redMask != 0x7c00 || greenMask != 0x3e0 || blueMask != 0x1f) {
			std::cerr << "Error: Bit masks other than 0RRRRRGGGGGBBBBB are not supported at the moment.\n";
			return false;
		}
	}

	pixels_ = new uint8_t[infoHeader_.width * infoHeader_.height * (infoHeader_.bpp / 8)];
	
	uint32_t pitch = infoHeader_.width * (infoHeader_.bpp / 8);
	uint32_t padding = pitch % 4 == 0 ? 0 : (4 - pitch % 4);
	for (uint32_t y=0; y<infoHeader_.height; ++y) {
		uint32_t offset = y * pitch;
		ifs.read((char*)(pixels_ + offset), pitch);
		if (padding) ifs.seekg(padding, std::ios_base::cur);
	}

	return true;
}

bool Bitmap::save(const std::string &path) {
	std::ofstream ofs(path, std::ios_base::binary);

	if (!ofs.is_open()) {
		return false;
	}

	ofs.write((char*)&header_, sizeof(header_));
	ofs.write((char*)&infoHeader_, sizeof(infoHeader_));
	
	int pitch = infoHeader_.width * (infoHeader_.bpp / 8);

	// Bitmap pixel data is aligned on 4-byte boundary, so we need to take
	// that into account when saving it
	int padMod = pitch % 4;
	int padCount = padMod > 0 ? 4 - padMod : 0;
	char *padding = nullptr;
	if (padMod != 0) {
		padding = new char[padCount];
		memset(padding, 0, padCount);
	}
	for (uint32_t y=0; y<infoHeader_.height; ++y) {
		char *start = (char*)(pixels_ + y * pitch);
		ofs.write(start, pitch);
		if (padCount > 0)
			ofs.write(padding, padCount);
	}
	if (padding)
		delete padding;

	return true;
}

const uint8_t *Bitmap::pixels() const {
	return pixels_;
}

const uint32_t Bitmap::width() const {
	return infoHeader_.width;
}

const uint32_t Bitmap::height() const {
	return infoHeader_.height;
}
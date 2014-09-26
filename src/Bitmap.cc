#include <FlicTool/Bitmap.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>

Bitmap::Bitmap() : pixels_(nullptr) {
}

Bitmap::Bitmap(uint8_t *pixels, int width, int height, int bpp) : pixels_(nullptr) {
	create(pixels, width, height, bpp);
}

void Bitmap::create(uint8_t *pixels, int width, int height, int bpp) {
	pixels_.reset(pixels);

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

	ifs.read(reinterpret_cast<char*>(&header_), sizeof(header_));
	ifs.read(reinterpret_cast<char*>(&infoHeader_), sizeof(infoHeader_));

	uint32_t bitMask[4] = { 0 };
	switch (infoHeader_.bpp) {
		case 8:
			std::cerr << "Error: Bitmaps with a lower bit depth than 16 are not supported at the moment.\n";
			return false;
		case 16:
			for (int i=0; i<3; ++i) bitMask[i] = (0x1f << (i * 5));
			break;
		case 24:
			for (int i=0; i<3; ++i) bitMask[i] = (0xff << (i * 8));
			break;
		case 32:
			for (int i=0; i<4; ++i) bitMask[i] = (0xff << (i * 8));
			std::cerr << "Warning: Alpha channel will be discarded since it is not supported by Rock Raiders.\n";
			break;
		default:
			std::cerr << "Error: Unsupported bit depth: " << infoHeader_.bpp << '\n';
			return false;
	}
	
	switch (infoHeader_.compression) {
	case BI_RGB:
		break;
	case BI_BITFIELDS:
		for (int i=0; i<3; ++i) ifs.read(reinterpret_cast<char*>(&bitMask[2 - i]), 4);
		break;
	case BI_ALPHABITFIELDS:
		for (int i=0; i<4; ++i) ifs.read(reinterpret_cast<char*>(&bitMask[3 - i]), 4);
		break;
	default:
		std::cerr << "Error: Unrecognized bitmap compression method.\n";
		return false;
	}
	
	switch (infoHeader_.infoHeaderSize) {
	case 40: // BITMAPINFOHEADER
	case 52: // BITMAPV2INFOHEADER
	case 56: // BITMAPV3INFOHEADER
		break;
	case 108: // BITMAPV4HEADER
	case 124: { // BITMAPV5HEADER
		char signature[5];
		ifs.read(signature, 4);
		signature[4] = 0;
		if (strcmp(signature, "BGRs") != 0) {
			std::cerr << "Error: Unsupported color space. Expected \"BGRs\", got \"" << signature << "\"\n";
			return false;
		}
		break;
	}
	default:
		std::cerr << "Error: Unrecognized bitmap header type.\n";
		return false;
	}
	
	ifs.seekg(header_.pixelOffset);
	
	uint8_t *original = new uint8_t[infoHeader_.width * infoHeader_.height * (infoHeader_.bpp / 8)];

	uint32_t pitch = infoHeader_.width * (infoHeader_.bpp / 8);
	uint32_t padding = pitch % 4 == 0 ? 0 : (4 - pitch % 4);
	for (uint32_t y = 0; y < infoHeader_.height; ++y) {
		uint32_t offset = y * pitch;
		ifs.read(reinterpret_cast<char*>(original + offset), pitch);
		if (padding) ifs.seekg(padding, std::ios_base::cur);
	}
	
	if (infoHeader_.bpp == 16) {
		pixels_.reset(original);
	} else {
		pixels_.reset(downsamplePixels(original, infoHeader_.width, infoHeader_.height, infoHeader_.bpp, bitMask));
		delete original;
	}

	return true;
}

bool Bitmap::save(const std::string &path) {
	std::ofstream ofs(path, std::ios_base::binary);

	if (!ofs.is_open()) {
		return false;
	}

	ofs.write(reinterpret_cast<char*>(&header_), sizeof(header_));
	ofs.write(reinterpret_cast<char*>(&infoHeader_), sizeof(infoHeader_));
	
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
	for (uint32_t y = 0; y < infoHeader_.height; ++y) {
		char *start = reinterpret_cast<char*>(pixels_.get() + y * pitch);
		ofs.write(start, pitch);
		if (padCount > 0)
			ofs.write(padding, padCount);
	}
	if (padding)
		delete padding;

	return true;
}

const uint8_t *Bitmap::pixels() const {
	return pixels_.get();
}

const uint32_t Bitmap::width() const {
	return infoHeader_.width;
}

const uint32_t Bitmap::height() const {
	return infoHeader_.height;
}

uint8_t *Bitmap::downsamplePixels(uint8_t *original, uint32_t width, uint32_t height, uint32_t bpp, uint32_t *bitMask) {
	std::cout << "Downsampling " << bpp << " to 16.\n";
	uint32_t length = width * height;
	uint8_t *result = new uint8_t[length * 2];
	uint32_t stride = bpp / 8;
	
	uint32_t bitShift[4] = { 0 };
	for (int i = 0; i < 4; ++i) {
		if (bitMask[i] == 0) continue;
		int n = 0;
		for (uint32_t p = bitMask[i]; (p & 1) == 0; p >>= 1) ++n;
		bitShift[i] = n;
		std::cout << "Bitmask: " << std::hex << bitMask[i] << std::dec << ", Shift: " << n << std::endl;
	}
	
	for (uint32_t i=0; i<length; ++i) {
		uint32_t pixel = *(uint32_t*)(original + i * stride);
		/*float red = (pixel & bitMask[2]) >> bitShift[2];
		float green = (pixel & bitMask[1]) >> bitShift[1];
		float blue = (pixel & bitMask[0]) >> bitShift[0];
		int newRed = std::round(red / (bitMask[2] >> bitShift[2]) * 31);
		int newGreen = std::round(green / (bitMask[1] >> bitShift[1]) * 31);
		int newBlue = std::round(blue / (bitMask[0] >> bitShift[0]) * 31);
		uint16_t newPixel = ((newRed & 0x1f) << 10) |
							((newGreen & 0x1f) << 5) |
							(newBlue & 0x1f);*/
		uint16_t newPixel = 0;
		for (int j=0; j<3; ++j) {
			float c = (pixel & bitMask[j]) >> bitShift[j];
			int newC = std::round(c / (bitMask[j] >> bitShift[j]) * 31);
			newPixel |= ((newC & 0x1f) << (j * 5));
		}
		*(uint16_t*)(result + i * 2) = newPixel;
	}
	
	return result;
}
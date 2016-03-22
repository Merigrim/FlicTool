#include <FlicTool/Flic.h>

#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

void Flic::compile(const std::string &input, const std::string &output) {
	std::cout << "Compiling \"" << input << "\" > \"" << output << "\"\n";

	// First, we need to find all the frames to compile and load their data
	// Note: 16-bit bitmaps aren't very big, but let's load them lazily in the
	// next version
	std::vector<std::string> frameFilenames;
	std::regex frameFilter("frame[0-9]{4}.bmp");
	fs::directory_iterator endIter;
	for (fs::directory_iterator iter(input); iter != endIter; ++iter) {
		if (!fs::is_regular_file(iter->status())) continue;

		auto fileName = iter->path().filename().string();
		if (!std::regex_match(fileName, frameFilter)) continue;

		frameFilenames.push_back(iter->path().string());
	}
	if (frameFilenames.size() == 0) {
		std::cerr << "Error: No frames found in input folder.\n";
		return;
	}
	std::cout << "Found " << frameFilenames.size() << " frames in input folder.\n";
	std::vector<Bitmap> bitmaps;
	for (const auto &filename : frameFilenames) {
		Bitmap bmp;
		if (!bmp.load(filename)) {
			std::cerr << "Error: Program can't continue due to an invalid frame.\n";
			return;
		}
		bitmaps.push_back(bmp);
	}

	// Create the header, but ignore the size field for now, since we haven't calculated it yet
	FlicHeader header = { 0 };
	header.magic = 0xaf43;
	header.frames = frameFilenames.size();
	header.width = bitmaps[0].width();
	header.height = bitmaps[0].height();
	header.depth = 16;
	
	std::ofstream ofs(output, std::ios_base::binary);
	ofs.write(reinterpret_cast<char*>(&header), sizeof(header));

	// We need to grab the size of the first frame since FLH files have some
	// weird offset to the end of the first frame in the header
	uint32_t frameSize = createBrun(header, bitmaps[0], ofs);
	int64_t cur = ofs.tellp();
	ofs.seekp(0x50, std::ios_base::beg);
	uint32_t magic80 = 0x80;
	uint32_t unknownValue = frameSize + 0x80;
	ofs.write(reinterpret_cast<char*>(&magic80), 4);
	ofs.write(reinterpret_cast<char*>(&unknownValue), 4);
	ofs.seekp(cur, std::ios_base::beg);
	progressBar(1, header.frames, 50);
	for (int i = 1; i < header.frames; ++i) {
		createLc(header, bitmaps[i-1], bitmaps[i], ofs);
		progressBar((i + 1), header.frames, 50);
	}
	std::cout << "\n";

	// With the file completed we can grab the size and write it to the header
	int32_t size = (int32_t)ofs.tellp();
	ofs.seekp(0, std::ios_base::beg);
	ofs.write(reinterpret_cast<char*>(&size), 4);
}

void Flic::writeRepeatPacket(const uint8_t *data, int32_t count, uint8_t bpp, std::vector<Packet> &packets) {
	Packet packet;
	packet.size = bpp + 1;
	packet.data = new char[packet.size];
	memcpy(packet.data + 1, data, bpp);
	packet.data[0] = count;
	packets.push_back(packet);
}
void Flic::writeCopyPacket(const uint8_t *data, int32_t count, uint8_t bpp, std::vector<Packet> &packets) {
	Packet packet;
	packet.size = count * bpp + 1;
	packet.data = new char[packet.size];
	memcpy(packet.data + 1, data, count * bpp);
	packet.data[0] = -count;
	packets.push_back(packet);
}

void Flic::encodeRle(const uint8_t *data, uint32_t width, uint8_t bpp, std::vector<Packet> &packets) {
	uint32_t bytesEncoded = 0;
	uint32_t offset = 0;
	const uint8_t *last = nullptr, *last2 = nullptr;
	bool repeat = false;
	int count = 0;
	const int repeatBuffer = 1;
	while (bytesEncoded < width * bpp) {
		if (offset >= width * bpp) {
			// Reached end of line but haven't encoded all of it yet.
			if (repeat) {
				writeRepeatPacket(data + bytesEncoded, count, bpp, packets);
			} else {
				writeCopyPacket(data + bytesEncoded, count, bpp, packets);
			}
			break;
		}
		const uint8_t *p = data + offset;
		if (last && last2 && !repeat && memcmp(p, last, bpp) == 0) {
			repeat = true;
			if (count > repeatBuffer) {
				count -= repeatBuffer;
				writeCopyPacket(data + bytesEncoded, count, bpp, packets);
				bytesEncoded += count * bpp;
				count = repeatBuffer;
			}
		} else if (last && repeat && memcmp(p, last, bpp) != 0) {
			repeat = false;
			writeRepeatPacket(data + bytesEncoded, count, bpp, packets);
			bytesEncoded += count * bpp;
			count = 0;
		}
		last = p;
		last2 = last;
		offset += bpp;
		++count;
	}
}

uint32_t Flic::createBrun(const FlicHeader &header, const Bitmap &bmp, std::ostream &os) {
	FlicFrameHeader frameHeader = { 0 };
	frameHeader.magic = 0xf1fa;
	frameHeader.chunks = 1;
	// We don't know the size of this frame yet so leave it blank for now
	int32_t frameOffset = (int32_t)os.tellp();
	os.write(reinterpret_cast<char*>(&frameHeader), sizeof(frameHeader));
	FlicChunkHeader chunkHeader = { 0 };
	chunkHeader.type = FlicChunkType::FLI_DTA_BRUN;
	// We don't know the size of the chunk either
	int32_t chunkOffset = (int32_t)os.tellp();
	os.write(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));

	size_t pitch = header.width * (header.depth / 8);
	for (int y = header.height - 1; y >= 0; --y) {
		const uint8_t *line = bmp.pixels() + y * pitch;
		std::vector<Packet> packets;
		encodeRle(line, header.width, header.depth / 8, packets);
		uint8_t packetCount = packets.size();
		os.write(reinterpret_cast<char*>(&packetCount), 1);
		for (const auto &packet : packets) {
			os.write(packet.data, packet.size);
			delete packet.data;
		}
	}

	// Now we need to go back to the fields we didn't know the values of before and fill them in
	int32_t frameEnd = (int32_t)os.tellp();
	os.seekp(chunkOffset, std::ios_base::beg);
	int32_t chunkSize = frameEnd - chunkOffset;
	os.write(reinterpret_cast<char*>(&chunkSize), 4);
	os.seekp(frameOffset, std::ios_base::beg);
	int32_t frameSize = frameEnd - frameOffset;
	os.write(reinterpret_cast<char*>(&frameSize), 4);
	os.seekp(frameEnd, std::ios_base::beg);

	return frameSize;
}

void Flic::writeDeltaRepeatPacket(const uint8_t *data, int32_t count, uint8_t bpp, uint8_t pixelSkip, std::vector<Packet> &packets) {
	Packet packet;
	packet.size = bpp + 2;
	packet.data = new char[packet.size];
	memcpy(packet.data + 2, data, bpp);
	packet.data[0] = pixelSkip;
	packet.data[1] = -count;
	packets.push_back(packet);
}
void Flic::writeDeltaCopyPacket(const uint8_t *data, int32_t count, uint8_t bpp, uint8_t pixelSkip, std::vector<Packet> &packets) {
	Packet packet;
	packet.size = count * bpp + 2;
	packet.data = new char[packet.size];
	memcpy(packet.data + 2, data, count * bpp);
	packet.data[0] = pixelSkip;
	packet.data[1] = count;
	packets.push_back(packet);
}

void Flic::getSubChunks(const uint8_t *data, const uint8_t *oldData, uint32_t width, uint8_t bpp, std::vector<SubChunk> &subChunks) {
	uint8_t pixelSkip = 0;
	const uint8_t *subChunkStart = 0;
	uint32_t subChunkLength = 0;
	uint32_t offset = 0;
	while (offset < width * bpp) {
		const uint8_t *p = data + offset;
		const uint8_t *oldP = oldData + offset;
		if (memcmp(p, oldP, bpp) == 0) {
			// If we are in the middle of reading a sub-chunk when encountering
			// a non-updated pixel, we append the subchunk to our vector and
			// start over
			if (subChunkLength > 0) {
				SubChunk subChunk;
				subChunk.length = subChunkLength;
				subChunk.pixelSkip = pixelSkip;
				subChunk.start = subChunkStart;
				subChunks.push_back(subChunk);
				pixelSkip = 0;
				subChunkLength = 0;
			}
			++pixelSkip;
		} else {
			// If we aren't currently reading a sub-chunk, we store the position
			// of the first pixel in the next sub-chunk
			if (subChunkLength == 0) {
				subChunkStart = p;
			}
			++subChunkLength;
		}
		offset += bpp;
	}
	// If we have a sub-chunk in progress, append it
	if (subChunkLength > 0) {
		SubChunk subChunk;
		subChunk.length = subChunkLength;
		subChunk.pixelSkip = pixelSkip;
		subChunk.start = subChunkStart;
		subChunks.push_back(subChunk);
	}
}

void Flic::encodeDeltaRle(const uint8_t *data, const uint8_t *oldData, uint32_t width, uint8_t bpp, std::vector<Packet> &packets) {
	std::vector<SubChunk> subChunks;
	getSubChunks(data, oldData, width, bpp, subChunks);
	for (const auto &subChunk : subChunks) {
		const uint8_t *last = nullptr;
		bool repeat = false;
		uint32_t bytesEncoded = 0, count = 0, offset = 0;
		int lastSkip = subChunk.pixelSkip;
		while (bytesEncoded < subChunk.length * bpp) {
			if (offset >= subChunk.length * bpp) {
				// Reached end of line but haven't encoded all of it yet.
				if (repeat) {
					writeDeltaRepeatPacket(subChunk.start + bytesEncoded, count, bpp, lastSkip, packets);
				} else {
					writeDeltaCopyPacket(subChunk.start + bytesEncoded, count, bpp, lastSkip, packets);
				}
				lastSkip = 0;
				break;
			}
			const uint8_t *p = subChunk.start + offset;
			if (last && !repeat && memcmp(p, last, bpp) == 0) {
				repeat = true;
				if (count > 1) {
					count -= 1;
					writeDeltaCopyPacket(subChunk.start + bytesEncoded, count, bpp, lastSkip, packets);
					lastSkip = 0;
					bytesEncoded += count * bpp;
					count = 1;
				}
			} else if (last && repeat && memcmp(p, last, bpp) != 0) {
				repeat = false;
				writeDeltaRepeatPacket(subChunk.start + bytesEncoded, count, bpp, lastSkip, packets);
				lastSkip = 0;
				bytesEncoded += count * bpp;
				count = 0;
			}
			last = p;
			offset += bpp;
			++count;
		}
	}
}

uint32_t Flic::createLc(const FlicHeader &header, const Bitmap &lastBmp, const Bitmap &bmp, std::ostream &os) {
	FlicFrameHeader frameHeader = { 0 };
	frameHeader.magic = 0xf1fa;
	frameHeader.chunks = 1;
	// We don't know the size of this frame yet so leave it blank for now
	int32_t frameOffset = (int32_t)os.tellp();
	os.write(reinterpret_cast<char*>(&frameHeader), sizeof(frameHeader));
	FlicChunkHeader chunkHeader = { 0 };
	chunkHeader.type = FlicChunkType::FLI_DTA_LC;
	// We don't know the size of the chunk either
	int32_t chunkOffset = (int32_t)os.tellp();
	os.write(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
	
	// We don't know the number of lines to update yet, so we will leave a
	// spot for the line count here
	int32_t lineOffset = (int32_t)os.tellp();
	int16_t nil = 0;
	os.write(reinterpret_cast<char*>(&nil), 2);

	size_t pitch = header.width * (header.depth / 8);
	int16_t lineSkip = 0;
	uint16_t lines = 0;
	for (int y = header.height - 1; y >= 0; --y) {
		const uint8_t *line = bmp.pixels() + y * pitch;
		const uint8_t *lastLine = lastBmp.pixels() + y * pitch;
		if (memcmp(line, lastLine, pitch) == 0) {
			// Line is exactly the same, skip it
			++lineSkip;
			continue;
		} else {
			if (lineSkip > 0) {
				lineSkip = -lineSkip;
				os.write(reinterpret_cast<char*>(&lineSkip), 2);
			}
			std::vector<Packet> packets;
			encodeDeltaRle(line, lastLine, header.width, header.depth / 8, packets);
			uint16_t packetCount = packets.size();
			os.write(reinterpret_cast<char*>(&packetCount), 2);
			for (const auto &packet : packets) {
				os.write(packet.data, packet.size);
				delete packet.data;
			}
			++lines;
			lineSkip = 0;
		}
	}

	// Now we need to go back to the fields we didn't know the values of before and fill them in
	int32_t frameEnd = (int32_t)os.tellp();
	os.seekp(lineOffset, std::ios_base::beg);
	os.write(reinterpret_cast<char*>(&lines), 2);
	os.seekp(chunkOffset, std::ios_base::beg);
	int32_t chunkSize = frameEnd - chunkOffset;
	os.write(reinterpret_cast<char*>(&chunkSize), 4);
	os.seekp(frameOffset, std::ios_base::beg);
	int32_t frameSize = frameEnd - frameOffset;
	os.write(reinterpret_cast<char*>(&frameSize), 4);
	os.seekp(frameEnd, std::ios_base::beg);

	return frameSize;
}

void Flic::decompile(const std::string &input, const std::string &output) {
	std::cout << "Decompiling \"" << input << "\" > \"" << output << "\"\n";

	std::ifstream ifs(input, std::ios_base::binary);

	FlicHeader header;
	ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (header.magic != 0xaf43) {
		std::cerr << "Error: Flic file is not a valid Rock Raiders Flic file!" << std::endl;
		return;
	}
	for (uint32_t i = 0; i < header.frames; ++i) {
		FlicFrameHeader frameHeader;
		ifs.read(reinterpret_cast<char*>(&frameHeader), sizeof(frameHeader));
		FlicFrame frame;
		for (uint32_t c = 0; c < frameHeader.chunks; ++c) {
			FlicChunkHeader chunkHeader;
			ifs.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
			frame.pixels = new uint8_t[header.width * header.height * (header.depth / 8)];
			switch (chunkHeader.type) {
			case FLI_DTA_BRUN:
				readBrun(header, frame, ifs);
				break;
			case FLI_DTA_LC:
				readLc(header, frame, ifs);
				break;
			default:
				std::cerr << "Warning: Unknown chunk type: " << chunkHeader.type << std::endl;
				ifs.seekg(chunkHeader.size - sizeof(chunkHeader), std::ios_base::cur);
				break;
			}
		}
		frames_.push_back(frame);
		progressBar((i + 1), header.frames, 50);
	}
	std::cout << "\n";

	fs::path outputPath(output);

	for (size_t i = 0; i < frames_.size(); ++i) {
		const auto &frame = frames_[i];
		std::ostringstream frameName;
		frameName << "frame" << std::setw(4) << std::setfill('0') << (i + 1) << ".bmp";
		Bitmap bmp(frame.pixels, header.width, header.height, header.depth);
		if (!bmp.save((outputPath / frameName.str()).string())) {
			std::cerr << "Error: Writing bitmap " << (outputPath / frameName.str()) << " failed.\n";
			return;
		}
	}
}

void Flic::readBrun(const FlicHeader &header, FlicFrame &frame, std::istream &is) {
	int bytespp = header.depth / 8;
	for (int y = 0; y < header.height; ++y) {
		uint8_t packets;
		is.read(reinterpret_cast<char*>(&packets), 1);
		int x = 0;
		while (x < header.width) {
			int offset = ((header.height - y - 1) * header.width + x) * bytespp;
			char count;
			is.read(&count, 1);
			if (count >= 0) {
				char *tmp = new char[bytespp];
				is.read(tmp, bytespp);
				for (int j = 0; j < count; ++j) {
					memcpy(frame.pixels + offset + j * bytespp, tmp, bytespp);
				}
				x += count;
				delete tmp;
			} else {
				for (int j = 0; j < (-count); ++j) {
					char *tmp = new char[bytespp];
					is.read(tmp, bytespp);
					memcpy(frame.pixels + offset + j * bytespp, tmp, bytespp);
					delete tmp;
				}
				x += -count;
			}
		}
	}
}

void Flic::readLc(const FlicHeader &header, FlicFrame &frame, std::istream &is) {
	int bytespp = header.depth / 8;
	memcpy(frame.pixels, frames_.back().pixels, header.width * header.height * bytespp);
	uint16_t lines;
	is.read((char*)&lines, 2);
	int j = 0, y = 0;
	while (j < lines) {
		int16_t lineSkip;
		is.read(reinterpret_cast<char*>(&lineSkip), 2);
		if (lineSkip < 0) {
			y += -lineSkip;
			continue;
		} else {
			int packets = lineSkip;
			int x = 0;
			for (int k = 0; k < packets; ++k) {
				uint8_t pixelSkip;
				is.read((char*)&pixelSkip, 1);
				x += pixelSkip;
				int offset = ((header.height - y - 1) * header.width + x) * bytespp;
				char count;
				is.read(&count, 1);
				if (count < 0) {
					char *tmp = new char[bytespp];
					is.read(tmp, bytespp);
					for (int j = 0; j < -count; ++j) {
						memcpy(frame.pixels + offset + j * bytespp, tmp, bytespp);
					}
					x += -count;
					delete tmp;
				} else {
					for (int j = 0; j < count; ++j) {
						char *tmp = new char[bytespp];
						is.read(tmp, bytespp);
						memcpy(frame.pixels + offset + j * bytespp, tmp, bytespp);
						delete tmp;
					}
					x += count;
				}
			}
			++y;
		}
		++j;
	}
}

inline void Flic::progressBar(uint32_t x, uint32_t n, uint32_t w) {
	if ((x != n) && (x % ((n / 100) + 1) != 0)) return;
 
	float ratio = x / static_cast<float>(n);
	uint32_t c = (uint32_t)(ratio * w);
 
	for (uint32_t i = 0; i < w + 8; ++i) {
		std::cout << "\b";
	}
    std::cout << std::setw(4) << static_cast<int>(ratio * 100) << "% [";
 
    for (uint32_t x = 0; x < c; x++)
       std::cout << "=";
 
    for (uint32_t x = c; x < w; x++)
       std::cout << " ";
 
    std::cout << "]";
	std::flush(std::cout);
}
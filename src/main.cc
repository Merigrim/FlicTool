/*****************************************************************************
 * Flic Tool
 *  Version: 1.0dev
 *  Creator: Merigrim (http://www.rockraidersunited.org/user/4758-merigrim/)
 *****************************************************************************/

#include <functional>
#include <iostream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <FlicTool/Flic.h>

#define FLICTOOL_VERSION "1.0a"

namespace fs = boost::filesystem;
namespace po = boost::program_options;

void showHelp(const po::options_description &desc) {
	std::cout << "FlicTool " FLICTOOL_VERSION "\nCopyright (c) 2014 Merigrim (https://github.com/Merigrim)\n\n" << desc;
}

bool prompt() {
	std::string line;
	std::getline(std::cin, line);
	std::transform(line.begin(), line.end(), line.begin(), ::toupper);
	if (line.length() > 0 && line[0] == 'Y') {
		return true;
	}
	return false;
}

int main(int argc, char **argv) {
	po::options_description desc;
	std::string input, output;
	desc.add_options()
		("help", "show program help")
		("input,i", po::value<std::string>(&input), "input path to either a Flic file to decompile or a directory of bitmaps to compile")
		("output,o", po::value<std::string>(&output)->default_value(""), "output path to either the compiled Flic file or a directory to put decompiled frames in")
	;
	po::positional_options_description pdesc;
	pdesc.add("input", 1);
	pdesc.add("output", 2);

	po::variables_map vm;
	try {
		po::store(po::command_line_parser(argc, argv).options(desc).positional(pdesc).run(), vm);
	} catch (po::unknown_option &e) {
		showHelp(desc);
		return 1;
	}
	po::notify(vm);

	// If the user has forgotten to write any arguments or explicitly requested help
	if (!vm.count("input") || vm.count("help")) {
		showHelp(desc);
		return 1;
	}

	// Make sure the input path actually exists
	if (!fs::exists(input)) {
		std::cerr << "Error: Invalid input path specified: \"" << input << "\" does not exist.\n";
		return 1;
	}

	bool compiling = fs::is_directory(input);
	
	if (output.empty()) {
		// We need different default output filenames depending on the desired action
		if (compiling) {
			output = "output.flh";
		} else {
			output = "output";
		}
	}

	if (fs::exists(output)) {
		// We need to check if the user is about to accidentally overwrite already existing files
		if (fs::is_regular_file(output)) {
			std::cout << "Warning: Output file \"" << output << "\" already exists. Overwrite it? (Y to overwrite, default: no) ";
			if (!prompt()) {
				return 0;
			}
		} else if (fs::is_directory(output)) {
			// Check if there are any files in the output directory. Instead of looping through an
			// unknown number of files and checking file names, we just check if the folder isn't
			// empty, in which case we warn the user
			int count = std::count_if(fs::directory_iterator(output), fs::directory_iterator(),
						std::bind(static_cast<bool(*)(const fs::path&)>(fs::is_regular_file), 
						std::bind(&fs::directory_entry::path, std::placeholders::_1)));
			if (count > 0) {
				std::cout << "Warning: Output directory \"" << output << "\" isn't empty. Overwrite any existing frames (if there are any)? (Y to overwrite, default: no) ";
				if (!prompt()) {
					return 0;
				}
			}
		}
	} else if (!compiling) { // If we're decompiling but the output directory doesn't exist, we need to create it
		if (!fs::create_directories(output)) {
			std::cerr << "Error: Unable to create output directory \"" << output << "\". Please make sure that your permissions are set up correctly." << std::endl;
			return 1;
		}
	}

	Flic flic;
	if (compiling) {
		flic.compile(input, output);
	} else {
		flic.decompile(input, output);
	}

	return 0;
}
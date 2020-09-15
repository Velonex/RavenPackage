#include "RavenPackage.h"

#include <filesystem>
#include <fstream>

namespace rvn {
	int package::createArchiveFromDir(const std::string& dirPath, const std::string& archivePath, bool overrideOldTarget)
	{
		if (!std::filesystem::exists(dirPath)) {
			/* The input doesn't exist */
			RPK_ERROR("Input directory doesn't exist");
			return RPK_INPUT_DOESNT_EXIST;
		}
		else if (!std::filesystem::is_directory(dirPath)) {
			/* The input is not a directory */
			RPK_ERROR("Input path is not a directory");
			return RPK_INPUT_NOT_DIRECTORY;
		} 
		else {
			if (std::filesystem::exists(archivePath) && !overrideOldTarget) {
				/* There is already a file with the path of the output */
				RPK_ERROR("Output target already exists. This error can be disabled by setting overrideOldTarget to true");
				return RPK_OUTPUT_EXISTS;
			}
			/* Structure definition */
			using fpath = std::filesystem::path;
			struct Structure {
				struct File {
					File(const std::string& name, const fpath& path) {
						this->name = name;
						this->path = path;
						length = std::filesystem::file_size(path);
					}
					const std::string& getName() const { return name; }
					uint64_t getLength() const { return length; }
					const fpath& getPath() const { return path; }
					int writeToOutput(std::ofstream& out) {
						std::ifstream in(path, std::ios::binary);
						if (in.is_open()) {
							std::string buf(BUF_SIZE, 0x00);
							for (std::uint64_t i = 0; i < (length / BUF_SIZE) + 1; i++) {
								buf = std::string(length - in.tellg() > BUF_SIZE ? BUF_SIZE : length - in.tellg(), 0x00);
								in.read(&buf[0], buf.length());
								out << buf;
							}
						}
						else {
							RPK_ERROR("Couldn't open file '" + name + "'");
							return RPK_COULDNT_OPEN_FILE;
						}
						return RPK_OK;
					}
					std::uint64_t length;
					std::string name;
					fpath path;
				};
				struct Directory {
					Directory(const std::string& name, const fpath& path) {
						this->name = name;
						this->path = path;
						headerlength = 0;
						length = 0;
					}
					void readFiles() {
						for (auto p : std::filesystem::directory_iterator(path)) {
							if (p.is_directory()) {
								Directory dir(p.path().filename().string(), p.path());
								dir.readFiles();
								directories.push_back(dir);
							}
							else if (p.is_regular_file()) {
								File file(p.path().filename().string(), p.path());
								files.push_back(file);
							}
							else {
								RPK_TRACE("Unknown/unsupported file type. Skipping...");
							}
						}
						headerlength = RPK_V1_FILE_COUNT_LENGTH;
						length = 0;
						for (auto file : files) {
							length += file.getLength();
							headerlength += RPK_V1_FILE_HEADER_LENGTH;
							headerlength += file.getName().length();
						}
						for (auto dir : directories) {
							length += dir.getLength() + dir.getHeaderLength();
							headerlength += RPK_V1_DIR_HEADER_LENGTH;
							headerlength += dir.getName().length();
						}
					}
					const std::vector<Directory>& getDirectories() const { return directories; }
					const std::vector<File>& getFiles() const { return files; }
					const std::string& getName() const { return name; }
					const fpath& getPath() const { return path; }
					std::uint64_t getLength() const { return length; }
					std::uint64_t getHeaderLength() const { return headerlength; }
					int writeToOutput(std::ofstream& out) {
						if ((directories.size() + files.size()) > UINT16_MAX) return RPK_TOO_MANY_FILES;
						else {
							std::uint64_t dirStart = out.tellp();
							std::uint64_t currentBegin = dirStart + headerlength;
							out.write(util::convertUint16ToChars((std::uint16_t)files.size() + (std::uint16_t)directories.size()).chars, 2);
							for (auto file : files) {
								out << (char)RPK_TRAIT_IS_FILE;
								out << (std::uint8_t)file.getName().length();
								out << file.getName().substr(0, 255);
								out.write(util::convertUint64ToChars(currentBegin).chars, 8);
								currentBegin += file.getLength();
								out.write(util::convertUint64ToChars(currentBegin).chars, 8);
							}
							for (auto dir : directories) {
								out << (char)0;
								out << (uint8_t)dir.getName().length();
								out << dir.getName().substr(0, 255);
								out.write(util::convertUint64ToChars(currentBegin).chars, 8);
								currentBegin += dir.getLength() + dir.getHeaderLength();
							}
							for (auto file : files) {
								file.writeToOutput(out);
							}
							for (auto dir : directories) {
								dir.writeToOutput(out);
							}
						}
						return RPK_OK;
					}
					std::vector<Directory> directories;
					std::vector<File> files;
					std::uint64_t length, headerlength;
					std::string name;
					fpath path;
				};
				Structure() {}
				void readStructure(const std::string& dirPath) {
					this->dirPath = dirPath;
					for (auto p : std::filesystem::directory_iterator(dirPath)) {
						if (p.is_directory()) {
							Directory dir(p.path().filename().string(), p.path());
							dir.readFiles();
							directories.push_back(dir);
						}
						else if(p.is_regular_file()) {
							File file(p.path().filename().string(), p.path());
							files.push_back(file);
						}
						else {
							RPK_TRACE("Unknown/unsupported file type. Skipping...");
						}
					}
				}
				const std::string& getPath() const { return dirPath; }
				const std::vector<Directory>& getDirectories() const { return directories; }
				const std::vector<File>& getFiles() const { return files; }
				std::string dirPath;
				std::vector<Directory> directories;
				std::vector<File> files;
			};
			/* Structure definition end */
			Structure structure;
			structure.readStructure(dirPath);

			if ((structure.getDirectories().size() + structure.getFiles().size()) > UINT16_MAX) {
				RPK_ERROR("Base directory contains too many files and directories (over 65535)");
				return RPK_TOO_MANY_FILES;
			}
			if ((structure.getDirectories().size() + structure.getFiles().size()) == 0) {
				RPK_ERROR("Base directory is empty");
				return RPK_DIR_IS_EMPTY;
			}

			std::ofstream out(archivePath, std::ios::binary);
			if (!out) {
				RPK_ERROR("Couldn't open output file");
				return RPK_COULDNT_OPEN_FILE;
			}

			out << RPK_MAGIC_NUMBER;

			out << (char)RPK_VERSION_1;

			/* Write file count */
			out.write(util::convertUint16ToChars((std::uint16_t)structure.getFiles().size() 
				+ (std::uint16_t)structure.getDirectories().size()).chars, 2);

			/* Header length calculation */
			std::uint64_t headerLength = RPK_MAGIC_NUMBER_LENGTH + RPK_VERSION_LENGTH + RPK_V1_FILE_COUNT_LENGTH;
			for (auto dir : structure.directories) {
				headerLength += RPK_V1_DIR_HEADER_LENGTH;
				headerLength += dir.getName().length();
			}
			for (auto file : structure.files) {
				headerLength += RPK_V1_FILE_HEADER_LENGTH;
				headerLength += file.getName().length();
			}
			/* Write file headers */
			std::uint64_t currentFileBegin = headerLength;
			for (auto file : structure.getFiles()) {
				out << (char)RPK_TRAIT_IS_FILE;
				out << (std::uint8_t)file.getName().length();
				out << file.getName().substr(0, 255);
				out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
				currentFileBegin += file.getLength();
				out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
			}
			for (auto dir : structure.getDirectories()) {
				out << (char)0;
				out << (std::uint8_t)dir.getName().length();
				out << dir.getName().substr(0, 255);
				out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
				currentFileBegin += (dir.getLength() + dir.getHeaderLength());
			}
			for (auto file : structure.getFiles()) {
				file.writeToOutput(out);
			}
			for (auto dir : structure.getDirectories()) {
				dir.writeToOutput(out);
			}
		}
		return RPK_OK;
	}
	int package::extractFile(const std::string& archPath, const std::string& filePath, const std::string& targetPath)
	{
		if (!std::filesystem::exists(archPath)) {
			RPK_ERROR("Input doesnt exist");
			return RPK_ARCHIVE_DOESNT_EXIST;
		} 
		else if (std::filesystem::exists(targetPath)) {
			RPK_ERROR("Target already exists");
			return RPK_OUTPUT_EXISTS;
		}
		else if (std::filesystem::is_directory(archPath)) {
			RPK_ERROR("Input cant be a directory");
			return RPK_INPUT_IS_DIRECTORY;
		}
		else {
			std::ifstream in(archPath, std::ios::binary);
			if (in) {
				std::string buffer(RPK_MAGIC_NUMBER_LENGTH, 0x00);
				in.read(&buffer[0], buffer.length());
				char cbuffer;
				if (buffer != RPK_MAGIC_NUMBER) {
					RPK_ERROR("Input is no Raven Package");
					return RPK_INPUT_ISNT_RAVEN_PACKAGE;
				}
				in.get(cbuffer);
				if (std::find(supportedExtractVersions.begin(), supportedExtractVersions.end(), (std::uint8_t)cbuffer)
					== supportedExtractVersions.end()) {
					RPK_ERROR("Unsupported file version");
					return RPK_UNSUPPORTED_VERSION;
				}
				switch ((std::uint8_t)cbuffer) {
				case RPK_VERSION_1: {
					std::string filePathCopy = filePath;
					std::size_t position = 0;
					while (filePathCopy.find('\\') != filePathCopy.npos) {
						position = filePathCopy.find('\\');
						filePathCopy = filePathCopy.replace(position, 1, "/");
					}
					std::vector<std::string> fileNames = util::split(filePathCopy, "/");
					for (auto file : fileNames) {
						if (file.empty()) { RPK_ERROR("Invalid path"); return RPK_INVALID_PATH; }
					}
					std::uint64_t begin = in.tellg();
					for (auto file : fileNames) {
						in.seekg(begin);
						buffer = std::string(2, 0x00);
						in.read(&buffer[0], buffer.length());
						std::uint16_t fileCount = util::convertCharsToUint16(buffer.c_str());
						for (std::uint16_t i = 0; i < fileCount; i++) {
							in.get(cbuffer);
							bool isFile = cbuffer & RPK_TRAIT_IS_FILE;
							in.get(cbuffer);
							buffer = std::string((std::uint8_t)cbuffer, 0x00);
							in.read(&buffer[0], buffer.length());
							if (file != buffer) {
								if (isFile) {
									in.seekg(in.tellg() + std::streamoff(16));
								}
								else {
									in.seekg(in.tellg() + std::streamoff(8));
								}
							}
							if ((file == buffer) && isFile) {
								buffer = std::string(8, 0x00);
								in.read(&buffer[0], buffer.length());
								std::uint64_t beg = util::convertCharsToUint64(buffer.c_str());
								in.read(&buffer[0], buffer.length());
								std::uint64_t end = util::convertCharsToUint64(buffer.c_str());
								std::ofstream out(targetPath, std::ios::binary);
								std::uint64_t length = (end - beg);
								std::uint64_t counter = 0;
								in.seekg(beg);
								if (out) {
									std::string buf(BUF_SIZE, 0x00);
									for (int i = 0; i < (length / BUF_SIZE) + 1; i++) {
										buf = std::string(length - counter > BUF_SIZE ? BUF_SIZE : length - counter, 0x00);
										in.read(&buf[0], buf.length());
										out << buf;
										counter += buf.length();
									}
								}
								else {
									RPK_ERROR("Couldn't open output file");
									return RPK_COULDNT_OPEN_FILE;
								}
							}
							if ((file == buffer) && !isFile) {
								buffer = std::string(8, 0x00);
								in.read(&buffer[0], buffer.length());
								begin = util::convertCharsToUint64(buffer.c_str());
							}
						}
					}
					break;
				}
				}
			}
			else {
				RPK_ERROR("Couln't open input");
				return RPK_COULDNT_OPEN_FILE;
			}
		}
		return RPK_OK;
	}
	int package::extractFile(const std::string& archPath, const std::string& filePath)
	{
		return extractFile(archPath, filePath, std::filesystem::path(filePath).filename().string());
	}
	package::bytes<2> package::util::convertUint16ToChars(std::uint16_t uint16)
	{
		package::bytes<2> bytes;
		bytes.chars[0] = uint16 & 0xFF;
		bytes.chars[1] = (uint16 >> 8) & 0xFF;
		return bytes;
	}
	package::bytes<8> package::util::convertUint64ToChars(std::uint64_t uint64)
	{
		package::bytes<8> bytes;
		bytes.chars[0] = uint64 & 0xFF;
		bytes.chars[1] = (uint64 >> 8) & 0xFF;
		bytes.chars[2] = (uint64 >> 16) & 0xFF;
		bytes.chars[3] = (uint64 >> 24) & 0xFF;
		bytes.chars[4] = (uint64 >> 32) & 0xFF;
		bytes.chars[5] = (uint64 >> 40) & 0xFF;
		bytes.chars[6] = (uint64 >> 48) & 0xFF;
		bytes.chars[7] = (uint64 >> 56) & 0xFF;
		return bytes;
	}
	std::uint16_t package::util::convertCharsToUint16(bytes<2> bytes)
	{
		return std::uint16_t((std::uint16_t((std::uint8_t)bytes.chars[0]) | (std::uint16_t((std::uint8_t)bytes.chars[1]) << 8)));
	}
	std::uint64_t package::util::convertCharsToUint64(bytes<8> bytes)
	{
		return std::uint64_t(
			(std::uint64_t((std::uint8_t)bytes.chars[0]) |
			(std::uint64_t((std::uint8_t)bytes.chars[1]) << 8)  |
			(std::uint64_t((std::uint8_t)bytes.chars[2]) << 16) |
			(std::uint64_t((std::uint8_t)bytes.chars[3]) << 24) | 
			(std::uint64_t((std::uint8_t)bytes.chars[4]) << 32) | 
			(std::uint64_t((std::uint8_t)bytes.chars[5]) << 40) | 
			(std::uint64_t((std::uint8_t)bytes.chars[6]) << 48) | 
			(std::uint64_t((std::uint8_t)bytes.chars[7]) << 56)));
	}
	std::vector<std::string> package::util::split(const std::string& str, const std::string& delim)
	{
		std::vector<std::string> out;
		size_t start;
		size_t end = 0;

		while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
		{
			end = str.find(delim, start);
			out.push_back(str.substr(start, end - start));
		}
		return out;
	}
}
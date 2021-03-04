#include "RavenPackage.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace rvn {
	/* Structure definition */
	using fpath = std::filesystem::path;
	struct package::Structure {
		struct FileEntry {
			FileEntry(const File& file, const std::string& name) : file(file) 
			{
				this->name = name;
			}
			int writeToOutput(std::ofstream& out) {
				file.open();
				auto& in = file.getIStream();
				if (in) {
					std::string buf(BUF_SIZE, 0x00);
					for (std::uint64_t i = 0; i < (file.getLength() / BUF_SIZE) + 1; i++) {
						buf = std::string(file.getLength() - in->tellg() > BUF_SIZE ? BUF_SIZE : file.getLength() - in->tellg(), 0x00);
						in->read(&buf[0], buf.length());
						out << buf;
					}
				}
				else {
					RPK_ERROR("Couldn't open file '" + file.getName() + "'");
					return RPK_COULDNT_OPEN_FILE;
				}
				return RPK_OK;
			}
			File file;
			std::string name;
		};
		struct DirectoryEntry {
			DirectoryEntry(const std::string& name) {
				this->name = name;
				headerlength = 0;
				length = 0;
			}
			void calculateLength() {
				headerlength = RPK_V1_FILE_COUNT_LENGTH;
				length = 0;
				for (auto& dir : directories) {
					dir.calculateLength();
				}
				for (auto& file : files) {
					length += file.file.getLength();
					headerlength += RPK_V1_FILE_HEADER_LENGTH;
					headerlength += file.name.length();
				}
				for (auto& dir : directories) {
					length += dir.getLength() + dir.getHeaderLength();
					headerlength += RPK_V1_DIR_HEADER_LENGTH;
					headerlength += dir.name.length();
				}
			}
			int writeToOutput(std::ofstream& out) {
				if ((directories.size() + files.size()) > UINT16_MAX) return RPK_TOO_MANY_FILES;
				else {
					std::uint64_t dirStart = out.tellp();
					std::uint64_t currentBegin = dirStart + headerlength;
					out.write(util::convertUint16ToChars((std::uint16_t)files.size() + (std::uint16_t)directories.size()).chars, 2);
					for (auto& file : files) {
						out << (char)RPK_TRAIT_IS_FILE;
						out << (std::uint8_t)file.file.getName().length();
						out << file.file.getName().substr(0, 255);
						out.write(util::convertUint64ToChars(currentBegin).chars, 8);
						currentBegin += file.file.getLength();
						out.write(util::convertUint64ToChars(currentBegin).chars, 8);
					}
					for (auto& dir : directories) {
						out << (char)0;
						out << (uint8_t)dir.name.length();
						out << dir.name.substr(0, 255);
						out.write(util::convertUint64ToChars(currentBegin).chars, 8);
						currentBegin += dir.length + dir.headerlength;
					}
					for (auto& file : files) {
						file.writeToOutput(out);
					}
					for (auto& dir : directories) {
						dir.writeToOutput(out);
					}
				}
				return RPK_OK;
			}
			std::uint64_t getLength() const { return length; }
			std::uint64_t getHeaderLength() const { return headerlength; }
			std::vector<FileEntry> files;
			std::vector<DirectoryEntry> directories;
			std::string name;
			std::uint64_t headerlength, length;
		};
		DirectoryEntry base = DirectoryEntry("");
		Structure(PackageCreator& creator)
		{
			for (auto& entry : creator.entry) {
				DirectoryEntry* current = &base;
				std::vector<std::string> fileNames = util::convertPath(entry.path);
				for (const auto& name : fileNames) {
					if (fileNames.back() == name) {
						if (entry.file.has_value())
							current->files.push_back({ entry.file.value(), name });
						else
							current->directories.push_back(name);
					}
					else {
						bool found = false;
						for (auto& entryEntry : current->directories) {
							if (entryEntry.name == name) {
								current = &entryEntry;
								found = true;
							}
						}
						if (!found) {
							current->directories.push_back({ name });
							current = &(current->directories.back());
						}
					}
				}
			}
		}
	};
	/* Structure definition end */
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
			PackageCreator creator;
			for (auto& file : std::filesystem::recursive_directory_iterator(dirPath)) {
				if (file.is_directory()) {
					creator.addDirectory(std::filesystem::relative(file.path(), dirPath).string());
				}
				if (file.is_regular_file()) {
					creator.addFile(std::filesystem::relative(file.path().string(), dirPath).string(), File(file.path().filename().string(), file.path().string()));
				}
			}
			Structure structure(creator);
			structure.base.calculateLength();
			return createArchiveFromStructure(structure, archivePath);
		}
		return RPK_OK;
	}
	int package::createArchive(const PackageCreator& package, const std::string& archivePath, bool overrideOldTarget)
	{
		if (std::filesystem::exists(archivePath) && !overrideOldTarget) {
			/* There is already a file with the path of the output */
			RPK_ERROR("Output target already exists. This error can be disabled by setting overrideOldTarget to true");
			return RPK_OUTPUT_EXISTS;
		}
		PackageCreator pk = package;
		Structure structure(pk);
		structure.base.calculateLength();
		return createArchiveFromStructure(structure, archivePath);
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
					for (auto& file : fileNames) {
						if (file.empty()) { RPK_ERROR("Invalid path"); return RPK_INVALID_PATH; }
					}
					std::uint64_t begin = in.tellg();
					for (auto& file : fileNames) {
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
	Entries package::getEntriesAt(const std::string& archPath, const std::string& filePath)
	{
		Entries ret;
		std::vector<Entry>& entries = ret.entries;
		int& status = ret.status;
		if (!std::filesystem::exists(archPath)) {
			RPK_ERROR("Input doesnt exist");
			status = RPK_ARCHIVE_DOESNT_EXIST;
		}
		else if (std::filesystem::is_directory(archPath)) {
			RPK_ERROR("Input cant be a directory");
			status = RPK_INPUT_IS_DIRECTORY;
		}
		else {
			std::ifstream in(archPath, std::ios::binary);
			if (in) {
				std::string buffer(RPK_MAGIC_NUMBER_LENGTH, 0x00);
				in.read(&buffer[0], buffer.length());
				char cbuffer;
				if (buffer != RPK_MAGIC_NUMBER) {
					RPK_ERROR("Input is no Raven Package");
					status = RPK_INPUT_ISNT_RAVEN_PACKAGE; return ret;
				}
				in.get(cbuffer);
				if (std::find(supportedExtractVersions.begin(), supportedExtractVersions.end(), (std::uint8_t)cbuffer)
					== supportedExtractVersions.end()) {
					RPK_ERROR("Unsupported file version");
					status = RPK_UNSUPPORTED_VERSION; return ret;
				}
				switch ((std::uint8_t)cbuffer) {
				case RPK_VERSION_1: {
					std::string filePathCopy = filePath;
					std::size_t position = 0;
					//bool exists = false;
					while (filePathCopy.find('\\') != filePathCopy.npos) {
						position = filePathCopy.find('\\');
						filePathCopy = filePathCopy.replace(position, 1, "/");
					}
					std::vector<std::string> fileNames = util::split(filePathCopy, "/");
					for (auto& file : fileNames) {
						if (file.empty()) { RPK_ERROR("Invalid path"); status = RPK_INVALID_PATH; return ret; }
					}
					std::uint64_t begin = in.tellg();
					for (auto& file : fileNames) {
						//exists = false;
						buffer.resize(2, 0);
						in.read(&buffer[0], 2);
						std::uint16_t fileCount = util::convertCharsToUint16(buffer.c_str());
						for (std::uint16_t i = 0; i < fileCount; i++) {
							in.get(cbuffer);
							bool isFile = cbuffer & RPK_TRAIT_IS_FILE;
							// name length
							in.get(cbuffer);
							buffer.resize((std::size_t)(std::uint8_t)cbuffer);
							in.read(&buffer[0], buffer.length());
							std::string name = buffer;
							if (name == file && !isFile) {
								//exists = true;
								buffer.resize(8);
								in.read(&buffer[0], 8);
								begin = util::convertCharsToUint64(buffer.c_str());
							}
							else {
								if (isFile) {
									in.seekg(in.tellg() + (std::streampos)16);
								}
								else {
									in.seekg(in.tellg() + (std::streampos)8);
								}
							}
						}
					}
					// analyze the directory
					in.seekg(begin);
					buffer.resize(2, 0);
					in.read(&buffer[0], 2);
					std::uint16_t fileCount = util::convertCharsToUint16(buffer.c_str());
					for (std::uint16_t i = 0; i < fileCount; i++) {
						Entry entry;
						in.get(cbuffer);
						bool isFile = cbuffer & RPK_TRAIT_IS_FILE;
						entry.isFile = isFile;
						// name length
						in.get(cbuffer);
						buffer.resize((std::size_t)(std::uint8_t)cbuffer);
						in.read(&buffer[0], buffer.length());
						std::string name = buffer;
						entry.name = name;
						if (isFile) {
							// size
							buffer.resize(8);
							in.read(&buffer[0], 8);
							std::uint64_t begin = util::convertCharsToUint64(buffer.c_str());
							in.read(&buffer[0], 8);
							entry.length = util::convertCharsToUint64(buffer.c_str()) - begin;
							entry.formattedLength = util::formatBytes(entry.length);
						}
						else {
							in.seekg(in.tellg() + (std::streampos)8);
						}
						ret.entries.push_back(entry);
					}
				}
				}
			}
			else {
				RPK_ERROR("Couln't open input");
				status = RPK_COULDNT_OPEN_FILE;
			}
		}
		return ret;
	}
	int package::createArchiveFromStructure(Structure& structure, const std::string& archivePath)
	{
		if ((structure.base.directories.size() + structure.base.files.size()) > UINT16_MAX) {
			RPK_ERROR("Base directory contains too many files and directories (over 65535)");
			return RPK_TOO_MANY_FILES;
		}
		if ((structure.base.directories.size() + structure.base.files.size()) == 0) {
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
		out.write(util::convertUint16ToChars((std::uint16_t)structure.base.files.size() 
			+ (std::uint16_t)structure.base.directories.size()).chars, 2);

		/* Header length calculation */
		std::uint64_t headerLength = RPK_MAGIC_NUMBER_LENGTH + RPK_VERSION_LENGTH + RPK_V1_FILE_COUNT_LENGTH;
		for (auto& dir : structure.base.directories) {
			headerLength += RPK_V1_DIR_HEADER_LENGTH;
			headerLength += dir.name.length();
		}
		for (auto& file : structure.base.files) {
			headerLength += RPK_V1_FILE_HEADER_LENGTH;
			headerLength += file.name.length();
		}
		/* Write file headers */
		std::uint64_t currentFileBegin = headerLength;
		for (auto& file : structure.base.files) {
			out << (char)RPK_TRAIT_IS_FILE;
			out << (std::uint8_t)file.name.length();
			out << file.name.substr(0, 255);
			out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
			currentFileBegin += file.file.getLength();
			out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
		}
		for (auto& dir : structure.base.directories) {
			out << (char)0;
			out << (std::uint8_t)dir.name.length();
			out << dir.name.substr(0, 255);
			out.write(util::convertUint64ToChars(currentFileBegin).chars, 8);
			currentFileBegin += (dir.getLength() + dir.getHeaderLength());
		}
		for (auto& file : structure.base.files) {
			file.writeToOutput(out);
		}
		for (auto& dir : structure.base.directories) {
			dir.writeToOutput(out);
		}
		return RPK_OK;
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
	std::string package::util::formatBytes(std::uint64_t bytes)
	{
		std::string pre = "";
		std::uint64_t dividend = 1;
		if (bytes > 1099511627776) {
			pre = 'T';
			dividend = 1099511627776;
		}
		else if (bytes > 1073741824) {
			pre = 'G';
			dividend = 1073741824;
		}
		else if (bytes > 1048576) {
			pre = 'M';
			dividend = 1048576;
		}
		else if (bytes > 1024) {
			pre = 'k';
			dividend = 1024;
		}
		std::stringstream ret;
		ret << std::setprecision(5) << (static_cast<double>(static_cast<std::uint64_t>((double)bytes / (double)dividend) * 10.) / 10.);
		ret << pre << "B";
		return ret.str();
	}
	std::vector<std::string> package::util::convertPath(const std::string& path)
	{
		std::size_t position = 0;
		std::string filePathCopy = path;
		while (filePathCopy.find('\\') != filePathCopy.npos) {
			position = filePathCopy.find('\\');
			filePathCopy = filePathCopy.replace(position, 1, "/");
		}
		return util::split(filePathCopy, "/");
	}
	void package::PackageCreator::addFile(const std::string& path, std::shared_ptr<std::string> source)
	{
		entry.push_back({ path, File(std::filesystem::path(path).filename().string(), source) });
	}
	void package::PackageCreator::addFile(const std::string& path, const File& file)
	{
		entry.push_back({ path, file });
	}
	void package::PackageCreator::addFile(const std::string& path, const std::string& harddrivePath)
	{
		entry.push_back({ path, File(std::filesystem::path(path).filename().string(), harddrivePath) });
	}
	void package::PackageCreator::addDirectory(const std::string& path)
	{
		entry.push_back({ path });
	}
}
#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <fstream>
#include <filesystem>

// Logging defines for possibility of custom logging
#ifndef RPK_NO_LOG
	#ifndef RPK_NO_TRACE
		#ifndef RPK_TRACE
			#define RPK_TRACE(str) std::cout << "RavenPackage-Trace: " << str << std::endl
		#endif
	#endif
	#ifndef RPK_ERROR
		#define RPK_ERROR(str) std::cerr << "RavenPackage-Error: " << str << std::endl
	#endif
#endif

#define BIT(x) (1 << x)

#define BUF_SIZE 8192

// Return defines
#define RPK_OK 0
#define RPK_INPUT_DOESNT_EXIST 1
#define RPK_INPUT_NOT_DIRECTORY 2
#define RPK_OUTPUT_EXISTS 3
#define RPK_COULDNT_OPEN_FILE 4
#define RPK_TOO_MANY_FILES 5
#define RPK_DIR_IS_EMPTY 6
#define RPK_ARCHIVE_DOESNT_EXIST 7
#define RPK_INPUT_IS_DIRECTORY 8
#define RPK_INPUT_ISNT_RAVEN_PACKAGE 9
#define RPK_UNSUPPORTED_VERSION 10
#define RPK_INVALID_PATH 11

// Traits
#define RPK_TRAIT_IS_FILE BIT(0)

// Magic Number
const std::string RPK_MAGIC_NUMBER = { 'R', 'a', 'v', 'e', 'n', 'G', 'a', 'm', 'e', 'F', 'i', 'l', 'e', 0x00 };

#define RPK_MAGIC_NUMBER_LENGTH []() -> std::uint64_t { return RPK_MAGIC_NUMBER.length(); }()
#define RPK_VERSION_LENGTH 1

// Version 1
// Indices have 8 bytes
// Files have a start and an end defined, dirs only a start
#define RPK_VERSION_1 1
#define RPK_V1_FILE_COUNT_LENGTH 2
#define RPK_V1_FILE_HEADER_LENGTH 18
#define RPK_V1_DIR_HEADER_LENGTH 10

// Supported versions
const std::vector<std::uint8_t> supportedExtractVersions = { RPK_VERSION_1 };

// Size of buffer used to reading in files, can be overidden
#ifndef RPK_BUFFER_SIZE
#define RPK_BUFFER_SIZE 8192
#endif

namespace rvn {
	// An entry in a directory
	struct Entry {
		std::string name = "";
		bool isFile = true;
		std::size_t length = 0;
		std::string formattedLength = "";
		bool hasSubFiles = false;
	};
	struct Entries {
		std::vector<Entry> entries;
		int status = RPK_OK;
	};
	struct package {
		struct PackageCreator;
	public:
		// Creates a Raven Package from a directory on the harddrive
		static int createArchiveFromDir(const std::string& dirPath, const std::string& archivePath, bool overrideOldTarget = false);
		// Creates a Raven Package from a PackageCreator struct, so it can be used to create a package from memory
		static int createArchive(const PackageCreator& package, const std::string& archivePath, bool overrideOldTarget = false);
		// Extracts a file from an archive to a certain location
		static int extractFile(const std::string& archPath, const std::string& filePath, const std::string& targetPath);
		// Extract a file from an archive and gives the file the name it had in the archive
		static int extractFile(const std::string& archPath, const std::string& filePath);
		// Extracts all directories and files in an directory in an archive (whether a directory has sub files doesn't work yet)
		static Entries getEntriesAt(const std::string& archPath, const std::string& filePath);
	private:
		struct File {
			File(const std::string& name, const std::string& path) {
				_name = name;
				_path = path;
			}
			File(const std::string& name, const std::shared_ptr<std::string>& source) {
				_name = name;
				_source = source;
			}
			void open() {
				if (_path.has_value()) {
					_istream.reset(new std::ifstream(_path.value()));
				}
				else {
					_istream.reset(new std::istringstream(*_source.value().get()));
				}
			}
			const std::shared_ptr<std::istream>& getIStream() const { return _istream; }
			std::uint64_t getLength() const {
				if (_path.has_value()) {
					return std::filesystem::file_size(_path.value());
				}
				else {
					return _source.value()->length();
				}
			}
			const std::string& getName() const { return _name; }
		protected:
			std::string _name;
			std::optional<std::string> _path;
			std::optional<std::shared_ptr<std::string>> _source;
		private:
			std::shared_ptr<std::istream> _istream;
		};
		struct Structure;
		static int createArchiveFromStructure(Structure& structure, const std::string& archivePath);
	public:
		// Struct to create packages from memory
		struct PackageCreator {
			friend struct Structure;
			friend struct package;
		public:
			PackageCreator() = default;
			void addFile(const std::string& path, std::shared_ptr<std::string> source);
			void addFile(const std::string& path, const std::string& harddrivePath);
			void addDirectory(const std::string& path);
		protected:
			void addFile(const std::string& path, const File& file);
			struct Entry {
				Entry() = default;
				Entry(const std::string& path, const File& file)
					: path(path), file(file)
				{}
				Entry(const std::string& path)
					: path(path)
				{}
				std::string path;
				std::optional<File> file;
			};
			std::vector<Entry> entry;
			std::string path;
		};
	private:
		template<std::uint8_t size>
		struct bytes {
			bytes() = default;
			bytes(const char* str) { for (std::uint8_t i = 0; i < size; i++) { chars[i] = str[i]; } }
			char chars[size];
		};
		struct util {
			static bytes<2> convertUint16ToChars(std::uint16_t uint16);
			static bytes<8> convertUint64ToChars(std::uint64_t uint64);
			static std::uint16_t convertCharsToUint16(bytes<2> bytes);
			static std::uint64_t convertCharsToUint64(bytes<8> bytes);
			static std::vector<std::string> split(const std::string& str, const std::string& delim);
			static std::string formatBytes(std::uint64_t bytes);
			static std::vector<std::string> convertPath(const std::string& path);
		};
	};
}
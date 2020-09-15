#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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
	struct package { 
		static int createArchiveFromDir(const std::string& dirPath, const std::string& archivePath, bool overrideOldTarget = false);
		static int extractFile(const std::string& archPath, const std::string& filePath, const std::string& targetPath);
		static int extractFile(const std::string& archPath, const std::string& filePath);
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
		};
	};
}
#pragma once

#include <cstdint>
#include <iostream>
#include <string>

// Logging defines for possibility of custom logging
#ifndef RPK_NO_LOG
	#ifndef RPK_NO_TRACE
		#ifndef RPK_TRACE
			#define RPK_TRACE(str) std::cout << "RavenPackage-Trace: " << str << std::endl;
		#endif
	#endif
	#ifndef RPK_ERROR
		#define RPK_ERROR(str) std::cerr << "RavenPackage-Error: " << str << std::endl;
	#endif
#endif

#define BIT(x) (1 << x)

// Magic Number
const std::string RPK_MAGIC_NUMBER = { 'R', 'a', 'v', 'e', 'n', 'G', 'a', 'm', 'e', 'F', 'i', 'l', 'e', 0x00 };

#define RPK_MAGIC_NUMBER_LENGTH []() -> uint64_t { return RPK_MAGIC_NUMBER.length(); }()
#define RPK_VERSION_LENGTH 1

// Version 1
// Indices have 8 bytes
// Files have a start and an end defined, dirs only a start
#define RPK_VERSION_1 0
#define RPK_V1_FILE_COUNT_SIZE 2
#define RPK_V1_FILE_HEADER_LENGTH 18
#define RPK_V1_DIR_HEADER_LENGTH 10

// Size of buffer used to reading in files, can be overidden
#ifndef RPK_BUFFER_SIZE
#define RPK_BUFFER_SIZE 8192
#endif
namespace rvn {
	namespace package {
		int createArchiveFromDir(const std::string& dirPath, const std::string& archivePath);
	}
}
#pragma once

#include <cstdint>
#include <iostream>
#include <string>

// Logging defines for possibility of custom logging
#ifndef RPK_TRACE
#define RPK_TRACE(str) std::cout << "RavenPackage-Trace: " << str << std::endl;
#endif
#ifndef RPK_ERROR
#define RPK_ERROR(str) std::cerr << "RavenPackage-Error: " << str << std::endl;
#endif

namespace rvn {
	namespace package {


	}
}
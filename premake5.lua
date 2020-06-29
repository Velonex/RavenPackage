workspace "RavenPackage"
	architecture "x86_64"
	startproject "RavenPackageExecutable"

	configurations {
		"Debug",
		"Release",
		"Dist"
	}
	
	flags
	{
		"MultiProcessorCompile"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}

include "RavenPackage"

project "RavenPackageExecutable"
	location "RavenPackageExecutable"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	files
	{
		"%{prj.name}/**.h",
		"%{prj.name}/**.cpp"
	}
	includedirs
	{
		"%{prj.name}/src",
		"RavenPackage/src"
	}
	links
	{
		"RavenPackage"
	}
	filter "system:windows"
		systemversion "latest"
	filter "configurations:Debug"
		defines "DEBUG"
		symbols "on"
		runtime "Debug"

	filter "configurations:Release"
		defines "RELEASE"
		optimize "on"
		runtime "Release"

	filter "configurations:Dist"
		defines "DIST"
		optimize "on"
		runtime "Release"
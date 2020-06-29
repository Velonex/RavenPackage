project "RavenPackage"
	location "."
	kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"
	
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	includedirs
	{
		"%{prj.name}/src"
	}
	links
	{
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
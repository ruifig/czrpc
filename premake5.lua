language "C++"

targetdir "build/bin"
objdir "build/tmp/%{prj.name}/%{cfg.platform}_%{cfg.buildcfg}"
targetname "%{prj.name}_%{cfg.platform}_%{cfg.buildcfg}"

location "build"
workspace "czrpc"
	platforms { "x64" }
	configurations { "Debug", "Release" }
	-- flags { "ExtraWarnings" }
	flags { "FloatFast", "EnableSSE2" }
	configuration "Debug"
		symbols "On"
		defines "DEBUG"
	configuration "Release"
		optimize "Speed"
		defines { "NDEBUG" }

---------------------------------------------------
--	UnitTest++
---------------------------------------------------
project "UnitTest++"
	kind "StaticLib"
	location "build"
	targetdir "build/lib"

	files { "UnitTest++/*.h", "UnitTest++/*.cpp" }
	filter "system:Windows"
		files { "UnitTest++/Win32/*" }
	filter "system:Linux"
		files { "UnitTest++/Posix/*" }

---------------------------------------------------
--	czrpc
---------------------------------------------------
project "czrpc"
	kind "None" -- None is used for projects that only have header files
	location "build"
	language "C++"
	files { "source/crazygaze/rpc/*.h" }

---------------------------------------------------
--	tests
---------------------------------------------------
project "tests"
	kind "ConsoleApp"
	location "build"

	links {"UnitTest++"}
	includedirs { ".", "./source", "asio/asio/include"}

	files {"./tests/*.h", "./tests/*.cpp"} 

	pchheader "testsPCH.h" -- This is treated as a string (not as a path)
	pchsource "./tests/testsPCH.cpp" -- This is treated as a path, so needs to be the path to the file

	filter "action:vs*"
		buildoptions "/bigobj"

newaction
{
	trigger = "get_asio",
	description = "Downloads standalone asio (Required for the samples)",
	execute = function()
		if os.isdir("./asio") then
			if os.execute("git --git-dir=./asio/.git checkout master") ~=0 then
				error("Error updating asio git repository")
			end
			if os.execute("git --git-dir=./asio/.git pull --update") ~=0 then
				error("Error updating asio git repository")
			end
		else
			if os.execute("git clone https://github.com/chriskohlhoff/asio.git asio") ~= 0 then
				error("Error getting asio")
			end
		end

		if os.execute("git --git-dir=./asio/.git checkout tags/asio-1-10-6") ~= 0 then
			error("Error getting asio")
		end
	end
}

newaction
{
	trigger = "clean",
	description = "Delete all build and temporary files",
	execute = function()
		files_to_delete =
		{
		}
		directories_to_delete =
		{
			"build"
		}
		for i,v in ipairs( directories_to_delete ) do
			os.rmdir( v )
		end		
		for i,v in ipairs( files_to_delete ) do
			os.execute( "{DELETE} " .. v)
		end
	end
}

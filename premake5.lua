--[[
    MIT License

    Copyright (c) 2016-2022 Raúl Ramos

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
--]]

-- check out https://github.com/premake/premake-core/wiki/Tokens

workspace "tutorial-callstack-tracker"
	language "C++"
	cppdialect "C++17"

    configurations { "Debug", "Release" }
    platforms      { "x64" }

    location ".build"
    flags  { "FatalCompileWarnings", "FatalLinkWarnings" }

    filter { "configurations:Debug"          } defines { "DEBUG"  } symbols  "On" 
    filter { "configurations:Release"        } defines { "NDEBUG" } optimize "Speed" 
    filter { "platforms:*64"                 } architecture "x86_64"
    filter { "platforms:*86"                 } architecture "x86"
    filter { "system:macosx", "action:gmake" } toolset "clang"
    filter { "system:windows", "action:vs*"  } buildoptions { "/W3", "/EHsc" }
    filter { "system:linux"                  } links "pthread"
    filter { "toolset:clang or toolset:gcc"  } buildoptions { "-Wall", "-Wextra", "-fno-exceptions", "-msse4.2" }

    startproject "host"

-- Library that will receive the call stacks

project "qcstudio"
    kind "SharedLib"
    targetdir ".out/%{cfg.platform}/%{cfg.buildcfg}"
    objdir ".tmp/%{prj.name}"
    defines { "BUILDING_QCSTUDIO" }
    links {
        "dbghelp"
    }

    files { "src/qcstudio/*" }

-- Test shared libraries

project "foo"
    kind "SharedLib"
    includedirs { "src" }
    targetdir ".out/%{cfg.platform}/%{cfg.buildcfg}"
    objdir ".tmp/%{prj.name}"
    dependson { "qcstudio" }

    libdirs { "%{cfg.buildtarget.directory}" }
    links { 
        "qcstudio.lib"
    }

    files { "src/foo/*" }

project "bar"
    kind "SharedLib"
    includedirs { "src" }
    targetdir ".out/%{cfg.platform}/%{cfg.buildcfg}"
    objdir ".tmp/%{prj.name}"
    dependson { "qcstudio" }

    libdirs { "%{cfg.buildtarget.directory}" }
    links { 
        "qcstudio.lib" 
    }

    files { "src/bar/*" }

-- Host application that produces the data

project "host"
    kind "ConsoleApp"
    dependson { "foo", "bar", "baz", "qcstudio" }
    includedirs { "src" }

    targetdir ".out/%{cfg.platform}/%{cfg.buildcfg}"
    objdir ".tmp/%{prj.name}"

    libdirs { "%{cfg.buildtarget.directory}" }
    links { 
        "foo.lib", 
        "qcstudio.lib" 
    }

    files { "src/host/*" }

project "viewer"
    kind "ConsoleApp"
    dependson { "qcstudio" }
    includedirs { "src" }

    targetdir ".out/%{cfg.platform}/%{cfg.buildcfg}"
    objdir ".tmp/%{prj.name}"

    libdirs { "%{cfg.buildtarget.directory}" }
    links { 
        "qcstudio.lib",
        "dbghelp"
    }

    files { "src/viewer/*" }

-- Handle Dropbox annoying sync of temporary folders

print("[] Excluding .build, .tmp and .out from Dropbox sync...");

if os.target() == "windows" then

    -- Do not allow Dropbox to sync these temporary folders

    local script = [[
        New-Item .build    -type directory -force | Out-Null
        New-Item .tmp      -type directory -force | Out-Null
        New-Item .out      -type directory -force | Out-Null
        Set-Content -Path '.build' -Stream com.dropbox.ignored -Value 1
        Set-Content -Path '.tmp'   -Stream com.dropbox.ignored -Value 1
        Set-Content -Path '.out'   -Stream com.dropbox.ignored -Value 1
    ]]

    -- Feed the script to powershell process stdin

    local pipe = io.popen("powershell -command -", "w")
    pipe:write(script)
    pipe:close()

elseif os.target() == "macosx" then

    -- Do not allow Dropbox to sync these temporary folders

    local script = [[
        mkdir -p .build
        mkdir -p .tmp
        mkdir -p .out
        xattr -w com.dropbox.ignored 1 .build
        xattr -w com.dropbox.ignored 1 .tmp
        xattr -w com.dropbox.ignored 1 .out
    ]]

    -- Run the script

    local pipe = io.popen("bash", "w")
    pipe:write(script)
    pipe:close()

end


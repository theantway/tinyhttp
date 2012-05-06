solution "network-demo"
  configurations { "debug", "release" }
  
  output_path = "build/"
  if _ACTION ~= nil then 
  	output_path = "build/" .. _ACTION .. "/"
  end
  
  location (output_path)

  configuration { "debug" }
    targetdir (output_path .. "bin/debug")
    flags {"Symbols"}
    buildoptions { "-fno-inline -Wall -O0" }
 
  configuration { "release" }
    targetdir (output_path .. "bin/release")
 
  if _ACTION == "clean" then
    os.rmdir("build")
  end

project "http_ae"
  language "c"
  kind     "ConsoleApp"
  files  { "src/http.c", "src/rio.c", "src/buffered_request.c", "src/thttp_ae.c", "lib/ae/ae.c", "lib/ae/zmalloc.c" }
  includedirs { "include", "lib/ae" }
  
  configuration { "Debug*" }
    defines { "_DEBUG", "DEBUG" }
 
  configuration { "Release*" }
    defines { "NDEBUG" }

project "http_epoll"
  language "c"
  kind     "ConsoleApp"
  files  { "src/http.c", "src/rio.c", "src/buffered_request.c", "src/thttp_epoll.c" }
  includedirs { "include", "lib/ae" }
  
  configuration { "Debug*" }
    defines { "_DEBUG", "DEBUG" }
 
  configuration { "Release*" }
    defines { "NDEBUG" }

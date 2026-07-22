# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-src"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-build"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/tmp"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/src"
  "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/quackdcs/micro-opcua/b_100/_deps/unity-subbuild/unity-populate-prefix/src/unity-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

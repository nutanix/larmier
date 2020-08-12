#
# Copyright (c) 2019 Nutanix Inc. All rights reserved.
#
# Author: Felipe Franciosi <felipe@nutanix.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 2 only.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

project(larmier C)

include (CTest)

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wmissing-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedefs")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_C_FLAGS_RELEASE "-O2")
set(CMAKE_C_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_EXE_LINKER_FLAGS "-pie" )

include_directories(${CMAKE_SOURCE_DIR})

add_executable(larmier larmier.c)
target_link_libraries(larmier rt)
set_target_properties(larmier PROPERTIES PUBLIC_HEADER
                      "larmier.h;larmier_stub.h")

install(TARGETS larmier
        RUNTIME       DESTINATION /usr/local/bin
        PUBLIC_HEADER DESTINATION /usr/local/include
        RESOURCE      DESTINATION /var/lib/larmier)

add_subdirectory(samples)

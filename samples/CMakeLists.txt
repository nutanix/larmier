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

configure_file(${PROJECT_SOURCE_DIR}/dlsym.supp
               ${CMAKE_CURRENT_BINARY_DIR}/dlsym.supp COPYONLY)

function(add_larm_lib lib)
  add_library(${lib} SHARED ${ARGN})
  target_link_libraries(${lib} dl rt unwind)
  set_target_properties(${lib} PROPERTIES NO_SONAME TRUE)
  set_target_properties(${lib} PROPERTIES COMPILE_FLAGS "-O0")
endfunction(add_larm_lib)

function(add_larm_test test stub)
  add_executable(${test} ${ARGN})
  set_target_properties(${test} PROPERTIES COMPILE_FLAGS "-O0")
  add_test(NAME ${test} COMMAND larmier -ddd -l ${stub} ./${test})
endfunction(add_larm_test)

add_larm_lib(test1_stub test1_stub.c)
add_larm_test(test1 libtest1_stub.so test1.c)

add_larm_lib(test2_stub test2_stub.c)
add_larm_test(test2 libtest2_stub.so test2.c)
add_executable(test2_leak test2_leak.c)

add_larm_lib(test3_stub test3_stub.c)
add_larm_test(test3 libtest3_stub.so test3.c)

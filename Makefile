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

BUILD_TYPE ?= release
BUILD_ARCH := $(shell arch)

ifeq ($(BUILD_TYPE), debug)
	CMAKE_BUILD_TYPE ?= Debug
else
	CMAKE_BUILD_TYPE ?= Release
	CFLAGS += -DNDEBUG
endif

GCC_PREFIX ?= /usr
ifneq ($(BUILD_ARCH),x86_64)
	$(error "Unknown ARCH $(ARCH)")
endif

ifeq ($(CC),cc)
	export CC := $(GCC_PREFIX)/bin/gcc
endif

BUILD_DIR_BASE := $(CURDIR)/build
BUILD_DIR := $(BUILD_DIR_BASE)/$(BUILD_TYPE)

ifeq ($(VERBOSE),)
	MAKEFLAGS += -s
endif

PHONY_TARGETS := all realclean buildclean force_cmake export install-export

.PHONY: $(PHONY_TARGETS)

all $(filter-out $(PHONY_TARGETS), $(MAKECMDGOALS)): $(BUILD_DIR)/Makefile
	+$(MAKE) -C $(BUILD_DIR) $@

realclean:
	rm -rf $(BUILD_DIR_BASE)

buildclean:
	rm -rf $(BUILD_DIR)

force_cmake: $(BUILD_DIR)/Makefile

$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR); cmake \
		-D "CMAKE_C_FLAGS:STRING=$(CFLAGS)" \
		-D "CMAKE_BUILD_TYPE:STRING=$(CMAKE_BUILD_TYPE)" \
		$(CURDIR)

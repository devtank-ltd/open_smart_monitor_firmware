#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#
# CFLAGS += -Werror
PROJECT_NAME := smart-factory

include $(IDF_PATH)/make/project.mk

.PHONY: git
git:
	echo \#define GIT_COMMIT \"`git rev-parse --short HEAD`\" > main/commit.h

build: git

#flash: build dfu
dfu:
	./into_dfu.sh

cppcheck:
	cppcheck --enable=all --inconclusive --std=posix . -v

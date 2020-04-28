#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

SF_GIT_COMMIT := $(shell git log -n 1 --format="%h-%f")

entrypoint.o : git_commit.h

sf_git_$(SF_GIT_COMMIT):
	mkdir -p "`dirname "$@"`"
	rm -f build/sf_git_*
	touch $@

git_commit.h : sf_git_$(SF_GIT_COMMIT)
	echo "static const char * build_git_commit = \"GIT : $(SF_GIT_COMMIT)\";" > $@

CFLAGS += -I.

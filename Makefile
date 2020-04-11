# Copyright Joyent, Inc. and other Node contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

SRCDIR ?= $(CURDIR)

<<<<<<< HEAD
ifeq (,$(builddir_name))

VPATH := $(SRCDIR)
include $(SRCDIR)/build.mk
=======
ifdef MSVC
uname_S := MINGW
endif

CPPFLAGS += -Iinclude -Iinclude/uv-private -Isrc/UDT4/src -Isrc/nacl -DEVPIPE_OSFD

CARES_OBJS =
CARES_OBJS += src/ares/ares__close_sockets.o
CARES_OBJS += src/ares/ares__get_hostent.o
CARES_OBJS += src/ares/ares__read_line.o
CARES_OBJS += src/ares/ares__timeval.o
CARES_OBJS += src/ares/ares_cancel.o
CARES_OBJS += src/ares/ares_data.o
CARES_OBJS += src/ares/ares_destroy.o
CARES_OBJS += src/ares/ares_expand_name.o
CARES_OBJS += src/ares/ares_expand_string.o
CARES_OBJS += src/ares/ares_fds.o
CARES_OBJS += src/ares/ares_free_hostent.o
CARES_OBJS += src/ares/ares_free_string.o
CARES_OBJS += src/ares/ares_gethostbyaddr.o
CARES_OBJS += src/ares/ares_gethostbyname.o
CARES_OBJS += src/ares/ares_getnameinfo.o
CARES_OBJS += src/ares/ares_getopt.o
CARES_OBJS += src/ares/ares_getsock.o
CARES_OBJS += src/ares/ares_init.o
CARES_OBJS += src/ares/ares_library_init.o
CARES_OBJS += src/ares/ares_llist.o
CARES_OBJS += src/ares/ares_mkquery.o
CARES_OBJS += src/ares/ares_nowarn.o
CARES_OBJS += src/ares/ares_options.o
CARES_OBJS += src/ares/ares_parse_a_reply.o
CARES_OBJS += src/ares/ares_parse_aaaa_reply.o
CARES_OBJS += src/ares/ares_parse_mx_reply.o
CARES_OBJS += src/ares/ares_parse_ns_reply.o
CARES_OBJS += src/ares/ares_parse_ptr_reply.o
CARES_OBJS += src/ares/ares_parse_srv_reply.o
CARES_OBJS += src/ares/ares_parse_txt_reply.o
CARES_OBJS += src/ares/ares_process.o
CARES_OBJS += src/ares/ares_query.o
CARES_OBJS += src/ares/ares_search.o
CARES_OBJS += src/ares/ares_send.o
CARES_OBJS += src/ares/ares_strcasecmp.o
CARES_OBJS += src/ares/ares_strdup.o
CARES_OBJS += src/ares/ares_strerror.o
CARES_OBJS += src/ares/ares_timeout.o
CARES_OBJS += src/ares/ares_version.o
CARES_OBJS += src/ares/ares_writev.o
CARES_OBJS += src/ares/bitncmp.o
CARES_OBJS += src/ares/inet_net_pton.o
CARES_OBJS += src/ares/inet_ntop.o

UDT_OBJS =
UDT_OBJS += src/UDT4/src/api.o
UDT_OBJS += src/UDT4/src/buffer.o
UDT_OBJS += src/UDT4/src/cache.o
UDT_OBJS += src/UDT4/src/ccc.o
UDT_OBJS += src/UDT4/src/channel.o
UDT_OBJS += src/UDT4/src/common.o
UDT_OBJS += src/UDT4/src/udt_core.o
UDT_OBJS += src/UDT4/src/epoll.o
UDT_OBJS += src/UDT4/src/list.o
UDT_OBJS += src/UDT4/src/md5.o
UDT_OBJS += src/UDT4/src/packet.o
UDT_OBJS += src/UDT4/src/queue.o
UDT_OBJS += src/UDT4/src/udtc.o
UDT_OBJS += src/UDT4/src/window.o

NACL_OBJS = src/nacl/tweetnacl.o

ifneq (,$(findstring MINGW,$(uname_S)))
include config-mingw.mk
else
include config-unix.mk
endif

TESTS=test/blackhole-server.c test/echo-server.c test/test-*.c
BENCHMARKS=test/blackhole-server.c test/echo-server.c test/dns-server.c test/benchmark-*.c

all: uv.a test/run-benchmarks test/echo-server-udt test/echo-client-udt
>>>>>>> origin/v0.8-udt

else  # Out of tree build.

<<<<<<< HEAD
# Drop all built-in rules.
.SUFFIXES:
=======
$(UDT_OBJS): %.o: %.cpp
	$(CC) -o $*.o -c $(CFLAGS) $(CPPFLAGS) $< -DHAVE_CONFIG_H
	
$(NACL_OBJS): %.o: %.c
	$(CC) -o $*.o -c $(CFLAGS) $(CPPFLAGS) $< -DHAVE_CONFIG_H

test/echo-server-udt: test/echo-server-udt.c uv.a
	$(CC) $(CPPFLAGS) -o test/echo-server-udt test/echo-server-udt.c uv.a -lstdc++ -lpthread -lm -lrt 

test/echo-client-udt: test/echo-server-udt.c uv.a
	$(CC) $(CPPFLAGS) -o test/echo-client-udt test/echo-client-udt.c uv.a -lstdc++ -lpthread -lm -lrt

test/run-tests$(E): test/*.h test/run-tests.c $(RUNNER_SRC) test/runner-unix.c $(TESTS) uv.a
	$(CC) $(CPPFLAGS) $(RUNNER_CFLAGS) -o test/run-tests test/run-tests.c \
		test/runner.c $(RUNNER_SRC) $(TESTS) uv.a $(RUNNER_LIBS) $(RUNNER_LINKFLAGS)
>>>>>>> origin/v0.8-udt

.PHONY:	$(builddir_name)
$(builddir_name): $(builddir_name)/.buildstamp
	$(MAKE) -C $@ -f $(CURDIR)/Makefile $(MAKECMDGOALS) \
		SRCDIR=$(CURDIR) builddir_name=

$(builddir_name)/.buildstamp:
	mkdir -p $(dir $@)
	touch $@

# Add no-op rules for Makefiles to stop make from trying to rebuild them.
Makefile:: ;
%.mk:: ;

# Turn everything else into a no-op rule that depends on the build directory.
%:: $(builddir_name) ;

.PHONY: clean distclean
clean distclean:
	$(RM) -fr $(builddir_name)

<<<<<<< HEAD
endif
=======
test: test/run-tests$(E)
	test/run-tests

#test-%:	test/run-tests$(E)
#	test/run-tests $(@:test-%=%)

bench: test/run-benchmarks$(E)
	test/run-benchmarks

#bench-%:	test/run-benchmarks$(E)
#	test/run-benchmarks $(@:bench-%=%)

clean: clean-platform
	$(RM) -rf src/*.o *.a out test/run-tests$(E) test/run-benchmarks$(E) test/echo-client-udt test/echo-server-udt

distclean: distclean-platform
	$(RM) -rf src/*.o *.a out test/run-tests$(E) test/run-benchmarks$(E) test/echo-client-udt test/echo-server-udt
>>>>>>> origin/v0.8-udt

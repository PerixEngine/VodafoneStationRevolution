## Makefile for the srclib subdirectory of GNU libiconv
## Copyright (C) 2003-2005 Free Software Foundation, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
## USA.

## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = 1.5 gnits no-dependencies
BUILT_SOURCES =
EXTRA_DIST =
MOSTLYCLEANFILES =

RM = rm -f

noinst_LIBRARIES = libicrt.a

# Sources that are compiled on all platforms.

libicrt_a_SOURCES = \
  allocsa.h allocsa.c \
  error.h error.c \
  exit.h \
  pathmax.h \
  progname.h progname.c progreloc.c \
  unlocked-io.h \
  width.c uniwidth.h unitypes.h cjk.h streq.h \
  xalloc.h xmalloc.c xstrdup.c \
  xreadlink.h xreadlink.c

# Sources that are compiled only on platforms that lack the functions.

LIBADD_SOURCE = \
  canonicalize.h canonicalize.c \
  memmove.c \
  readlink.c \
  relocatable.h relocatable.c \
  setenv.h setenv.c unsetenv.c \
  strerror.c

# How to build libicrt.a.

libicrt_a_LIBADD = @LIBOBJS@

# List of files to be distributed.

EXTRA_DIST += \
  $(LIBADD_SOURCE) relocwrapper.c

INCLUDES = -I. -I$(srcdir)

DEFS = -DDEPENDS_ON_LIBICONV=1 -DDEPENDS_ON_LIBINTL=1 @DEFS@


# >>> gnulib module stdbool.
EXTRA_DIST += stdbool_.h

# The following is needed in order to create an <stdbool.h> when the system
# doesn't have one that works.
all-local $(libicrt_a_OBJECTS): @STDBOOL_H@
stdbool.h: stdbool_.h
	sed -e 's/@''HAVE__BOOL''@/@HAVE__BOOL@/g' < $(srcdir)/stdbool_.h > stdbool.h
MOSTLYCLEANFILES += stdbool.h
# <<< gnulib module stdbool.


# >>> gnulib module stdint.
BUILT_SOURCES += $(STDINT_H)
EXTRA_DIST += stdint_.h

# We need the following in order to create an <stdint.h> when the system
# doesn't have one that works with the given compiler.
all-local $(libicrt_a_OBJECTS): $(STDINT_H)
stdint.h: stdint_.h
	sed -e 's/@''HAVE_LONG_64BIT''@/$(HAVE_LONG_64BIT)/g;s/@''HAVE_LONG_LONG_64BIT@/$(HAVE_LONG_LONG_64BIT)/g' < $(srcdir)/stdint_.h > $@-t
	mv $@-t $@
MOSTLYCLEANFILES += stdint.h stdint.h-t
# <<< gnulib module stdint.


# >>> gnulib module alloca.
EXTRA_DIST += alloca_.h

# The following is needed in order to create an <alloca.h> when the system
# doesn't have one that works with the given compiler.
all-local $(libicrt_a_OBJECTS): @ALLOCA_H@
alloca.h: alloca_.h
	cp $(srcdir)/alloca_.h alloca.h
MOSTLYCLEANFILES += alloca.h
# <<< gnulib module alloca.


# Woe32 support.

EXTRA_DIST += Makefile.msvc

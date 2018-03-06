JMK_ROOT=../../
include $(JMK_ROOT)/envir.mak

JMK_EXPORT_HEADERS_DIR=libxml
JMK_EXPORT_HEADERS=libxml.h

JMK_INTERNAL_HEADERS=config.h elfgcchack.h trionan.c trionan.h triodef.h

LIB=xml2.a
JMK_A_TARGET=lib$(LIB)
JMK_LOCAL_A_TARGET=liblocal_$(LIB)

JMK_EXPORT_LIBS+=$(JMK_A_TARGET) $(JMK_LOCAL_A_TARGET)

JMK_O_OBJS_$(JMK_A_TARGET)=SAX.o entities.o encoding.o error.o parserInternals.o \
  parser.o tree.o hash.o list.o xmlIO.o xmlmemory.o uri.o  valid.o xlink.o \
  HTMLparser.o HTMLtree.o debugXML.o xpath.o  xpointer.o xinclude.o nanohttp.o \
  nanoftp.o DOCBparser.o catalog.o globals.o threads.o c14n.o xmlstring.o \
  xmlregexp.o xmlschemas.o xmlschemastypes.o xmlunicode.o xmlreader.o \
  relaxng.o dict.o SAX2.o xmlwriter.o legacy.o chvalid.o pattern.o xmlsave.o \
  xmlmodule.o schematron.o

JMK_O_OBJS_$(JMK_LOCAL_A_TARGET)=$(JMK_O_OBJS_$(JMK_A_TARGET))

LIBXML2_CFLAGS=-DNO_LARGEFILE_SOURCE
JMK_CFLAGS+=$(LIBXML2_CFLAGS)
JMK_LOCAL_CFLAGS+=$(LIBXML2_CFLAGS)

JMK_WARN2ERR=n

JMK_SUBDIRS+=include/libxml

include $(JMKE_MK)


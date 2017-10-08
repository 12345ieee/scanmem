ACLOCAL_AMFLAGS = -I m4

SUBDIRS = test

if ENABLE_GUI
  SUBDIRS += po gui
endif

# '-O2 -g' are added by `configure`, unless the user overrides CFLAGS
AM_CFLAGS = -std=gnu99 -Wall
AM_CXXFLAGS = -std=gnu++11 -Wall -fpermissive -Wno-sign-compare

lib_LTLIBRARIES = libscanmem.la

libscanmem_la_includedir = $(includedir)/scanmem

libscanmem_la_include_HEADERS = commands.h \
    list.h \
    maps.h \
    scanmem.h \
    scanroutines.h \
    sets.h \
    show_message.h \
    targetmem.h \
    value.h

libscanmem_la_SOURCES = commands.cpp \
    common.h \
    ptrace.cpp \
    handlers.h \
    handlers.cpp \
    interrupt.h \
    list.cpp \
    licence.h \
    maps.cpp \
    scanmem.cpp \
    scanroutines.cpp \
    sets.cpp \
    show_message.cpp \
    targetmem.cpp \
    value.cpp

if !HAVE_GETLINE
  libscanmem_la_SOURCES += getline.h \
      getline.cpp
endif

libscanmem_la_LDFLAGS = -version-info 1:0:0 \
                        -export-symbols-regex '^sm_|^show_'

bin_PROGRAMS = scanmem

scanmem_SOURCES = menu.h \
    menu.cpp \
    main.cpp

if !WITH_READLINE
  scanmem_SOURCES += readline.h \
      readline.cpp
endif

scanmem_LDADD = libscanmem.la

dist_man_MANS = scanmem.1
dist_doc_DATA = README

EXTRA_DIST = gpl-3.0.txt lgpl-3.0.txt

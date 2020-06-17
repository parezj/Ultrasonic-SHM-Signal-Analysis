WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = windres

INC = 
CFLAGS = -Wall
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = 

INC_DEBUG = $(INC)
CFLAGS_DEBUG = $(CFLAGS) -g
RESINC_DEBUG = $(RESINC)
RCFLAGS_DEBUG = $(RCFLAGS)
LIBDIR_DEBUG = $(LIBDIR)
LIB_DEBUG = $(LIB)
LDFLAGS_DEBUG = $(LDFLAGS)
OBJDIR_DEBUG = obj/Debug
DEP_DEBUG = 
OUT_DEBUG = bin/Debug/shm_shift

INC_RELEASE = $(INC)
CFLAGS_RELEASE = $(CFLAGS) -O2
RESINC_RELEASE = $(RESINC)
RCFLAGS_RELEASE = $(RCFLAGS)
LIBDIR_RELEASE = $(LIBDIR)
LIB_RELEASE = $(LIB)
LDFLAGS_RELEASE = $(LDFLAGS) -s
OBJDIR_RELEASE = obj/Release
DEP_RELEASE = 
OUT_RELEASE = bin/Release/shm_shift

OBJ_DEBUG = $(OBJDIR_DEBUG)/lib/libcsv/libcsv.o $(OBJDIR_DEBUG)/src/example.o $(OBJDIR_DEBUG)/src/shm_shift.o

OBJ_RELEASE = $(OBJDIR_RELEASE)/lib/libcsv/libcsv.o $(OBJDIR_RELEASE)/src/example.o $(OBJDIR_RELEASE)/src/shm_shift.o

all: debug release

clean: clean_debug clean_release

before_debug: 
	test -d bin/Debug || mkdir -p bin/Debug
	test -d $(OBJDIR_DEBUG)/lib/libcsv || mkdir -p $(OBJDIR_DEBUG)/lib/libcsv
	test -d $(OBJDIR_DEBUG)/src || mkdir -p $(OBJDIR_DEBUG)/src

after_debug: 

debug: before_debug out_debug after_debug

out_debug: before_debug $(OBJ_DEBUG) $(DEP_DEBUG)
	$(LD) $(LIBDIR_DEBUG) -o $(OUT_DEBUG) $(OBJ_DEBUG)  $(LDFLAGS_DEBUG) $(LIB_DEBUG)

$(OBJDIR_DEBUG)/lib/libcsv/libcsv.o: lib/libcsv/libcsv.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c lib/libcsv/libcsv.c -o $(OBJDIR_DEBUG)/lib/libcsv/libcsv.o

$(OBJDIR_DEBUG)/src/example.o: src/example.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/example.c -o $(OBJDIR_DEBUG)/src/example.o

$(OBJDIR_DEBUG)/src/shm_shift.o: src/shm_shift.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c src/shm_shift.c -o $(OBJDIR_DEBUG)/src/shm_shift.o

clean_debug: 
	rm -f $(OBJ_DEBUG) $(OUT_DEBUG)
	rm -rf bin/Debug
	rm -rf $(OBJDIR_DEBUG)/lib/libcsv
	rm -rf $(OBJDIR_DEBUG)/src

before_release: 
	test -d bin/Release || mkdir -p bin/Release
	test -d $(OBJDIR_RELEASE)/lib/libcsv || mkdir -p $(OBJDIR_RELEASE)/lib/libcsv
	test -d $(OBJDIR_RELEASE)/src || mkdir -p $(OBJDIR_RELEASE)/src

after_release: 

release: before_release out_release after_release

out_release: before_release $(OBJ_RELEASE) $(DEP_RELEASE)
	$(LD) $(LIBDIR_RELEASE) -o $(OUT_RELEASE) $(OBJ_RELEASE)  $(LDFLAGS_RELEASE) $(LIB_RELEASE)

$(OBJDIR_RELEASE)/lib/libcsv/libcsv.o: lib/libcsv/libcsv.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c lib/libcsv/libcsv.c -o $(OBJDIR_RELEASE)/lib/libcsv/libcsv.o

$(OBJDIR_RELEASE)/src/example.o: src/example.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/example.c -o $(OBJDIR_RELEASE)/src/example.o

$(OBJDIR_RELEASE)/src/shm_shift.o: src/shm_shift.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c src/shm_shift.c -o $(OBJDIR_RELEASE)/src/shm_shift.o

clean_release: 
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)
	rm -rf bin/Release
	rm -rf $(OBJDIR_RELEASE)/lib/libcsv
	rm -rf $(OBJDIR_RELEASE)/src

.PHONY: before_debug after_debug clean_debug before_release after_release clean_release


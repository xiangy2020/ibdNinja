# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -O2 -Irapidjson/include -Izlib/zlib-1.2.13/ibdNinja/include

LDFLAGS = -Lzlib/zlib-1.2.13/ibdNinja/lib -lz -Wl,-rpath,zlib/zlib-1.2.13/ibdNinja/lib

# Target
TARGET = ibdNinja

# Source files, object files, and target
SRCS = main.cc ibdNinja.cc ibdUtils.cc
OBJS = $(SRCS:.cc=.o)

# Default target
all: $(TARGET)

ZLIB_DIR = $(CURDIR)/zlib/zlib-1.2.13
ZLIB_LIB = $(ZLIB_DIR)/ibdNinja/lib/libz.a

check_zlib:
	@if [ ! -f $(ZLIB_LIB) ]; then \
		echo "Building zlib..."; \
		cd $(ZLIB_DIR) && ./configure --prefix=$(ZLIB_DIR)/ibdNinja && make; make install; \
	fi

# Build the target
$(TARGET): check_zlib $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

-include *.d
# Compile each source file into an object file
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -MMD -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET) $(SRCS:.cc=.d)

# Phony targets
.PHONY: all clean

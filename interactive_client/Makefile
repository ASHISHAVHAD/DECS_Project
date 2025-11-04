# interactive_client/Makefile

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
# Include directory for httplib.h (assuming it's in the parent kv_project/ directory)
INCLUDES = -I. -I..

# Linker flags
# httplib does not typically require extra linker flags unless SSL support is enabled.
# If you enable SSL, you would add -lssl -lcrypto here.
LDFLAGS =

# Source files
SRCS = main.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = interactive_client

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
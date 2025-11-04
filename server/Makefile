# server/Makefile

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
# Include directory for httplib.h (assuming it's in the parent directory)
# and for other local headers
INCLUDES = -I. -I..

# Linker flags
# Libraries: MySQL C++ Connector, pthreads
LDFLAGS = -lmysqlcppconn -lpthread

# Source files (ADD logger.cpp here)
SRCS = cache.cpp database.cpp server_app.cpp logger.cpp main.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Executable name
TARGET = kv_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
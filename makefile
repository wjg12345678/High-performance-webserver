CXX ?= g++

TARGET := server
SRC_DIRS := app src/app src/server src/http src/log src/db src/timer
SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.cpp))
OBJECTS := $(SOURCES:.cpp=.o)

DEBUG ?= 1
CPPFLAGS += -Iinclude
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
endif

LDFLAGS += -lpthread -lmysqlclient

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

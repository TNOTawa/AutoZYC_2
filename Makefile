# AutoZYCParse.mod2 ? RPP/MIDI ???? + ???????
# Build: mingw32-make (GNU Make for MinGW-W64 x86_64)
# Output: AutoZYCParse.mod2 (64-bit DLL for Aviutl2)

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -m64 -shared -static
LDFLAGS = -static

TARGET = AutoZYCParse.mod2
SRC = AutoZYCParse.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	cmd /c "if exist $(TARGET) del /f $(TARGET)"
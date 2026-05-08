# AutoZYC.aux2 — 共用插件 (物件批量生成 + 菜单注册 + 配置持久化)
# Build: mingw32-make (GNU Make for MinGW-W64 x86_64)
# Output: build/AutoZYC.aux2 (64-bit DLL for Aviutl2)

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -m64 -shared -static -I../../ 
LDFLAGS = -static -lcomdlg32

TARGET = build\AutoZYC.aux2
SRC = AutoZYC.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	cmd /c "if exist $(TARGET) del /f $(TARGET)"

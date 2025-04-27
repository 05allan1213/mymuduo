# --- 编译器设置 ---
CXX = g++
CXXFLAGS = -g -I/usr/local/include/mymuduo
LDFLAGS = -L/usr/local/lib
LDLIBS = -lmymuduo -lpthread

# --- 目标设置 ---
TARGET = test_server
SRC = test_server.cc

# --- 规则定义 ---
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS) $(LDLIBS)

.PHONY: all clean

clean:
	rm -f $(TARGET)


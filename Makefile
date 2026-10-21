CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -fPIC -I. $(shell pkg-config --cflags Qt6Widgets Qt6Gui)
LDLIBS := $(shell pkg-config --libs Qt6Widgets Qt6Gui) -lz -lcrypto

PY_CXXFLAGS := $(shell python3-config --includes) -O2 -fPIC
PY_LDLIBS := $(shell python3-config --libs) -lz -lcrypto

MOC ?= /usr/lib/qt6/moc

SRC_DIR := src
GUI_DIR := gui

SRCS := $(SRC_DIR)/sudokucode.cpp \
        $(GUI_DIR)/mainwindow.cpp \
        $(GUI_DIR)/gridwidget.cpp \
        $(GUI_DIR)/gui_main.cpp

MOC_SRCS := $(GUI_DIR)/moc_mainwindow.cpp $(GUI_DIR)/moc_gridwidget.cpp

OBJS := $(SRCS:.cpp=.o) $(MOC_SRCS:.cpp=.o)

TARGET := sudokucode_gui
PY_TARGET := sudokucode.so

.PHONY: all clean py
all: $(TARGET)

py: $(PY_TARGET)

$(GUI_DIR)/moc_%.cpp: $(GUI_DIR)/%.h
	$(MOC) $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(PY_TARGET): $(SRC_DIR)/sudokucode.cpp pybind/sudokucode_module.cpp
	$(CXX) $(PY_CXXFLAGS) -shared -o $@ $(SRC_DIR)/sudokucode.cpp pybind/sudokucode_module.cpp $(PY_LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(PY_TARGET) $(MOC_SRCS) pybind/*.o

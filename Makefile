CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -fPIC -I. $(shell pkg-config --cflags Qt6Widgets Qt6Gui)
LDLIBS := $(shell pkg-config --libs Qt6Widgets Qt6Gui)

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

.PHONY: all clean
all: $(TARGET)

$(GUI_DIR)/moc_%.cpp: $(GUI_DIR)/%.h
	$(MOC) $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(MOC_SRCS)

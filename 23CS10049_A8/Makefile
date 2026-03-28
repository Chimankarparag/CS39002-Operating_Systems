CXX      = g++
CXXFLAGS_MAC = -Wall -g -std=c++17 -D_Alignof=__alignof -lpthread
CXXFLAGS_LINUX = -Wall -g -std=c++17 -lpthread

TARGET = manager
SRCS   = manager.cpp global.cpp

compile:
	$(CXX) $(CXXFLAGS_MAC) -o $(TARGET) $(SRCS)

run: compile
	./$(TARGET)

demorun: compile
	./$(TARGET) 5 15 25

clean:
	rm -f $(TARGET)

.PHONY: compile run demorun clean

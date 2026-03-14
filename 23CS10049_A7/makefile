CXX 	 = g++
C 		 = gcc
CFLAGS	 = -Wall -Wextra -g 
CXXFLAGS_LINUX = -Wall -g -std=c++17 
CXXFLAGS_MAC = -Wall -g -std=c++17 -D_Alignof=__alignof
LDFLAGS  = -pthread 

TARGET   = manufacture
GEN      = genschedule

.PHONY: compile run rundemo clean

compile: $(TARGET) $(GEN)

$(TARGET): foobar.cpp global.cpp manager.cpp work.cpp
	$(CXX) $(CXXFLAGS_LINUX) -o $(TARGET) foobar.cpp $(LDFLAGS)

$(GEN): genschedule.c
	$(C) $(CFLAGS) -o $(GEN) genschedule.c

run-genschedule: $(GEN)
	./$(GEN) 16 20 3 5 fooschedule.txt barschedule.txt

run: $(TARGET) 
	./$(TARGET) 2 1 fooschedule.txt barschedule.txt

rundemo: $(TARGET) 
	./$(TARGET) 2 1 demofoo.txt demobar.txt
clean:
	rm -f $(TARGET) $(GEN)

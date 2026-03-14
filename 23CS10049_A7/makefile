CLANG    = clang++
GPP 	 = g++
C 		 = gcc
CFLAGS	 = -Wall -Wextra -g
CXXFLAGS = -Wall -g -std=c++17 -stdlib=libc++
LDFLAGS  = -pthread

TARGET   = manufacture
GEN      = genschedule

.PHONY: compile run rundemo clean

compile: $(TARGET) $(GEN)

$(TARGET): foobar.cpp global.cpp manager.cpp work.cpp
	$(CLANG) $(CXXFLAGS) -o $(TARGET) foobar.cpp $(LDFLAGS)

$(GEN): genschedule.c
	$(C) $(CFLAGS) -o $(GEN) genschedule.c

run-genschedule:
	./$(GEN) 16 20 3 5 fooschedule.txt barschedule.txt

run: $(TARGET) 
	./$(TARGET) 2 1 fooschedule.txt barschedule.txt

rundemo: run

clean:
	rm -f $(TARGET) $(GEN)
	rm -f demofoo.txt demobar.txt

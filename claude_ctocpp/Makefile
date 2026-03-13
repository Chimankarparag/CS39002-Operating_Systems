CXX      = clang++
CXXFLAGS = -Wall -g -std=c++17 -stdlib=libc++
LDFLAGS  = -pthread

TARGET   = manufacture
GEN      = genschedule

.PHONY: compile run rundemo clean

compile: $(TARGET) $(GEN)

$(TARGET): foobar.cpp global.cpp manager.cpp work.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) foobar.cpp $(LDFLAGS)

$(GEN): genschedule.cpp
	$(CXX) $(CXXFLAGS) -o $(GEN) genschedule.cpp

run: $(TARGET) $(GEN)
	./$(GEN) 16 20 3 5 demofoo.txt demobar.txt
	./$(TARGET) 2 1 demofoo.txt demobar.txt

rundemo: run

clean:
	rm -f $(TARGET) $(GEN)
	rm -f demofoo.txt demobar.txt fooschedule.txt barschedule.txt

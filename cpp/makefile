CPP      = g++
CPPFLAGS  = -Wall -g std=c++17
LDFLAGS = -pthread

TARGET   = manufacture

fooschedule.txt: run-genschedule
barschedule.txt: run-genschedule

genschedule: genschedule.cpp
	$(CPP) $(CPPFLAGS) -o genschedule genschedule.cpp

$(TARGET): foobar.cpp global.cpp manager.cpp work.cpp
	$(CPP) $(CPPFLAGS) -o $(TARGET) foobar.cpp $(LDFLAGS)

run-genschedule: genschedule
	./genschedule 16 20 3 5 fooschedule.txt barschedule.txt

run: $(TARGET) fooschedule.txt barschedule.txt
	./$(TARGET) 2 1 fooschedule.txt barschedule.txt

clean:
	rm -f $(TARGET)
	rm -f fooschedule.txt barschedule.txt

.PHONY: genschedule run run-genschedule clean
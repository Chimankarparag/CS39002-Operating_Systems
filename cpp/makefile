CPP      = g++
CPPFLAGS = -Wall -g -std=c++17
LDFLAGS  = -pthread

TARGET   = manufacture
GENSCHED = genschedule

# ── Build the main simulation ────────────────────────────────────────────
# foobar.cpp #include-s the other three files, so it is the only
# translation unit that needs to be compiled.
compile: $(TARGET)

$(TARGET): foobar.cpp global.cpp manager.cpp work.cpp
	$(CPP) $(CPPFLAGS) -o $(TARGET) foobar.cpp $(LDFLAGS)

# ── Build the schedule generator ─────────────────────────────────────────
genschedule: $(GENSCHED)

$(GENSCHED): genschedule.c
	gcc -Wall -o $(GENSCHED) genschedule.c

# ── Generate default schedule files and run ──────────────────────────────
fooschedule.txt barschedule.txt: $(GENSCHED)
	./$(GENSCHED) 32 40 8 10 fooschedule.txt barschedule.txt

run: $(TARGET) fooschedule.txt barschedule.txt
	./$(TARGET) 2 1 fooschedule.txt barschedule.txt

# ── Generate small demo files (16/20 parts, 3/5 workers) and run ─────────
demofoo.txt demobar.txt: $(GENSCHED)
	./$(GENSCHED) 16 20 3 5 demofoo.txt demobar.txt

rundemo: $(TARGET) demofoo.txt demobar.txt
	./$(TARGET) 2 1 demofoo.txt demobar.txt

# ── Remove all generated artefacts ───────────────────────────────────────
clean:
	rm -f $(TARGET) $(GENSCHED)
	rm -f fooschedule.txt barschedule.txt demofoo.txt demobar.txt

.PHONY: compile run genschedule rundemo clean

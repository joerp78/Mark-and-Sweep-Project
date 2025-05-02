_DEPS = heap.h gc.h
_OBJ = heap.o gc.o
_MOBJ = main.o
# No test files for now
_TOBJ = test.o

APPBIN = marksweep_app
TESTBIN = marksweep_test

DEBUG = -DDEBUGMODE

IDIR = include
CC = g++
CFLAGS = -I$(IDIR) -Wall $(DEBUG) -Wextra -g -pthread
ODIR = obj
SDIR = src
LDIR = lib
TDIR = test
LIBS = -lm
XXLIBS = $(LIBS) -lstdc++ -lgtest -lgtest_main -lpthread
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
MOBJ = $(patsubst %,$(ODIR)/%,$(_MOBJ))
TOBJ = $(patsubst %,$(ODIR)/%,$(_TOBJ))

# Create obj directory if missing
$(shell mkdir -p $(ODIR))

$(ODIR)/%.o: $(SDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

# (This rule can stay even if no test directory/files yet)
$(ODIR)/%.o: $(TDIR)/%.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(APPBIN) $(TESTBIN) submission

$(APPBIN): $(OBJ) $(MOBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

$(TESTBIN): $(TOBJ) $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(XXLIBS)	

submission:
	zip -r submission src lib include

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
	rm -f $(APPBIN) $(TESTBIN)
	rm -f submission.zip
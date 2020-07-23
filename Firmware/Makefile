IDIR =..
DEFINES=LT_LINUX

CC=gcc
CXX=g++

CFLAGS=$(addprefix -I,$(IDIR)) $(addprefix -D,$(DEFINES)) -std=c99 -fmax-errors=1 -lm -ljsoncpp -ljansson `pkg-config --cflags --libs opencv`
CXXFLAGS=$(addprefix -I,$(IDIR)) $(addprefix -D,$(DEFINES)) -std=c++11 -fmax-errors=1 -pthread -lm -ljsoncpp -ljansson `pkg-config --cflags --libs opencv`

LIBS=modbus
LDFLAGS=$(addprefix -l,$(LIBS))

ODIR=obj

_OBJ = IS16read.o leddar_v4_0.o dbclient.o display.o
_OBJ_TEST = leddar_v4_0.o dbclient.o display.o
_OBJ_DISP = display.o dbclient.o displayinterface.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ_TEST = $(patsubst %,$(ODIR)/%,$(_OBJ_TEST))
OBJ_DISP = $(patsubst %,$(ODIR)/%,$(_OBJ_DISP))

all: IS16read

$(ODIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

$(ODIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

IS16read: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

IS16read_test: $(OBJ_TEST)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)

display: $(OBJ_DISP)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LDFLAGS)
	
.PHONY: clean all

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 


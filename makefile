

CC=g++
CFLAGS= -Wall -c -std=c++11 -I../i686-w64-mingw32/include 
LDFLAGS=-L../i686-w64-mingw32/lib -lmingw32 -mwindows -mconsole  -lSDL2main -lSDL2
SOURCES=burlington.cpp graphics.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=burlo

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS)  -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o && rm $(EXECUTABLE)

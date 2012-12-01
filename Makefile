CXX=g++ -D__MACOSX_CORE__ 
FLAGS=-c -g -Wall
LIBS=-framework OpenGL -framework GLUT -lportaudio

OBJS=fft.o Diana.o Thread.o

Diana: $(OBJS)
	$(CXX) -o Diana $(OBJS) $(LIBS)

fft.o: fft.h fft.c
	$(CXX) $(FLAGS) fft.c

Thread.o: Thread.h Thread.cpp
	$(CXX) $(FLAGS) Thread.cpp

clean:
	rm -f *~ *# *.o Diana

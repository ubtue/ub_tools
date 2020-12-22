x: x.o y.o
	g++ x.o y.o -o x

x.o: x.cc
	g++ -c x.cc

y.o: y.cc
	g++ -c y.cc

clean:
	rm -f *.o x *~

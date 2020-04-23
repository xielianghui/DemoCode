all:rawcode.so call

rawcode.so:raw_code.o
	g++ -shared -o rawcode.so raw_code.o
raw_code.o:raw_code.cpp
	g++ -D EXTERNC -g -c -fPIC raw_code.cpp
call:call_code.cpp
	g++ call_code.cpp -o call -ldl
clean:
	rm -rf raw_code.o

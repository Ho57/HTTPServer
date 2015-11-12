CXX = g++ -fPIC
NETLIBS= -lnsl

all: daytime-server use-dlopen hello.so myhttpd jj-mod.so

daytime-server : daytime-server.o
	$(CXX) -o $@ $@.o $(NETLIBS)

use-dlopen: use-dlopen.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

hello.so: hello.o
	ld -G -o hello.so hello.o

jj-mod.so: jj-mod.c
	gcc -shared -o jj-mod.so -fPIC jj-mod.c

%.o: %.cc
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<

myhttpd: myhttpd.cpp
	$(CXX) -g -o myhttpd myhttpd.cpp $(NETLIBS) -lpthread -ldl

clean:
	rm -f *.o use-dlopen jj-mod.so
	rm -f *.o use-dlopen hello.so
	rm -f *.o daytime-server
	rm -f *.o myhttpd


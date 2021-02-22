LIBNAME = libsession
	
CFLAGS = -O3

all: 
	gcc -O3 -c -Wall -Werror -fpic $(LIBNAME).c
	gcc -O3 -shared -o $(LIBNAME).so $(LIBNAME).o

install:
	cp $(LIBNAME).so /usr/lib
	chmod 0755 /usr/lib/$(LIBNAME).so
	ldconfig

clean:
	rm *.so
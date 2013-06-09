
INCLUDES += -Ilib -Igl-matrix.c -Ihidapi/hidapi
CFLAGS = -fPIC -g

.c.o:
	${CC} $(CFLAGS) -c ${INCLUDES} $<

LDFLAGS= -L/usr/local/lib

#HID_LIB=-lhidapi-hidraw
HID_LIB=/usr/local/lib/libhidapi-hidraw.a -ludev

#LIBS= -Llibovr_nsb -lovr_nsb -Lgl-matrix.c -lgl-matrix -lglut -lGL -lGLU -lpthread -lm
LIBS= lib/libovr_nsb.a gl-matrix.c/libgl-matrix.a -lglut -lGL -lGLU -lm $(HID_LIB)
THREADS=-lpthread

all: extralibs gltest1 consoletest threadtest hidapitest

gltest1: gltest1.o 
	$(CC) $< $(LDFLAGS) $(LIBS) $(THREADS) -o $@

consoletest: consoletest.o 
	$(CC) $< $(LDFLAGS) $(LIBS) -o $@

threadtest: threadtest.o 
	$(CC) $< $(LDFLAGS) $(LIBS) $(THREADS) -o $@

hidapitest: hidapitest.o 
	$(CC) $< $(LDFLAGS) $(LIBS) $(THREADS) -o $@
		
extralibs:
	make -C gl-matrix.c
	make -C lib

clean: 
	rm -f *.o gltest1 consoletest threadtest hidapitest
	make -C gl-matrix.c clean
	make -C lib clean

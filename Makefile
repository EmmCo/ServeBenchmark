CC?=	gcc
LIBS?=
LFLAGS?=  -lpthread
PREFIX?= /usr/local
VERSION=0.1
TMPDIR=/tmp/SerTanaBenchmark-$(VERSION)
 
SerTanaBenchmark:   
	$(CC)  SerTanaBenchmark.c rbtree.c -o SerTanaBenchmark $(LFLAGS)  $(LIBS)  

install: SerTanaBenchmark
	install -s SerTanaBenchmark  $(PREFIX)/bin	
	 
clean:
	-rm -f *.o SerTanaBenchmark *~ core *.core  
	
tar:   clean
	 
	rm -rf $(TMPDIR)
	install -d $(TMPDIR)
	cp -p Makefile SerTanaBenchmark.c $(TMPDIR) 
	-cd $(TMPDIR) && cd .. && tar cozf SerTanaBenchmark-$(VERSION).tar.gz SerTanaBenchmark-$(VERSION)

 

.PHONY: clean install all tar

all: src src_extras parser

clean: src-clean src_extras-clean parser-clean

src:
	$(MAKE) -f Makefile.msys -C ../src

src-clean:
	$(MAKE) -f Makefile.msys -C ../src clean

src_extras:
	$(MAKE) -f Makefile.msys -C ../src_extras

src_extras-clean:
	$(MAKE) -f Makefile.msys -C ../src_extras clean

parser:
	$(MAKE) -f Makefile.msys -C ../parser

parser-clean:
	$(MAKE) -f Makefile.msys -C ../parser clean

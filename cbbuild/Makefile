all:
	@echo " "
	@echo use:
	@echo "   make linux           for compiling everything in GNU/Linux or Mac OSX"
	@echo "   mingw32-make win32   for compiling everything in Microsoft Windows"
	@echo in case of error, or missing some dependency, read \"compiling.txt\".
	@echo " "


msys-win32:
	$(MAKE) -f Makefile.msys -C ../src
	$(MAKE) -f Makefile.msys -C ../parser
	$(MAKE) -f Makefile.msys -C ../src_extras

msys-win32-clean:
	$(MAKE) -f Makefile.msys -C ../src clean
	$(MAKE) -f Makefile.msys -C ../parser clean
	$(MAKE) -f Makefile.msys -C ../src_extras clean

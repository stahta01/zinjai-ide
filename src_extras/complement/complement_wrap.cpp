#include<cstring>
#include<iostream>
#if defined(__WIN32__)
#	include<io.h>
#	include<windows.h>
#	include <conio.h>
#else
#	include<cstdlib>
#	if !defined(__APPLE__)
#		include<wait.h>
#	endif
#	include <termios.h>
#	include <unistd.h>
#endif

using namespace std;

int main(int argc, char *argv[]) {
	if (argc<2) return 1;
#if defined(__WIN32__)

		string command;
		command+="\"";
		command+=argv[1];
		command+="\" ";
		for (int i=2;i<argc;i++) {
			command+="\"";
			command+=argv[i];
			command+="\" ";
		}

		STARTUPINFO si;
		PROCESS_INFORMATION pi;
		
		memset( &si, 0, sizeof(si) );
		si.cb = sizeof(si);
		memset( &pi, 0, sizeof(pi) );
		
		// Start the child process.
		CreateProcess( NULL, // No module name (use command line).
			(char*)command.c_str(), // Command line.
			NULL,             // Process handle not inheritable.
			NULL,             // Thread handle not inheritable.
			FALSE,            // Set handle inheritance to FALSE.
			0,                // No creation flags.
			NULL,             // Use parent's environment block.
			NULL,             // Use parent's starting directory.
			&si,              // Pointer to STARTUPINFO structure.
			&pi               // Pointer to PROCESS_INFORMATION structure.
			);
		
#else
	system((string("sudo ")+argv[1]).c_str());
	return 0;
#endif
}

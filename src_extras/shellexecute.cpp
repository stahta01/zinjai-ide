#include <windows.h>
int main(int argc, char *argv[]) {
	if (argc==2) ShellExecute(NULL,"open",argv[1],argv[1],NULL,SW_NORMAL);	
	return 0;
}

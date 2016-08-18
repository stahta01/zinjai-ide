#include <iostream>
#include <shlobj.h>
using namespace std;

int main(int argc, char *argv[]) {
	LPITEMIDLIST pidl;
	if (SHGetSpecialFolderLocation(NULL,CSIDL_DESKTOP,&pidl)!=NOERROR) return 1;
	char buf[MAX_PATH];
	SHGetPathFromIDList(pidl,buf);
	string out(buf);
	if (!out.size()) return 2;
	if (out[out.size()-1]!='\\') out+="\\";
	out+="zinjai-log.txt";
	string cmd = "MinGW\\bin\\gdb";
	cmd += " --ex \"set confirm off\"";
	cmd += " --ex \"set height 0\"";
	cmd += " --ex run";
	cmd += " --ex \"bt full\"";
	cmd += " --ex quit";
	cmd += " --args zinjai.exe --for-gdb";
	cmd += " > ";
	cmd += out;
	system(cmd.c_str());
	return 0;
}


#include <iostream>
#include <sys/stat.h>
#include <fstream>
#include <cstring>
using namespace std;

int main(int argc, char *argv[]) {
	char path[2048],cwd[1024];
	
	getcwd(cwd,1024); // get current working dir
	int lcwd=strlen(cwd); // remove the last /bin to go back to zinjai/resources directory
	if (strncmp(cwd+lcwd-4,"/bin",4)==0) cwd[lcwd-4]='\0';
	else if (strncmp(cwd+lcwd-5,"/bin/",5)==0) cwd[lcwd-5]='\0';
	
	// build a command to run that auxiliar script
	strcpy(path,"open -W "); // -W is for this process to wait the launched process, not async
	strcat(path,getenv("HOME"));
	strcat(path,"/.zinjai/runner.command");
	
	char *fname = path +8;
	
	// write the auxiliar script that actually does the job
	ofstream f(fname,ios::trunc); // 8 es el len de "open -W "
	f<<"echo -e \\\\E]0\\;'Zinjai - Consola de Ejecucion'\\\\a && clear && cd \"";
	f<<cwd<<"\" &&";
	for (int i=1;i<argc;i++)
		f<<" \""<<argv[i]<<"\"";
	f.close();
	chmod(fname,448);
	
	// run the command that launch the script
	system(path);
	return 0;
}


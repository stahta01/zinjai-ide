#ifndef ENVVARS_H
#define ENVVARS_H
#include <wx/string.h>

class EnvVars {
public:
	enum mode_t { NONE, COMPILING, RUNNING, DEBUGGING };
	static void Init(const wxString &zinjai_dir);
	static void SetMode(mode_t);
	static void Reset();
};

#endif


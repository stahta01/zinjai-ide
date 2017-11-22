#ifndef OSDEP_H
#define OSDEP_H

class OSDep {
public:
	static void AppInit();
	static int GetDPI();
	static bool SetFocus(unsigned long pid);
	static long GetChildPid(long pid);
	
#ifdef __WIN32__
	static bool winLoadDBP();
	static bool winDebugBreak(long proc_id);
#endif
};

#endif

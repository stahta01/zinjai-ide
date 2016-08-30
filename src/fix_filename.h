#ifndef FIX_FILENAME_H
#define FIX_FILENAME_H
#include <wx/string.h>

inline wxString fix_filename(wxString fname){
#ifdef __WIN32__
	fname.Replace("*","__start__",true);
	fname.Replace("?","__question__",true);
	fname.Replace("|","__pipe__",true);
//	fname.Replace("/","__slash__",true);
//	fname.Replace("\\","__backslask__",true);
//	fname.Replace("\"","__doublequote__",true);
	fname.Replace("\'","__quote__",true);
	fname.Replace(":","__colon__",true);
	fname.Replace(">","__greater__",true);
	fname.Replace("<","__lower__",true);
//	fname.Replace("&","__amp__",true);
#endif
	return fname;
}

#endif

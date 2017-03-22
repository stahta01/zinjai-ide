#include "Application.h"
#include "mxInfoWindow.h"
#include "ComplementArchive.h"
#include "mxCreateComplementWindow.h"
#include <iostream>
#include <wx/msgdlg.h>
#include "mac-stuff.h"
using namespace std;

bool spanish=false;

bool mxApplication::OnInit() {
	wxString zpath,fname;
	bool for_autobuilding = false;
	for(int i=1;i<argc;i++) { 
		wxString argvi(argv[i]);
		if (argvi.StartsWith("--lang=")) {
			spanish=argvi=="--lang=spanish";
		} else if (argvi=="--build") {
			for_autobuilding = true;
		} else {
			if (zpath.Len()==0) zpath=argvi; else fname=argvi;
		}
	}
	
	fix_mac_focus_problem();
	
#ifndef __WIN32__
	cerr<<(spanish?"\nNo cierre esta ventana.\n":"\nDo not close this window.\n");
#endif
	if (fname.Len() && !for_autobuilding )
		new mxInfoWindow(zpath,fname);
	else
		new mxCreateComplementWindow(zpath,fname);
	return true;
}

IMPLEMENT_APP(mxApplication)


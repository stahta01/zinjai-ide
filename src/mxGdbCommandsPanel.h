#ifndef MXGDBCOMMANDSPANEL_H
#define MXGDBCOMMANDSPANEL_H
#include <wx/panel.h>
#include <wx/stc/stc.h>
#include "DebugManager.h"

class wxTextCtrl;
class wxTimer;
class mxStyledOutput;

class mxGdbCommandsPanel : public wxPanel, public myBTEventHandler {
private:
	wxTextCtrl *input;
	mxStyledOutput *output;
public:
	void OnInput(wxCommandEvent &event);
	void SetFocusToInput();
	mxGdbCommandsPanel();
	void ProcessAnswer(wxString &ans);
	void PrintMessage(const wxString &str);
	
	virtual void OnDebugStart() override { PrintMessage("gdb started"); }
	virtual void OnDebugStop() override { PrintMessage("gdb stopped"); }
//	virtual void OnBacktraceUpdated(bool was_running) override { PrintMessage("backtrace updated"); }
	DECLARE_EVENT_TABLE();
};

#endif


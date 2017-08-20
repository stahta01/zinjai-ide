#ifndef MXHIDENPANEL_H
#define MXHIDENPANEL_H
#include <wx/panel.h>
#include <wx/timer.h>

class mxMainWindow;
enum hp_pos {HP_LEFT, HP_BOTTOM, HP_RIGHT};

class mxHidenPanel : public wxPanel {
private:
	wxString label;
	bool selected;
	bool mouse_in;
	bool showing;
	bool forced_show;
	hp_pos pos;
	static int used_bottom, used_right,used_left;
	static int used_bottom_right, used_bottom_left, used_right_bottom ,used_left_bottom;
	static int ignore_autohide;
	static mxMainWindow *main_window;
	friend class mxAUI;
public:
	wxTimer *timer;
	wxWindow *control;
	mxHidenPanel(wxWindow *parent, wxWindow *acontrol, hp_pos apos, wxString alabel);
	void OnPaint(wxPaintEvent &evt);
	void ProcessClose();
	void Hide();
	void ShowFloat(bool set_focus);
	void ShowDock();
	void ToggleFull();
	void ToggleDock();
	void ForceShow(bool set_focus);
	void ProcessParentResize();
	void OnClick(wxMouseEvent &evt);
	void OnMotion(wxMouseEvent &evt);
	void OnTimer(wxTimerEvent &evt);
	void OnResize(wxSizeEvent &evt);
	bool IsDocked() const { return selected; }
	bool IsVisible() const { return showing; }
	DECLARE_EVENT_TABLE();
	friend class mxHidenPanelIgnoreGuard;
};

class mxHidenPanelIgnoreGuard {
public:
	mxHidenPanelIgnoreGuard() { ++mxHidenPanel::ignore_autohide; }
	~mxHidenPanelIgnoreGuard() { --mxHidenPanel::ignore_autohide; }
};

#endif


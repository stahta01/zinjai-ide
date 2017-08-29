#ifndef MXDIFFSIDEBAR_H
#define MXDIFFSIDEBAR_H
#include <wx/window.h>
#include <wx/pen.h>
#include "Cpp11.h"

class mxSource;

class mxDiffSideBar : public wxWindow {
	static mxDiffSideBar *m_instance;
	wxPen pen_del,pen_add,pen_mod;
	int last_h;
	mxDiffSideBar();
public:
	void OnPaint(wxPaintEvent &event);
	void OnMouseDown(wxMouseEvent &evt);
	void OnMouseWheel(wxMouseEvent &evt);
	static bool HaveInstance() { return m_instance!=nullptr; }
	static mxDiffSideBar &GetInstance() { 
		if (!m_instance) m_instance = new mxDiffSideBar();
		return *m_instance; 
	}
	~mxDiffSideBar() { m_instance = nullptr; }
	DECLARE_EVENT_TABLE();
};

#endif


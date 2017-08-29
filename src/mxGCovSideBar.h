#ifndef MXGCOVSIDEBAR_H
#define MXGCOVSIDEBAR_H
#include <wx/panel.h>
#include <wx/timer.h>
#include "Cpp11.h"

class mxSource;

class mxGCovSideBar : public wxWindow {
	static mxGCovSideBar *m_instance;
	int line_count, *hits, hits_max;
	wxString last_path;
	bool ShouldLoadData(mxSource *src);
	mxSource *should_refresh;
	mxSource *src_load;
	mxGCovSideBar();
public:
	void LoadData(bool force=false);
	void OnPaint(wxPaintEvent &event);
	void OnPopup(wxMouseEvent &event);
	void OnRefresh(wxCommandEvent &event);
	void Refresh(mxSource *src);
	static bool HaveInstance() { return m_instance!=nullptr; }
	static mxGCovSideBar &GetInstance() { 
		if (!m_instance) m_instance = new mxGCovSideBar();
		return *m_instance; 
	}
	~mxGCovSideBar() { m_instance = nullptr; }
	DECLARE_EVENT_TABLE();
};

#endif


#ifndef MXMINIMAPPANEL_H
#define MXMINIMAPPANEL_H
#include <wx/panel.h>
#include <wx/sizer.h>
#include "Cpp11.h"

class mxSource;
class mxMiniSource;

class mxMiniMapPanel : public wxPanel {
	mxSource *m_current_src;
	mxMiniSource *m_current_map;
	wxBoxSizer *m_sizer;
public:
	mxMiniMapPanel(wxWindow *parent);
	void SetCurrentSource(mxSource *src);
	void UnregisterSource(mxSource *src);
};

#endif


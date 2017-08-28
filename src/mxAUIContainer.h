#ifndef MXAUICONTAINER_H
#define MXAUICONTAINER_H
#include <wx/aui/auibook.h>
#include <wx/arrstr.h>
#include "mxAUI.h"

class mxAUIContainer : public wxAuiNotebook {
	bool m_for_side_panel;
	wxArrayString m_names;	
public:
	mxAUIContainer(wxWindow *parent, bool for_side_panel=true);
	void Select(wxWindow *win);
	void Add(PaneId::type id, wxWindow *win);
	void Detach(wxWindow *win);
	wxWindow *GetCurrent();
	void OnPageChanged(wxAuiNotebookEvent &evt);
	DECLARE_EVENT_TABLE();
};

#endif


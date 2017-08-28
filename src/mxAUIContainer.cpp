#include "mxAUIContainer.h"
#include "mxMainWindow.h"
#include "mxAUI.h"

BEGIN_EVENT_TABLE(mxAUIContainer,wxAuiNotebook)
	EVT_AUINOTEBOOK_PAGE_CHANGED(mxID_LEFT_PANELS, mxAUIContainer::OnPageChanged)
END_EVENT_TABLE()

mxAUIContainer::mxAUIContainer (wxWindow * parent, bool for_side_panel) 
	: wxAuiNotebook(parent, wxID_ANY, wxDefaultPosition, wxSize(200,400), wxAUI_NB_BOTTOM | wxNO_BORDER),
	  m_for_side_panel(for_side_panel)
{
	
}

void mxAUIContainer::Select (wxWindow * win) {
	int i = GetPageIndex(win);
	if (i!=wxNOT_FOUND) 
		SetSelection(i);
}

void mxAUIContainer::Detach (wxWindow *win) {
	int i = GetPageIndex(win);
	if (i!=wxNOT_FOUND) 
		RemovePage(i);
}

void mxAUIContainer::Add (PaneId::type id, wxWindow * win) {
	win->Reparent(this);
	PaneConfig &cfg = PaneConfig::Get(id);
	AddPage(win,m_for_side_panel?cfg.GetVeryShortCaption():cfg.GetShortCaption(),true);
}


void mxAUIContainer::OnPageChanged (wxAuiNotebookEvent & evt) {
	int i = this->GetSelection();
	main_window->m_aui->GetPane(this).Caption(m_names[i]);
}



wxWindow * mxAUIContainer::GetCurrent ( ) {
	return GetPage(GetSelection());
}


#include <wx/bmpbuttn.h>
#include <wx/sashwin.h>
#include <wx/laywin.h>
#include <wx/treectrl.h>
#include <wx/html/htmprint.h>
#include <wx/settings.h>
#include "mxGenericHelpWindow.h"
#include "ids.h"
#include "mxMainWindow.h"
#include "mxUtils.h"
#include "ConfigManager.h"
#include "mxMessageDialog.h"
#include "mxSizers.h"
#include "mxArt.h"
#include "Language.h"

BEGIN_EVENT_TABLE(mxGenericHelpWindow,wxFrame)
	EVT_CLOSE(mxGenericHelpWindow::OnCloseEvent)
	EVT_BUTTON(mxID_HELPW_HIDETREE, mxGenericHelpWindow::OnHideTreeEvent)
	EVT_BUTTON(mxID_HELPW_HOME, mxGenericHelpWindow::OnHomeEvent)
	EVT_BUTTON(mxID_HELPW_PREV, mxGenericHelpWindow::OnPrevEvent)
	EVT_BUTTON(mxID_HELPW_NEXT, mxGenericHelpWindow::OnNextEvent)
	EVT_BUTTON(mxID_HELPW_COPY, mxGenericHelpWindow::OnCopyEvent)
	EVT_BUTTON(mxID_HELPW_ALWAYS_ON_TOP, mxGenericHelpWindow::OnAlwaysOnTop)
	EVT_BUTTON(mxID_HELPW_SEARCH, mxGenericHelpWindow::OnSearchEvent)
	EVT_BUTTON(mxID_HELPW_PRINT, mxGenericHelpWindow::OnPrintEvent)
	EVT_SASH_DRAGGED(wxID_ANY, mxGenericHelpWindow::OnSashDragEvent)
	EVT_TREE_SEL_CHANGED(wxID_ANY, mxGenericHelpWindow::OnTreeEvent)
	EVT_TREE_ITEM_ACTIVATED(wxID_ANY, mxGenericHelpWindow::OnTreeEvent)
	EVT_HTML_LINK_CLICKED(wxID_ANY, mxGenericHelpWindow::OnLinkEvent)
	EVT_CHAR_HOOK(mxGenericHelpWindow::OnCharHookEvent)
	EVT_SIZE(mxGenericHelpWindow::OnMaximize)
END_EVENT_TABLE();

mxGenericHelpWindow::mxGenericHelpWindow(wxString title, bool use_tree):wxFrame (nullptr,mxID_HELPW, title, wxDefaultPosition, wxSize(750,550),wxDEFAULT_FRAME_STYLE) {
	
#ifdef __WIN32__
	SetBackgroundColour(wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ));
#endif
	
	printer=nullptr;
	
	wxPanel *panel= new wxPanel(this);
	wxBoxSizer *sizer = general_sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);
	bottomSizer = new wxBoxSizer(wxHORIZONTAL);
		
	if (use_tree) {
		index_sash = new wxSashLayoutWindow(this, wxID_ANY,
			wxDefaultPosition, wxSize(200, 30),
			wxNO_BORDER | wxSW_3D | wxCLIP_CHILDREN);
		index_sash->SetDefaultSize(wxSize(120, 1000));
		index_sash->SetOrientation(wxLAYOUT_VERTICAL);
		index_sash->SetAlignment(wxLAYOUT_LEFT);
		index_sash->SetSashVisible(wxSASH_RIGHT, true);
		tree= new wxTreeCtrl(index_sash, wxID_ANY, wxPoint(0,0), wxSize(10,250),wxTR_DEFAULT_STYLE|wxTR_HIDE_ROOT);
		bottomSizer->Add(index_sash,sizers->Exp0);
		m_button_tree = new wxBitmapButton(panel, mxID_HELPW_HIDETREE, bitmaps->GetBitmap("dialogs/help_tree.png"));
		m_button_tree->SetToolTip(LANG(HELPW_TOGGLE_TREE,"Mostrar/Ocultar Indice"));
		topSizer->Add(m_button_tree,sizers->BA2);
	} else { tree=nullptr; m_button_tree=nullptr; }
	
	m_button_index = new wxBitmapButton(panel, mxID_HELPW_HOME, bitmaps->GetBitmap("dialogs/help_index.png"));
	m_button_index->SetToolTip(LANG(HELPW_INDEX,"Ir a la pagina de incio"));
	m_button_prev = new wxBitmapButton(panel, mxID_HELPW_PREV, bitmaps->GetBitmap("dialogs/help_prev.png"));
	m_button_prev->SetToolTip(LANG(HELPW_PREVIOUS,"Ir a la pagina anterior"));
	m_button_next = new wxBitmapButton(panel, mxID_HELPW_NEXT, bitmaps->GetBitmap("dialogs/help_next.png"));
	m_button_next->SetToolTip(LANG(HELPW_NEXT,"Ir a la pagina siguiente"));
	wxBitmapButton *button_copy = new wxBitmapButton(panel, mxID_HELPW_COPY, bitmaps->GetBitmap("dialogs/help_copy.png"));
	button_copy->SetToolTip(LANG(HELPW_COPY,"Copiar seleccion al portapapeles"));
	m_button_atop = new wxBitmapButton(panel, mxID_HELPW_ALWAYS_ON_TOP, bitmaps->GetBitmap("dialogs/help_atop.png"));
	m_button_atop->SetToolTip("Hacer que la ventana de ayuda permanezca siempre visible (siempre sobre las demás ventanas)");
	wxBitmapButton *button_print = new wxBitmapButton(panel, mxID_HELPW_PRINT, bitmaps->GetBitmap("dialogs/help_print.png"));
	button_print->SetToolTip(LANG(HELPW_PRINT,"Imprimir pagina actual"));
	topSizer->Add(m_button_index,sizers->BA2);
	topSizer->Add(m_button_prev,sizers->BA2);
	topSizer->Add(m_button_next,sizers->BA2);
	topSizer->Add(button_copy,sizers->BA2);
	topSizer->Add(m_button_atop,sizers->BA2);
	topSizer->Add(button_print,sizers->BA2);
	search_text = new wxTextCtrl(panel,wxID_ANY);
	search_text->SetToolTip(LANG(HELPW_SEARCH_LABEL,"Palabras a buscar"));
	topSizer->Add(search_text,sizers->BA2_Exp1);
	
	m_button_search = new wxBitmapButton(panel, mxID_HELPW_SEARCH, bitmaps->GetBitmap("dialogs/help_search.png"));
	m_button_search->SetToolTip(LANG(HELPW_FIND,"Buscar..."));
	topSizer->Add(m_button_search,sizers->BA2);
	panel->SetSizer(topSizer);
	html = new mxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxSize(200,100));
	bottomSizer->Add(html,sizers->Exp1);
	sizer->Add(panel,sizers->Exp0);
	sizer->Add(bottomSizer,sizers->Exp1);
	
	SetSizer(sizer);
	bottomSizer->SetItemMinSize(index_sash,250, 10);
	bottomSizer->Layout();
//	search_button->SetDefault();
}

void mxGenericHelpWindow::OnCloseEvent(wxCloseEvent &event) {
	Hide();
}

void mxGenericHelpWindow::OnHideTreeEvent(wxCommandEvent &event) {
	if (bottomSizer->GetItem(index_sash)->GetMinSize().GetWidth()<10)
		bottomSizer->SetItemMinSize(index_sash,200, 10);	
	else
		bottomSizer->SetItemMinSize(index_sash,0, 10);
	bottomSizer->Layout();
}

void mxGenericHelpWindow::OnHomeEvent(wxCommandEvent &event) {
	ShowIndex(); RepaintButtons();
}

void mxGenericHelpWindow::OnPrevEvent(wxCommandEvent &event) {
	OnPrev(); RepaintButtons();
}

void mxGenericHelpWindow::OnNextEvent(wxCommandEvent &event) {
	OnNext(); RepaintButtons();
}

void mxGenericHelpWindow::OnCopyEvent(wxCommandEvent &event) {
	if (html->SelectionToText()=="") return;
	mxUT::SetClipboardText(html->SelectionToText());
}

void mxGenericHelpWindow::OnSashDragEvent(wxSashEvent& event) {
	//index_sash->SetDefaultSize(wxSize(event.GetDragRect().width, 1000));
	bottomSizer->SetItemMinSize(index_sash,event.GetDragRect().width<150?150:event.GetDragRect().width, 10);
	//GetClientWindow()->Refresh();
	bottomSizer->Layout();
}

void mxGenericHelpWindow::OnTreeEvent(wxTreeEvent &event) {
	wxArrayTreeItemIds items;
	tree->GetSelections(items);
	if (items.GetCount()) OnTree(event.GetItem());
	RepaintButtons();
}

void mxGenericHelpWindow::OnLinkEvent(wxHtmlLinkEvent &event) {
	if (!OnLink(event.GetLinkInfo().GetHref())) event.Skip();
	RepaintButtons();
}

void mxGenericHelpWindow::OnCharHookEvent(wxKeyEvent &event) {
	if (event.GetKeyCode()==WXK_RETURN && search_text==FindFocus()) {
		wxCommandEvent e; OnSearchEvent(e);
	} else if (event.GetKeyCode()==WXK_ESCAPE)
		Close();
	else
		event.Skip();
}

void mxGenericHelpWindow::OnPrintEvent(wxCommandEvent &event) {
	if (!printer) {
		printer = new wxHtmlEasyPrinting("ZinjaI's help",this);
		int sizes[]={6,8,10,12,14,16,18};
		printer->SetFonts("","",sizes);
	}
	printer->PrintFile(html->GetOpenedPage());
}

void mxGenericHelpWindow::OnSearchEvent(wxCommandEvent & event) {
	OnSearch(search_text->GetValue());
	RepaintButtons();
}

void mxGenericHelpWindow::OnAlwaysOnTop (wxCommandEvent & evt) {
	if (GetWindowStyleFlag()&wxSTAY_ON_TOP)
		SetWindowStyleFlag(GetWindowStyleFlag()&(~wxSTAY_ON_TOP));
	else
		SetWindowStyleFlag(GetWindowStyleFlag()|wxSTAY_ON_TOP);
	RepaintButtons();
}

static void ButtonSetToggled(wxButton *button, bool value) {
	button->SetBackgroundColour(wxSystemSettings::GetColour(value?wxSYS_COLOUR_3DSHADOW:wxSYS_COLOUR_BTNFACE));
}

static void ButtonSetEnabled(wxButton *button, bool value) {
	button->Enable(value);
}

void mxGenericHelpWindow::RepaintButtons ( ) {
	if (m_button_tree) ButtonSetToggled(m_button_tree, bottomSizer->GetItem(index_sash)->GetMinSize().GetWidth()>10);
	ButtonSetToggled(m_button_atop, GetWindowStyleFlag()&wxSTAY_ON_TOP);
	ButtonSetToggled(m_button_index, CurrentPageIsHome());
	ButtonSetEnabled(m_button_prev, CanPrev());
	ButtonSetEnabled(m_button_next, CanNext());
	ButtonSetEnabled(m_button_search, !search_text->GetValue().IsEmpty());
}

void mxGenericHelpWindow::OnMaximize (wxSizeEvent & evt) {
	evt.Skip(); if (IsMaximized()) SetAlwaysOnTop(false);
}

void mxGenericHelpWindow::SetAlwaysOnTop (bool atop) {
	if ((GetWindowStyleFlag()&wxSTAY_ON_TOP)==atop) return;
	wxCommandEvent e2; OnAlwaysOnTop(e2);
}

void mxGenericHelpWindow::HideIndexTree () {
	bottomSizer->SetItemMinSize(index_sash,0, 10);
	bottomSizer->Layout();
}


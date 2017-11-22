#ifndef MXGENERICHELPWINDOW_H
#define MXGENERICHELPWINDOW_H

#include <wx/frame.h>
#include <wx/sashwin.h>
#include <wx/html/htmlwin.h>
#include "mxUtils.h" // HashStringTreeItem

class wxTextCtrl;
class wxTreeCtrl;
class wxSashLayoutWindow;
class wxBoxSizer;
class wxHtmlEasyPrinting;

class mxHtmlWindow :public wxHtmlWindow {
public:
	mxHtmlWindow(wxWindow *parent, wxWindowID id, wxPoint position, wxSize size) : wxHtmlWindow(parent,id,position,size) {}
	void ScrollToAnchor(const wxString &anchor) { 
		wxHtmlWindow::ScrollToAnchor(anchor); // why was this method private in wxHtmlWidgetCell??
		ScrollLines(1);
	}
};

class mxGenericHelpWindow:public wxFrame {
	DECLARE_EVENT_TABLE();
protected:
	wxBoxSizer *general_sizer;
	wxBoxSizer *bottomSizer;
	mxHtmlWindow *html;
	wxTextCtrl *search_text;
	wxTreeCtrl *tree;
	wxSashLayoutWindow *index_sash;
	wxHtmlEasyPrinting *printer;
	wxButton *m_button_tree, *m_button_atop, *m_button_index, *m_button_prev, *m_button_next, *m_button_search;
	
	void OnTreeEvent(wxTreeEvent &event);
	void OnLinkEvent(wxHtmlLinkEvent &event);
	void OnHideTreeEvent(wxCommandEvent &event);
	void OnCloseEvent(wxCloseEvent &event);
	void OnHomeEvent(wxCommandEvent &event);
	void OnPrevEvent(wxCommandEvent &event);
	void OnNextEvent(wxCommandEvent &event);
	void OnCopyEvent(wxCommandEvent &event);
	void OnPrintEvent(wxCommandEvent &event);
	void OnSearchEvent(wxCommandEvent &event);
	void OnSashDragEvent(wxSashEvent& event);
	void OnCharHookEvent(wxKeyEvent &event);
	void OnAlwaysOnTop(wxCommandEvent &evt);
	void OnMaximize(wxSizeEvent &evt);
	void RepaintButtons();
	
public:
	mxGenericHelpWindow(wxString title, bool use_tree);
	void SetAlwaysOnTop(bool atop=true);
	void HideIndexTree();
	virtual ~mxGenericHelpWindow() {}
	
	virtual void OnPrev() { html->HistoryBack(); }
	virtual void OnNext() { html->HistoryForward(); }
	virtual void ShowIndex()=0;
	virtual void OnSearch(wxString value){};
	virtual bool OnLink(wxString href){return true;};
	virtual void OnTree(wxTreeItemId item){};
	
	virtual bool CurrentPageIsHome() { return false; }
	virtual bool CanPrev() { return html->HistoryCanBack(); }
	virtual bool CanNext() { return html->HistoryCanForward(); }
	
};

#endif


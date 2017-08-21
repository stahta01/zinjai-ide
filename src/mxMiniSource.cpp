#include "mxMiniSource.h"
#include "mxSource.h"
#include "mxMiniMapPanel.h"
#include "mxColoursEditor.h"
#include "mxMainWindow.h"


BEGIN_EVENT_TABLE (mxMiniSource, wxStyledTextCtrl)
	EVT_LEFT_DOWN(mxMiniSource::OnClick)
	EVT_MIDDLE_DOWN(mxMiniSource::OnClick)
	EVT_RIGHT_DOWN(mxMiniSource::OnClick)
END_EVENT_TABLE()

mxMiniSource::mxMiniSource(mxMiniMapPanel *panel, mxSource *src) 
	: mxSourceBase(panel) 
{
	m_prev_first_line_on_scr = -1;
	SetUseHorizontalScrollBar(false);
	SetUseVerticalScrollBar(false);
	SetUseAntiAliasing(false);
	wxStyledTextCtrl::SetDocPointer(src->GetDocPointer());
	// el wxSTC_CARET_EVEN,wxSTC_VISIBLE_STRICT,0 es para que lo que pida que muestre en Refresh quede centrado
	wxStyledTextCtrl::SetVisiblePolicy(wxSTC_VISIBLE_STRICT,0);
	SetYCaretPolicy(wxSTC_CARET_EVEN|wxSTC_CARET_STRICT,0);
	SetEndAtLastLine(true);
	for(int i=0;i<16;i++) if (i!=mxSTC_MARK_USER) MarkerSetAlpha(i,0);
	for(int i=0;i<4;i++) SetMarginWidth (i,0);
	wxStyledTextCtrl::SetZoom( src->GetLineCount()>100 ? -10 : -9);
	mxMiniSource::SetStyle(src->lexer);
	Disable();
}

void mxMiniSource::Refresh (mxSource * src) {
	int first_line_on_scr = src->GetFirstVisibleLine();
	if (first_line_on_scr==m_prev_first_line_on_scr) return;
	m_prev_first_line_on_scr = first_line_on_scr;
	int total_lines = this->GetLineCount();
	int vis_lines_on_map = this->LinesOnScreen();
	int vis_lines_on_src = src->LinesOnScreen();
	int first_line_on_map = first_line_on_scr*(total_lines-vis_lines_on_map)/(total_lines-vis_lines_on_src);
	SetSelection(PositionFromLine(first_line_on_scr),PositionFromLine(first_line_on_scr+vis_lines_on_src));
	EnsureVisibleEnforcePolicy( first_line_on_map+vis_lines_on_map/2 );
}

void mxMiniSource::SetStyle (int lexer) {
	mxSourceBase::SetStyle(lexer);
	MarkerDefine(mxSTC_MARK_USER,wxSTC_MARK_BACKGROUND,g_ctheme->USER_LINE,g_ctheme->USER_LINE);
	float alph=2*35.0/256; float um_alph=1-alph;
	wxColour c_current_screen(
							  int(g_ctheme->CURRENT_LINE.Red()  *alph)+int(g_ctheme->DEFAULT_BACK.Red()  *um_alph),
							  int(g_ctheme->CURRENT_LINE.Green()*alph)+int(g_ctheme->DEFAULT_BACK.Green()*um_alph),
							  int(g_ctheme->CURRENT_LINE.Blue() *alph)+int(g_ctheme->DEFAULT_BACK.Blue() *um_alph) );
	SetSelBackground(true,c_current_screen);
}

void mxMiniSource::OnClick (wxMouseEvent & evt) {
	evt.Skip();
	main_window->GetCurrentSource()->GotoPos(PositionFromLine(GetCurrentLine()));
	main_window->GetCurrentSource()->EnsureCaretVisible();
	main_window->GetCurrentSource()->SetFocus();
}


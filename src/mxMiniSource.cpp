#include "mxMiniSource.h"
#include "mxSource.h"
#include "mxMiniMapPanel.h"
#include "mxColoursEditor.h"

mxMiniSource::mxMiniSource(mxMiniMapPanel *panel, mxSource *src) 
	: mxSourceBase(panel) 
{
	m_prev_first_line_on_scr = -1;
	SetUseHorizontalScrollBar(false);
	SetUseVerticalScrollBar(false);
	SetUseAntiAliasing(false);
	wxStyledTextCtrl::SetDocPointer(src->GetDocPointer());
	wxStyledTextCtrl::SetZoom(-100);
	// el wxSTC_CARET_EVEN,wxSTC_VISIBLE_STRICT,0 es para que lo que pida que muestre en Refresh quede centrado
	wxStyledTextCtrl::SetVisiblePolicy(wxSTC_VISIBLE_STRICT,0);
	SetYCaretPolicy(wxSTC_CARET_EVEN|wxSTC_CARET_STRICT,0);
	SetEndAtLastLine(true);
	mxSourceBase::SetStyle(src->lexer);
	SetSelBackground(true,g_ctheme->CURRENT_LINE);
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


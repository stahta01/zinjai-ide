#ifndef MXMINISOURCE_H
#define MXMINISOURCE_H
#include <wx/stc/stc.h>
#include "mxSourceBase.h"

class mxSource;
class mxMiniMapPanel;

class mxMiniSource : public mxSourceBase {
private:
	int m_prev_first_line_on_scr;
public:
	mxMiniSource(mxMiniMapPanel *panel, mxSource *src);
	void Refresh(mxSource *src);
	void SetStyle(int lexer);
	void OnClick(wxMouseEvent &evt);
	DECLARE_EVENT_TABLE();
};

#endif


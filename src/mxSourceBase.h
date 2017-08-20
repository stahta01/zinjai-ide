#ifndef MXSOURCEBASE_H
#define MXSOURCEBASE_H
#include <wx/stc/stc.h>

#define mxSOURCE_BOLD 1
#define mxSOURCE_ITALIC 2
#define mxSOURCE_UNDERL 4
#define mxSOURCE_HIDDEN 8

class mxSourceBase : public wxStyledTextCtrl {
private:
protected:
	void OnMouseWheel (wxMouseEvent & event);
public:
	mxSourceBase(wxWindow *parent);
private:
	///* auxiliar function for the public SetStyle
	void SetStyle(int idx, const wxChar *fontName, int fontSize, const wxColour &foreground, const wxColour &background, int fontStyle);
public:
	void SetStyle(int lexer);
	
};

#endif


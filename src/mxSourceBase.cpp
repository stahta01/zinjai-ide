#include "mxSourceBase.h"
#include "Cpp11.h"
#include "ConfigManager.h"
#include "mxColoursEditor.h"

mxSourceBase::mxSourceBase (wxWindow * parent) 
	: wxStyledTextCtrl (parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER|wxVSCROLL)
{
	wxStyledTextCtrl::Connect(wxEVT_MOUSEWHEEL,wxMouseEventHandler(mxSourceBase::OnMouseWheel),nullptr,this);
	wxFont font (config->Styles.font_size, wxMODERN, wxNORMAL, wxNORMAL);
	StyleSetFont (wxSTC_STYLE_DEFAULT, font);
	SetSelBackground(true,g_ctheme->SELBACKGROUND);
	StyleSetForeground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_FORE);
	StyleSetBackground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_BACK);
	StyleSetForeground (wxSTC_STYLE_LINENUMBER, g_ctheme->LINENUMBER_FORE);
	StyleSetBackground (wxSTC_STYLE_LINENUMBER, g_ctheme->LINENUMBER_BACK);
	StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, g_ctheme->INDENTGUIDE);
}

void mxSourceBase::OnMouseWheel (wxMouseEvent & event) {
	if (event.ControlDown()) {
		if (event.m_wheelRotation>0) {
			ZoomIn();
		} else {
			ZoomOut();
		}
	} else
		event.Skip();
}

void mxSourceBase::SetStyle(int idx, const wxChar *fontName, int fontSize, const wxColour &foreground, const wxColour &background, int fontStyle){
	StyleSetFontAttr(idx,fontSize,fontName,fontStyle&mxSOURCE_BOLD,fontStyle&mxSOURCE_ITALIC,fontStyle&mxSOURCE_UNDERL);
	StyleSetForeground (idx, foreground);
	StyleSetBackground (idx, background);
	StyleSetVisible (idx,!(fontStyle&mxSOURCE_HIDDEN));
}

#define AUXSetStyle(who,name) SetStyle(wxSTC_##who##_##name,config->Styles.font_name,config->Styles.font_size,g_ctheme->name##_FORE,g_ctheme->name##_BACK,(g_ctheme->name##_BOLD?mxSOURCE_BOLD:0)|(g_ctheme->name##_ITALIC?mxSOURCE_ITALIC:0)); // default
#define AUXSetStyle3(who,name,real) SetStyle(wxSTC_##who##_##name,config->Styles.font_name,config->Styles.font_size,g_ctheme->real##_FORE,g_ctheme->real##_BACK,(g_ctheme->real##_BOLD?mxSOURCE_BOLD:0)|(g_ctheme->real##_ITALIC?mxSOURCE_ITALIC:0)); // default
void mxSourceBase::SetStyle(int lexer) {
	SetLexer(lexer);
	if (lexer!=wxSTC_LEX_NULL) {
		AUXSetStyle(STYLE,BRACELIGHT); 
		AUXSetStyle(STYLE,BRACEBAD); 
		switch (lexer) {
		case wxSTC_LEX_CPP:
			AUXSetStyle(C,DEFAULT); // default
			AUXSetStyle(C,COMMENT); // comment
			AUXSetStyle(C,COMMENTLINE); // comment line
			AUXSetStyle(C,COMMENTDOC); // comment doc
			AUXSetStyle(C,NUMBER); // number
			AUXSetStyle(C,WORD); // keywords
			AUXSetStyle(C,STRING); // string
			AUXSetStyle(C,CHARACTER); // character
			AUXSetStyle3(C,UUID,DEFAULT); // uuid 
			AUXSetStyle(C,PREPROCESSOR); // preprocessor
			AUXSetStyle(C,OPERATOR); // operator 
			AUXSetStyle(C,IDENTIFIER); // identifier 
			AUXSetStyle(C,STRINGEOL); // string eol
			AUXSetStyle3(C,VERBATIM,STRINGEOL); // default verbatim
			AUXSetStyle3(C,REGEX,STRINGEOL); // regexp  
			AUXSetStyle(C,COMMENTLINEDOC); // special comment 
			AUXSetStyle(C,WORD2); // extra words
			AUXSetStyle(C,COMMENTDOCKEYWORD); // doxy keywords
			AUXSetStyle(C,COMMENTDOCKEYWORDERROR); // keywords errors
			AUXSetStyle(C,GLOBALCLASS); // keywords errors
			break;
		case wxSTC_LEX_HTML: case wxSTC_LEX_XML:
			SetProperty ("fold.html","0");
			SetProperty ("fold.html.preprocessor", "0");
			AUXSetStyle(H,DEFAULT); // keywords
			AUXSetStyle3(H,TAG,WORD); // keywords
			AUXSetStyle3(H,TAGUNKNOWN,WORD); // keywords
			AUXSetStyle3(H,ATTRIBUTE,COMMENTDOC); // comment doc
			AUXSetStyle3(H,ATTRIBUTEUNKNOWN,COMMENTDOC); // comment doc
			AUXSetStyle(H,COMMENT); // comment doc
			AUXSetStyle3(H,SINGLESTRING,STRING); // string
			AUXSetStyle3(H,DOUBLESTRING,STRING); // string
			AUXSetStyle(H,NUMBER); // number
			AUXSetStyle3(H,VALUE,PREPROCESSOR); // preprocessor
			AUXSetStyle3(H,SCRIPT,PREPROCESSOR); // preprocessor
			AUXSetStyle3(H,ENTITY,WORD2); // &nbsp;
			break;
		case wxSTC_LEX_MAKEFILE:
			AUXSetStyle(MAKE,DEFAULT); // default
			AUXSetStyle3(MAKE,IDENTIFIER,WORD); // keywords
			AUXSetStyle3(MAKE,COMMENT,COMMENT); // comment doc
			AUXSetStyle(MAKE,PREPROCESSOR); // preprocessor
			AUXSetStyle3(MAKE,OPERATOR,OPERATOR); // operator 
			AUXSetStyle3(MAKE,TARGET,OPERATOR); // string
			AUXSetStyle3(MAKE,IDEOL,NUMBER); // string
			break;
		case wxSTC_LEX_BASH:
			AUXSetStyle(SH,DEFAULT);
			AUXSetStyle3(SH,ERROR,COMMENTDOCKEYWORD);
			AUXSetStyle(SH,COMMENTLINE);
			AUXSetStyle(SH,NUMBER);
			AUXSetStyle(SH,WORD);
			AUXSetStyle(SH,STRING);
			AUXSetStyle3(SH,CHARACTER,STRING);
			AUXSetStyle(SH,OPERATOR);
			AUXSetStyle(SH,IDENTIFIER);
			AUXSetStyle3(SH,SCALAR,NUMBER);
			AUXSetStyle3(SH,PARAM,WORD2);
			AUXSetStyle3(SH,BACKTICKS,PREPROCESSOR);
			AUXSetStyle3(SH,HERE_DELIM,OPERATOR);
			AUXSetStyle3(SH,HERE_Q,OPERATOR);
			break;
		}
	} else {
		ClearDocumentStyle();
		SetLexer (wxSTC_LEX_NULL);
		StyleSetForeground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_FORE);
		StyleSetBackground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_BACK);
	}
}

#include <iostream>
#include <wx/encconv.h>
#include <wx/file.h>
#include <wx/utils.h>
#include "mxGCovSideBar.h"
#include "mxSource.h"
#include "mxUtils.h"
#include "ids.h"
#include "mxMainWindow.h"
#include "CodeHelper.h" 
#include "mxDropTarget.h" 
#include "mxMessageDialog.h"
#include "ProjectManager.h"
#include "DebugManager.h"
#include "mxArt.h" 
#include "mxBreakOptions.h"
#include "Parser.h"
#include "mxGotoFunctionDialog.h"
#include "mxApplication.h"
#include "Language.h"
#include "mxStatusBar.h"
#include "mxCompiler.h"
#include "Autocoder.h"
#include "mxColoursEditor.h"
#include "error_recovery.h"
#include "mxCalltip.h"
#include "MenusAndToolsConfig.h"
#include "mxInspectionExplorerDialog.h"
#include "mxInspectionBaloon.h"
#include "LocalRefactory.h"
#include "mxSourceParsingAux.h"
//#include "linStuff.h"
#include "mxMiniSource.h"
using namespace std;

// dwell time for margins
#define _DWEEL_TIME_ 100
// dwell time for code = _DWEEL_TIME_*_DWEEL_FACTOR_
#define _DWEEL_FACTOR_ 15


NavigationHistory g_navigation_history;

NavigationHistory::MaskGuard::MaskGuard() {
	g_navigation_history.masked=true;
}

NavigationHistory::MaskGuard::~MaskGuard() {
	g_navigation_history.masked=false;
}

void NavigationHistory::MaskGuard::UnmaskNow() {
	g_navigation_history.masked=false;
}

void NavigationHistory::OnClose(mxSource *src) {
	for(int i=0;i<hsize;i++) { 
		int j=(hbase+i)%MAX_NAVIGATION_HISTORY_LEN;
		if (locs[j].src==src) {
			if (src->sin_titulo) locs[j].file.Clear();
			else locs[j].file=src->GetFullPath(); 
			locs[j].src=nullptr;
		}
	}
	if (focus_source==src) focus_source=nullptr;
}

void NavigationHistory::Goto(int i) {
	if (masked) return;
	jumping=true;
	Location &loc=locs[i%MAX_NAVIGATION_HISTORY_LEN];
	if (!loc.src) {
		if (!loc.file.Len()) return;
		loc.src=main_window->OpenFile(loc.file);
	} else if (focus_source!=loc.src) {
		for (int i=0,j=main_window->notebook_sources->GetPageCount();i<j;i++)
			if (main_window->notebook_sources->GetPage(i)==loc.src) {
				main_window->notebook_sources->SetSelection(i);
				break;
			}
	}
	if (loc.src) {
		loc.src->GotoPos(loc.src->GetLineIndentPosition(loc.line));
		loc.src->SetFocus();
	}
	jumping=false;
}


void NavigationHistory::OnFocus(mxSource *src) {
	if (src==focus_source) return;
	focus_source=src; Add(src,src->GetCurrentLine());
}

void NavigationHistory::OnJump(mxSource *src, int current_line) {
	const int min_long_jump_len=10;
	if (!jumping) {
		if (
			current_line>=src->old_current_line+min_long_jump_len
			||
			current_line<=src->old_current_line-min_long_jump_len
		) {
			Add(src,current_line);
		} else {
			Location &old_loc=locs[(hbase+hcur)%MAX_NAVIGATION_HISTORY_LEN];
			old_loc.line=current_line;
		}
	}
	src->old_current_line=current_line;
}

void NavigationHistory::Add(mxSource *src, int line) {
	if (masked) return;
	Location &old_loc=locs[(hbase+hcur)%MAX_NAVIGATION_HISTORY_LEN];
	if (old_loc.src==src&&old_loc.line==line) return;
	if (hsize<MAX_NAVIGATION_HISTORY_LEN) hsize=(++hcur)+1;
	else hbase=(hbase+1)%MAX_NAVIGATION_HISTORY_LEN;
	Location &new_loc=locs[(hbase+hcur)%MAX_NAVIGATION_HISTORY_LEN];
	new_loc.src=src; new_loc.line=line;
}

void NavigationHistory::Prev() {
	if (hcur==0) return;
	Goto(hbase+(--hcur));
}

void NavigationHistory::Next() {
	if (hcur+1==hsize) return; // no hay historial para adelante
	Goto(hbase+(++hcur));
}

#define II_BACK(p,a) while(p>0 && (a)) p--;
#define II_BACK_NC(p,a) while(a) p--;
#define II_FRONT(p,a) while(p<l && (a)) p++;
#define II_FRONT_NC(p,a) while(a) p++;
#define II_IS_2(p,c1,c2) ((c=GetCharAt(p))==c1 || c==c2)
#define II_IS_3(p,c1,c2,c3) ((c=GetCharAt(p))==c1 || c==c2 || c==c3)
#define II_IS_4(p,c1,c2,c3,c4) ((c=GetCharAt(p))==c1 || c==c2 || c==c3 || c==c4)
#define II_IS_5(p,c1,c2,c3,c4,c5) ((c=GetCharAt(p))==c1 || c==c2 || c==c3 || c==c4 || c==c5)
#define II_IS_6(p,c1,c2,c3,c4,c5,c6) ((c=GetCharAt(p))==c1 || c==c2 || c==c3 || c==c4 || c==c5 || c==c6)
#define II_IS_COMMENT(p) ((s=GetStyleAt(p))==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR)
#define II_SHOULD_IGNORE(p) ((s=GetStyleAt(p))==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_CHARACTER || s==wxSTC_C_STRING || s==wxSTC_C_STRINGEOL || s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_PREPROCESSOR || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR)
#define II_IS_NOTHING_4(p) (II_IS_4(p,' ','\t','\r','\n') || II_SHOULD_IGNORE(p))
#define II_IS_NOTHING_2(p) (II_IS_2(p,' ','\t') || II_SHOULD_IGNORE(p)) 
#define II_IS_KEYWORD_CHAR(c) ( ( (c|32)>='a' && (c|32)<='z' ) || (c>='0' && c<='9') || c=='_' )

#define STYLE_IS_CONSTANT(s) (s==wxSTC_C_STRING || s==wxSTC_C_STRINGEOL || s==wxSTC_C_CHARACTER || s==wxSTC_C_REGEX || s==wxSTC_C_NUMBER)
#define STYLE_IS_COMMENT(s) (s==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR)

static const wxChar* s_reserved_keywords =
	"and asm auto break case catch class const const_cast "
	"continue default delete do dynamic_cast else enum explicit "
	"export extern false for friend if goto inline "
	"mutable namespace new not operator or private protected public "
	"reinterpret_cast return sizeof static_cast "
	"struct switch template this throw true try typedef typeid "
	"typename union using virtual while xor "
	"auto constexpr decltype static_assert final override noexcept nullptr"; // c++ 2011
static const wxChar* s_types_keywords =
	"bool char const double float int long mutable register "
	"short signed static unsigned void volatile wchar_t";
static const wxChar* s_doxygen_keywords =
	"a addindex addtogroup anchor arg attention author b brief bug c "
	"class code date def defgroup deprecated dontinclude e em endcode "
	"endhtmlonly endif endlatexonly endlink endverbatim enum example "
	"exception f$ f[ f] file fn hideinitializer htmlinclude "
	"htmlonly if image include ingroup internal invariant interface "
	"latexonly li line link mainpage n name namespace nosubgrouping note "
	"overload p page par param post pre ref relates remarks return "
	"retval sa section see showinitializer since skip skipline struct "
	"subsection test throw todo tparam typedef union until var verbatim "
	"verbinclude version warning weakgroup $ @ \"\" & < > # { }";


enum Margins { MARGIN_LINENUM=0, MARGIN_BREAKS, MARGIN_FOLD, MARGIN_NULL };

BEGIN_EVENT_TABLE (mxSource, wxStyledTextCtrl)
	// edit
//	EVT_MENU (mxID_EDIT_CLEAR, mxSource::OnEditClear)
	EVT_MENU (mxID_EDIT_CUT, mxSource::OnEditCut)
	EVT_MENU (mxID_EDIT_COPY, mxSource::OnEditCopy)
	EVT_MENU (mxID_EDIT_PASTE, mxSource::OnEditPaste)
	EVT_MENU (mxID_EDIT_SELECT_ALL, mxSource::OnEditSelectAll)
	EVT_MENU (mxID_EDIT_REDO, mxSource::OnEditRedo)
	EVT_MENU (mxID_EDIT_UNDO, mxSource::OnEditUndo)
	EVT_MENU (mxID_EDIT_DUPLICATE_LINES, mxSource::OnEditDuplicateLines)
	EVT_MENU (mxID_EDIT_DELETE_LINES, mxSource::OnEditDeleteLines)
	EVT_MENU (mxID_EDIT_MARK_LINES, mxSource::OnEditMarkLines)
	EVT_MENU (mxID_EDIT_GOTO_MARK, mxSource::OnEditGotoMark)
	EVT_MENU (mxID_EDIT_FORCE_AUTOCOMPLETE, mxSource::OnEditForceAutoComplete)
	EVT_MENU (mxID_EDIT_HIGHLIGHTED_WORD_EDITION, mxSource::OnEditHighLightedWordEdition)
	EVT_MENU (mxID_EDIT_RECTANGULAR_EDITION, mxSource::OnEditRectangularEdition)
	EVT_MENU (mxID_EDIT_AUTOCODE_AUTOCOMPLETE, mxSource::OnEditAutoCompleteAutocode)
	EVT_MENU (mxID_EDIT_TOGGLE_LINES_UP, mxSource::OnEditToggleLinesUp)
	EVT_MENU (mxID_EDIT_TOGGLE_LINES_DOWN, mxSource::OnEditToggleLinesDown)
	EVT_MENU (mxID_EDIT_COMMENT, mxSource::OnComment)
	EVT_MENU (mxID_EDIT_UNCOMMENT, mxSource::OnUncomment)
	EVT_MENU (mxID_EDIT_BRACEMATCH, mxSource::OnBraceMatch)
	EVT_MENU (mxID_EDIT_INDENT, mxSource::OnIndentSelection)
	EVT_MENU (mxID_EDIT_HIGHLIGHT_WORD, mxSource::OnHighLightWord)
	EVT_MENU (mxID_EDIT_FIND_KEYWORD, mxSource::OnFindKeyword)
	EVT_MENU (mxID_EDIT_MAKE_LOWERCASE, mxSource::OnEditMakeLowerCase)
	EVT_MENU (mxID_EDIT_MAKE_UPPERCASE, mxSource::OnEditMakeUpperCase)
	// view
	EVT_MENU (mxID_FOLDTOGGLE, mxSource::OnFoldToggle)
	EVT_SET_FOCUS (mxSource::OnSetFocus)
	EVT_KILL_FOCUS (mxSource::OnKillFocus)
	EVT_STC_MARGINCLICK (wxID_ANY, mxSource::OnMarginClick)
	// Stc
	EVT_STC_CHARADDED (wxID_ANY, mxSource::OnCharAdded)
	EVT_STC_UPDATEUI (wxID_ANY, mxSource::OnUpdateUI)
	EVT_STC_DWELLSTART (wxID_ANY, mxSource::OnToolTipTime)
	EVT_STC_DWELLEND (wxID_ANY, mxSource::OnToolTipTimeOut)
	EVT_STC_SAVEPOINTREACHED(wxID_ANY, mxSource::OnSavePointReached)
	EVT_STC_SAVEPOINTLEFT(wxID_ANY, mxSource::OnSavePointLeft)
	EVT_LEFT_DOWN(mxSource::OnClick)
	EVT_LEFT_UP(mxSource::OnClickUp)
	EVT_RIGHT_DOWN(mxSource::OnPopupMenu)
	EVT_STC_ROMODIFYATTEMPT (wxID_ANY, mxSource::OnModifyOnRO)
	EVT_STC_DOUBLECLICK (wxID_ANY, mxSource::OnDoubleClick)
	
	EVT_STC_PAINTED(wxID_ANY, mxSource::OnPainted)
	
	EVT_KEY_DOWN(mxSource::OnKeyDown)
	EVT_STC_MACRORECORD(wxID_ANY,mxSource::OnMacroAction)
	EVT_STC_AUTOCOMP_SELECTION(wxID_ANY,mxSource::OnAutocompSelection)
	
	EVT_TIMER(wxID_ANY,mxSource::OnTimer)
//	EVT_MOUSEWHEEL(mxSource::OnMouseWheel)
END_EVENT_TABLE()

mxSource::mxSource (wxWindow *parent, wxString ptext, project_file_item *fitem) 
	: mxSourceBase (parent), focus_helper(this), autocomp_helper(this)
{
	
//#ifdef __linux__
//	if (MyLocale::IsUTF8()) SetCodePage(wxSTC_CP_UTF8);
//#endif

//	AutoCompSetDropRestOfWord(true); // esto se torna muy molesto en muchos casos (por ejemplo, intentar agregar unsigned antes de int), mejor no usar
	
	calltip = nullptr;	calltip_mode = MXS_NULL; inspection_baloon = nullptr;
	
	old_current_line=-1000;
	
	brace_1=-1; brace_2=-1;
	
	next_source_with_same_file=this;
	
	source_time_dont_ask = false; source_time_reload = false;
	diff_brother=nullptr; first_diff_info=last_diff_info=nullptr;
	
	readonly_mode = ROM_NONE;
	
	LoadSourceConfig();
	
	if (fitem) {
		m_extras = &fitem->GetSourceExtras();
//		m_extras->ToSource(this); // not here, mxSource is still empty!!!
		m_owns_extras=false;
		treeId=fitem->GetTreeItem();
		if (fitem->IsReadOnly()) SetReadOnlyMode(ROM_ADD_PROJECT);
	} else {
		m_owns_extras=true;
		m_extras = new SourceExtras();
	}
	
	ignore_char_added=false;
	
	page_text = ptext;
	
	RegisterImage(2,*(bitmaps->parser.icon02_define));
	RegisterImage(3,*(bitmaps->parser.icon03_func));
	RegisterImage(4,*(bitmaps->parser.icon04_class));
	RegisterImage(5,*(bitmaps->parser.icon05_att_unk));
	RegisterImage(6,*(bitmaps->parser.icon06_att_pri));
	RegisterImage(7,*(bitmaps->parser.icon07_att_pro));
	RegisterImage(8,*(bitmaps->parser.icon08_att_pub));
	RegisterImage(9,*(bitmaps->parser.icon09_mem_unk));
	RegisterImage(10,*(bitmaps->parser.icon10_mem_pri));
	RegisterImage(11,*(bitmaps->parser.icon11_mem_pro));
	RegisterImage(12,*(bitmaps->parser.icon12_mem_pub));
	RegisterImage(13,*(bitmaps->parser.icon13_none));
	RegisterImage(14,*(bitmaps->parser.icon14_global_var));
	RegisterImage(15,*(bitmaps->parser.icon15_res_word));
	RegisterImage(16,*(bitmaps->parser.icon16_preproc));
	RegisterImage(17,*(bitmaps->parser.icon17_doxygen));
	RegisterImage(18,*(bitmaps->parser.icon18_typedef));
	RegisterImage(19,*(bitmaps->parser.icon19_enum_const));
	RegisterImage(20,*(bitmaps->parser.icon20_argument));
	RegisterImage(21,*(bitmaps->parser.icon21_local));
	
	lexer = wxSTC_LEX_CPP;
	
	config_running = config->Running;
	
	wxString fname = DIR_PLUS_FILE(config->temp_dir,"sin_titulo.cpp");
	source_filename = wxEmptyString;
	temp_filename = fname;
	m_cem_ref.SetFName(fname);
	binary_filename = DIR_PLUS_FILE(config->temp_dir,"sin_titulo")+_T(BINARY_EXTENSION);
	working_folder = wxFileName::GetHomeDir();

	sin_titulo = true;
	cpp_or_just_c = true;
	never_parsed = true;
	first_view = true;
//	current_line = 0; current_marker = -1;
	
//	SetViewEOL (false); // no mostrar fin de lineas
//	SetIndentationGuides (true); 
//	SetViewWhiteSpace(config_source.whiteSpace?wxSTC_WS_VISIBLEALWAYS:wxSTC_WS_INVISIBLE);
//	SetViewWhiteSpace (wxSTC_WS_INVISIBLE);
	SetOvertype (config_source.overType);
	config_source.wrapMode = config->Init.wrap_mode==2||(config->Init.wrap_mode==1&&!config_source.syntaxEnable);
	SetWrapMode (config_source.wrapMode?wxSTC_WRAP_WORD: wxSTC_WRAP_NONE);
	SetWrapVisualFlags(wxSTC_WRAPVISUALFLAG_END|wxSTC_WRAPVISUALFLAG_START);
	
	SetKeyWords (0, s_reserved_keywords);
	SetKeyWords (1, s_types_keywords);
	SetKeyWords (2, s_doxygen_keywords);

	//SetCaretLineBackground("Z LIGHT BLUE");
	//SetCaretLineVisible(true);

	// numeros de linea
//	SetMarginWidth (MARGIN_LINENUM, TextWidth (wxSTC_STYLE_LINENUMBER, " XXX"));

	// set margin as unused
	SetMarginType (MARGIN_BREAKS, wxSTC_MARGIN_SYMBOL);
	SetMarginWidth (MARGIN_BREAKS, 16);
	SetMarginSensitive (MARGIN_BREAKS, true);

	// folding
	SetMarginType (MARGIN_FOLD, wxSTC_MARGIN_SYMBOL);
	SetMarginMask (MARGIN_FOLD, wxSTC_MASK_FOLDERS);
	SetMarginWidth (MARGIN_FOLD, 0);
	SetMarginSensitive (MARGIN_FOLD, false);
	// folding enable
	if (config_source.foldEnable) {
		SetMarginWidth (MARGIN_FOLD, true? 15: 0);
		SetMarginSensitive (MARGIN_FOLD, 1);
		SetProperty("fold", true?"1":"0");
		SetProperty("fold.comment",true?"1":"0");
		SetProperty("fold.compact",true?"1":"0");
		SetProperty("fold.preprocessor",true?"1":"0");
		SetProperty("fold.html.preprocessor",true?"1":"0");
		SetFoldFlags( wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED | wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED );
	}

	// set visibility
	SetVisiblePolicy (wxSTC_VISIBLE_STRICT|wxSTC_VISIBLE_SLOP, 1);
	SetXCaretPolicy (wxSTC_CARET_EVEN|wxSTC_VISIBLE_STRICT|wxSTC_CARET_SLOP, 1);
	SetYCaretPolicy (wxSTC_CARET_EVEN|wxSTC_VISIBLE_STRICT|wxSTC_CARET_SLOP, 1);
//	SetVisiblePolicy(wxSTC_CARET_SLOP,50);
//	SetXCaretPolicy(wxSTC_CARET_SLOP,50);
//	SetYCaretPolicy(wxSTC_CARET_SLOP,50);
		
	SetEndAtLastLine(!config->Init.autohide_panels);

	AutoCompSetSeparator('\n');
	AutoCompSetIgnoreCase(true);
	AutoCompSetAutoHide(false);
	AutoCompSetTypeSeparator('$');
	
	IndicatorSetStyle(0,wxSTC_INDIC_SQUIGGLE);
	IndicatorSetStyle(1,wxSTC_INDIC_SQUIGGLE);
	IndicatorSetForeground (0, 0x0000ff);
	IndicatorSetForeground (1, 0x550055);
	
	// lineas que no se compilan
	IndicatorSetStyle(2,wxSTC_INDIC_STRIKE);
	IndicatorSetForeground (2, 0x005555);

	if (config_source.syntaxEnable) {
		lexer=wxSTC_LEX_CPP;
		SetStyle(true);
	}
	
	SetColours(false);
	
	SetLayoutCache (wxSTC_CACHE_PAGE);
	
	SetDropTarget(new mxDropTarget(this));
	UsePopUp(false);
#ifndef __WIN32__
	// el default para BufferedDraw es true, pero por alguna raz�n en algun momento
	// lo puse en false, tal vez por velocidad o problemas de refresco
	// en gtk parece no cambiar nada, pero en windows genera un flickering molesto
	// (para verlo solo basta pararse al comienzo de una linea con codigo y esperar)
	SetBufferedDraw(false); 
#endif
//	SetTwoPhaseDraw (false);
	SetMouseDwellTime(_DWEEL_TIME_); // mis tooltips bizarros (con showbaloon = calltip)
	
	if (debug->IsDebugging() && !config->Debug.allow_edition) SetReadOnlyMode(ROM_ADD_DEBUG);

	er_register_source(this);
	
}


mxSource::~mxSource () {
	HideCalltip(); if (calltip) { calltip->Destroy(); calltip=nullptr; }
	
	bool only_view=next_source_with_same_file==this; // there can be more than one view of the same source
	
	if (diff_brother) diff_brother->SetDiffBrother(nullptr); 
	diff_brother=nullptr;
	while (first_diff_info) delete first_diff_info;
	
	g_navigation_history.OnClose(this);
	parser->UnregisterSource(this);
	debug->UnregisterSource(this);
	if (main_window) main_window->UnregisterSource(this);
	er_unregister_source(this);
	if (main_window) {
		if (compiler->last_compiled==this) compiler->last_compiled=nullptr;
		if (compiler->last_runned==this) compiler->last_runned=nullptr;
	}
	
	if (!only_view) {
		mxSource *iter=this;
		while (iter->next_source_with_same_file!=this)
			iter=iter->next_source_with_same_file;
		iter->next_source_with_same_file=next_source_with_same_file;
		m_extras->ChangeSource(next_source_with_same_file);
		if (m_owns_extras) next_source_with_same_file->m_owns_extras=true;
	} else {
		// si no es un fuente de un proyecto, tiene la resposabilidad de liberar la memoria de los breakpoints
		m_extras->ChangeSource(nullptr);
		if (m_owns_extras) delete m_extras; /// @todo: esto puede traer problemas en combinacion con el split
	}
	
	
	
}

void mxSource::OnEditDeleteLines (wxCommandEvent &event) {
	int ss = GetSelectionStart();
	int min,max; GetSelectedLinesRange(min,max);
	if (max==min) {
		LineDelete();
		if (LineFromPosition(ss)!=min)
			GotoPos(GetLineEndPosition(min));
		else
			GotoPos(ss);
	} else {
		GotoPos(ss);
		UndoActionGuard undo_action(this);
		for (int i=min;i<=max;i++)
			LineDelete();
		if (LineFromPosition(ss)!=min) 
			GotoPos(GetLineEndPosition(min));
		else 
			GotoPos(ss);
	}
}

void mxSource::OnEditGotoMark (wxCommandEvent &event) {
	int cl=GetCurrentLine(), lc=GetLineCount();
	int i=cl+1;
	while (!(MarkerGet(i)&(1<<mxSTC_MARK_USER)) && i<lc)
		i++;
	if (i<lc) {
		GotoPos(GetLineIndentPosition(i));
		EnsureVisibleEnforcePolicy(GetCurrentLine());
		return;
	}
	i=0;
	while (!(MarkerGet(i)&(1<<mxSTC_MARK_USER)) && i<cl)
		i++;
	if (i<cl) {
		GotoPos(GetLineIndentPosition(i));
		EnsureVisibleEnforcePolicy(GetCurrentLine());
		return;
	}
}

void mxSource::OnEditMarkLines (wxCommandEvent &event) {
	int min,max; GetSelectedLinesRange(min,max);
	if (max==min) {
		if (MarkerGet(min)&(1<<mxSTC_MARK_USER))
			MarkerDelete(min,mxSTC_MARK_USER);
		else
			MarkerAdd(min,mxSTC_MARK_USER);
	} else {
			int mark=true;
			for (int i=min;i<=max;i++)
				if (MarkerGet(i)&(1<<mxSTC_MARK_USER)) {
					mark=false;
					break;
				}
			for (int i=min;i<=max;i++)
				if (mark)
					MarkerAdd(i,mxSTC_MARK_USER);
				else
					MarkerDelete(i,mxSTC_MARK_USER);
	}
	Refresh();
}

void mxSource::OnEditDuplicateLines (wxCommandEvent &event) {
	int ss = GetSelectionStart(), se = GetSelectionEnd();
	int min,max; GetSelectedLinesRange(min,max);
	UndoActionGuard undo_action(this);
	if (max==min) {
		LineDuplicate();
	} else {
		wxString text;
		for (int i=min;i<=max;i++)
			text+=GetLine(i);
		InsertText(PositionFromLine(max+1),text);
		SetSelection(ss,se);
	}
}

void mxSource::OnEditRedo (wxCommandEvent &event) {
    if (!CanRedo()) return;
    Redo ();
	EnsureVisibleEnforcePolicy(GetCurrentLine());
}

void mxSource::OnEditUndo (wxCommandEvent &event) {
    if (!CanUndo()) return;
    Undo ();
	EnsureVisibleEnforcePolicy(GetCurrentLine());
}

//void mxSource::OnEditClear (wxCommandEvent &event) {
//    if (GetReadOnly()) return;
//    Clear ();
// }

void mxSource::OnEditCut (wxCommandEvent &event) {
//    if (GetReadOnly() || (GetSelectionEnd()-GetSelectionStart() <= 0)) return;
//	int ss = GetSelectionStart(), se = GetSelectionEnd();
//	if (se==ss) return;
//	if (se<ss) { int aux=ss; ss=se; se=aux; }
//	wxTextDataObject data;
//	if (wxTheClipboard->Open()) {
//		wxTheClipboard->SetData( new wxTextDataObject(GetTextRange(ss,se)) );
//		wxTheClipboard->Close();
//	}
//	ReplaceSelection("");
	Cut();
}

void mxSource::OnEditCopy (wxCommandEvent &event) {
//	int ss = GetSelectionStart(), se = GetSelectionEnd();
//    if (se==ss) return;
//	if (se<ss) { int aux=ss; ss=se; se=aux; }
//	wxTextDataObject data;
//	if (wxTheClipboard->Open()) {
//		wxTheClipboard->SetData( new wxTextDataObject(GetTextRange(ss,se)) );
//		wxTheClipboard->Close();
//	}
    Copy ();
}

void mxSource::OnEditPaste (wxCommandEvent &event) {
	if (config_source.indentPaste && config_source.syntaxEnable) {
		wxString str = mxUT::GetClipboardText();
		if (str.IsEmpty()) return;
		UndoActionGuard undo_action(this);
		// borrar la seleccion previa
		if (GetSelectionEnd()-GetSelectionStart()!=0)
			DeleteBack();
		// insertar el nuevo texto
		int cp = GetCurrentPos();
		InsertText(cp,str);
		ignore_char_added=false;
		// indentar el nuevo texto
		Colourise(cp,cp+str.Len());
		int l1 = LineFromPosition(cp);
		int l2 = LineFromPosition(cp+str.Len());
		int pos = PositionFromLine(l1);
		char c;
		int p=cp-1;
		while (p>=pos && II_IS_2(p,' ','\t'))
			p--;
		if (p!=pos && p>=pos) 
			l1++;
//		p=cp+data.GetTextLength();
//		if ((unsigned int)PositionFromLine(l2)==cp+str.Len())
//			l2--;
		//			pos=GetLineEndPosition(l2);
		//			while (p<=pos && II_IS_2(p,' ','\t'))
		//				p++;
		//			if (p<=pos && c!='\n' && c!='\r')
		//				l2--;
		// indentar y acomodar el cursor
		cp+=str.Len();
		if (l2>=l1) {
			c=LineFromPosition(cp);
			pos=cp-GetLineIndentPosition(c);
			if (pos<0) pos=0;
			Indent(l1,l2);
			pos+=GetLineIndentPosition(c);
			SetSelection(pos,pos);
		} else
			SetSelection(cp,cp);
		// is that the problem?
		HideCalltip();
	} else {
		if (CanPaste()) Paste();
	}
}

bool mxSource::GetCurrentScopeLimits(int pos, int &pmin, int &pmax, bool only_curly_braces) {
	pmin=pos;
	char c=GetCharAt(pmin);
	if (c!='{' && c!='}' && (only_curly_braces || (c!='[' && c!=']' && c!='(' && c!=')' ) ) ) {
		pmin--;  c=GetCharAt(pmin);
	}
	if (c!='{' && c!='}' && (only_curly_braces || (c!='(' && c!=')' &&  c!='[' && c!=']') ) ) {
		int omin=pmin-1;
		while (pmin>=0 && (c=GetCharAt(pmin))!='{' && (only_curly_braces || (c!='('&& c!='[') ) ) {
			if ( (c=='}' || (!only_curly_braces && (c==')' || c==']') ) ) && omin!=pmin )
				if ((pmin=BraceMatch(pmin))==wxSTC_INVALID_POSITION)
					return false;
		pmin--;
		}
		if (pmin<0) return false;
	}
	pmax = BraceMatch (pmin);
	return pmax!=wxSTC_INVALID_POSITION;
}

void mxSource::OnBraceMatch (wxCommandEvent &event) {
	int pmin,pmax; 
	if (GetCurrentScopeLimits(GetCurrentPos(),pmin,pmax)) {
//		BraceHighlight (pmin+1, pmax);
		if (pmax > pmin) 
			SetSelection (pmin, pmax+1);
		else
			SetSelection (pmin+1, pmax);
	} else {
		if (pmin!=wxSTC_INVALID_POSITION) BraceBadLight (pmin);
	}
}

void mxSource::OnFoldToggle (wxCommandEvent &event) {
    ToggleFold (GetFoldParent(GetCurrentLine()));
}

void mxSource::OnMarginClick (wxStyledTextEvent &event) {
	
    if (event.GetMargin() == MARGIN_FOLD) { // margen del folding
        int lineClick = LineFromPosition (event.GetPosition());
        int levelClick = GetFoldLevel (lineClick);
        if ((levelClick & wxSTC_FOLDLEVELHEADERFLAG) > 0) {
            ToggleFold (lineClick);
        }
		
	} else { // margen de los puntos de interrupcion
		
		// buscar si habia un breakpoint en esa linea
		int l = LineFromPosition (event.GetPosition());
		BreakPointInfo *bpi=m_extras->FindBreakpointFromLine(this,l);
		
		// si apret� shift o ctrl (por alguna razon en linux solo me anda shift) mostrar el cuadro de opciones
		if (event.GetModifiers()&wxSTC_SCMOD_SHIFT || event.GetModifiers()&wxSTC_SCMOD_CTRL) {
			if (!debug->IsDebugging() || debug->CanTalkToGDB()) {
				if (!bpi) { // si no habia, lo crea
					bpi=new BreakPointInfo(this,l);
					if (debug->IsDebugging()) debug->SetBreakPoint(bpi);
					else bpi->SetStatus(BPS_UNKNOWN);
				}
				new mxBreakOptions(bpi); // muestra el dialogo de opciones del bp
			}
		
		// si hay que sacar el breakpoint
		} else if (bpi) { 
			if (debug->IsDebugging()) debug->DeleteBreakPoint(bpi); // debug se encarga de hacer el delete y eso se lleva el marker en el destructor
			else delete bpi;
		
		// si hay que poner un breakpoint nuevo
		} else { 
			if (IsEmptyLine(l)) { // no dejar poner un pto de interrupci�n en una l�nea que no tiene c�digo
				mxMessageDialog(main_window,LANG(DEBUG_NO_BREAKPOINT_ON_EMPTY_LINE,""
												 "Los puntos de interrupcion no pueden colocarse en lineas vacias\n"
												 " que contengan solo comentarios o directivas de preprocesador"))
					.Title(LANG(DEBUG_BREAKPOINT_CAPTION,"Puntos de Interrupcion")).IconInfo().Run();
				return;
			}
			bpi=new BreakPointInfo(this,l);
			if (debug->IsDebugging()) debug->LiveSetBreakPoint(bpi); // esta llamada cambia el estado del bpi y eso pone la marca en el margen
			else bpi->SetStatus(BPS_SETTED);
		}
		
	}
}

//void mxSource::OnEditSelectLine (wxCommandEvent &event) {
//	int lineStart = PositionFromLine (GetCurrentLine());
//	int lineEnd = PositionFromLine (GetCurrentLine() + 1);
//	SetSelection (lineStart, lineEnd);
// }

void mxSource::OnEditSelectAll (wxCommandEvent &event) {
	SetSelection (0, GetTextLength ());
}

struct auxMarkersConserver {
	struct amc_aux {
		bool um; // user mark
		BreakPointInfo *bpi;
		void Get(mxSource *s, int l) { 
			um = s->MarkerGet(l)&(1<<mxSTC_MARK_USER);
			if (um) s->MarkerDelete(l,mxSTC_MARK_USER);
			bpi = s->m_extras->FindBreakpointFromLine(s,l);
			if (bpi) {
				s->MarkerDeleteHandle(bpi->marker_handle);
				bpi->marker_handle=-1;
			}
		}
		void Set(mxSource *s, int l) { 
			if (um) s->MarkerAdd(l,mxSTC_MARK_USER);
			if (bpi) { bpi->line_number=l; bpi->SetMarker(); }
		}
	};
	amc_aux v[1000];
	bool u;
	int m,n; 
	mxSource *s;
	auxMarkersConserver(mxSource *src, int min, int max, bool up) {
		u=up; s=src; n = max-min+2; m = min-1;
		if (n+1>1000) { n=0; return; }
		for(int i=0;i<=n;i++) v[i].Get(s,m+i);
	}
	~auxMarkersConserver() {
		if (u) {
			for(int i=1;i<n;i++) v[i].Set(s,m+i-1);
			v[0].Set(s,m+n-1); v[n].Set(s,m+n);
		} else {
			v[0].Set(s,m); v[n].Set(s,m+1);
			for(int i=1;i<n;i++) v[i].Set(s,m+i+1);
		}
	}
};

void mxSource::GetSelectedLinesRange(int &lmin, int &lmax) {
	int sel_end = GetSelectionEnd();
	lmin = LineFromPosition(GetSelectionStart());
	lmax = LineFromPosition(sel_end);
	if (lmin>lmax) { int aux=lmin; lmin=lmax; lmax=aux; }
	if (lmin<lmax && PositionFromLine(lmax)==sel_end) lmax--;
}

void mxSource::OnEditToggleLinesUp (wxCommandEvent &event) {
	int min,max; GetSelectedLinesRange(min,max);
	if (min>0) {
		UndoActionGuard undo_action(this);
		wxString line = GetLine(min-1);
		if (max==GetLineCount()-1) AppendText("\n");
		auxMarkersConserver aux_mc(this,min,max,true);
		SetTargetStart(PositionFromLine(max+1));
		SetTargetEnd(PositionFromLine(max+1));
		ReplaceTarget(line);
		SetTargetStart(PositionFromLine(min-1));
		SetTargetEnd(PositionFromLine(min));
		ReplaceTarget("");
		EnsureVisibleEnforcePolicy(min);
	}
}

void mxSource::OnEditToggleLinesDown (wxCommandEvent &event) {
	int min,max; GetSelectedLinesRange(min,max);
	int ss = GetSelectionStart(), se = GetSelectionEnd();
	if (PositionFromLine(min)==ss) ss=-1;
	if (GetLineEndPosition(max)==se||PositionFromLine(max+1)==se) se=-1;
	if (max+1<GetLineCount()) {
		auxMarkersConserver aux_mc(this,min,max,false);
		UndoActionGuard undo_action(this);
		wxString line = GetLine(max+1);
		SetTargetStart(GetLineEndPosition(max));
		SetTargetEnd(GetLineEndPosition(max+1));
		ReplaceTarget("");
		SetTargetStart(PositionFromLine(min));
		SetTargetEnd(PositionFromLine(min));
		ReplaceTarget(line);
		if (ss==-1) SetSelectionStart(PositionFromLine(min+1));
		if (se==-1) SetSelectionEnd(GetLineEndPosition(max+1));
		EnsureVisibleEnforcePolicy(max);
	}	
}

void mxSource::OnComment (wxCommandEvent &event) {
	int ss = GetSelectionStart(), se = GetSelectionEnd();
	int min,max; GetSelectedLinesRange(min,max);
	if (min==max && se!=ss) {
		ReplaceSelection(wxString("/*")<<GetSelectedText()<<"*/");
		SetSelection(ss,se+4);
		return;
	}
	UndoActionGuard undo_action(this);
	if (cpp_or_just_c) {
		for (int i=min;i<=max;i++) {
			//if (GetLine(i).Left(2)!="//") {
			SetTargetStart(PositionFromLine(i));
			SetTargetEnd(PositionFromLine(i));
			ReplaceTarget("//");
		}	
	} else {
		for (int i=min;i<=max;i++) {
			int lp=PositionFromLine(i), ll=GetLineEndPosition(i); 
			while (true) {
				int s; char c; // para los II_*
				int l0=lp;
				while (lp<ll && II_IS_2(lp,' ','\t')) lp++;
				if (lp==ll) break;
				if (II_IS_COMMENT(lp)) {
					while (lp<ll && II_IS_COMMENT(lp)) lp++;
					if (lp==ll) break;
				} else {
					int le=lp;
					while (le<ll && !II_IS_COMMENT(le)) le++;
					SetTargetStart(le); SetTargetEnd(le); ReplaceTarget("*/");
					SetTargetStart(l0); SetTargetEnd(l0); ReplaceTarget("/*");
					lp=le+4; ll+=4;
				}
			}
		}
	}
}

void mxSource::OnUncomment (wxCommandEvent &event) {
	int ss = GetSelectionStart();
	int min,max; GetSelectedLinesRange(min,max);
	UndoActionGuard undo_action(this);
	bool did_something=false;
	for (int i=min;i<=max;i++) {
		int s, p=GetLineIndentPosition(i); char c=GetCharAt(p+1); // s y c se reutilizan para II_*
		if (II_IS_COMMENT(p)) {
			bool remove_asterisco_barra=false, add_barra_asterisco=false;
			if (GetCharAt(p)=='/' && (c=='/' || c=='*')) {
				SetTargetStart(p);
				SetTargetEnd(p+2);
				ReplaceTarget("");
				remove_asterisco_barra = c=='*';
			} else if (i>0) {
				p=GetLineEndPosition(i-1);
				SetTargetStart(p);
				SetTargetEnd(p);
				ReplaceTarget("*/");
				p+=2;
				remove_asterisco_barra=add_barra_asterisco=true;
			}
			if (remove_asterisco_barra||add_barra_asterisco) {
				int p2=p,pl=GetLineEndPosition(i);
				while (p2<pl && !(GetCharAt(p2)=='*' && GetCharAt(p2+1)=='/') ) p2++;
				if (p2!=pl) {
					SetTargetStart(p2);
					SetTargetEnd(p2+2);
					ReplaceTarget("");
					Colourise(p,p2);
				} else if (add_barra_asterisco && i+1<GetLineCount()) {
					p=PositionFromLine(i+1);
					SetTargetStart(p);
					SetTargetEnd(p);
					ReplaceTarget("/*");
				}
				did_something=true; // can we just return now?
			} 
		}
	}
	
	// si no hizo nada por linea, recupera el funcionamiento original para comentarios con /* y */ dentro de una linea
	if (!did_something && GetStyleAt(ss)==wxSTC_C_COMMENT && GetLine(min).Left((GetLineIndentPosition(min))-PositionFromLine(min)+2).Right(2)!="//") {
		int se=ss, l=GetLength();
		while (ss>0 && (GetCharAt(ss)!='/' || GetCharAt(ss+1)!='*') )
			ss--;
		if (!se) 
			se++;
		while (se<l && (GetCharAt(se-1)!='*' || GetCharAt(se)!='/') )
			se++;
		if (GetCharAt(se)=='/' && GetCharAt(se-1)=='*') {
			SetTargetStart(se-1);
			SetTargetEnd(se+1);
			ReplaceTarget("");
		}
		if (GetCharAt(ss)=='/' && GetCharAt(ss+1)=='*') {
			SetTargetStart(ss);
			SetTargetEnd(ss+2);
			ReplaceTarget("");
		}
		SetSelection(ss,se-3);
		return;
	}
	
}


bool mxSource::LoadFile (const wxFileName &filename) {
	wxString ext = filename.GetExt().MakeLower();
	if (mxUT::ExtensionIsCpp(ext) || mxUT::ExtensionIsH(ext)) {
		SetStyle(wxSTC_LEX_CPP);
	} else if (ext=="htm" || ext=="html") {
		SetStyle(wxSTC_LEX_HTML);
	} else if (ext=="sh") {
		SetStyle(wxSTC_LEX_BASH);
	} else if (ext=="xml") {
		SetStyle(wxSTC_LEX_XML);
	} else if (filename.GetName().MakeUpper()=="MAKEFILE") {
		SetStyle(wxSTC_LEX_MAKEFILE);
	} else {
		SetStyle(wxSTC_LEX_NULL);
	}
	
	m_cem_ref.SetFName(filename.GetFullPath());
	source_filename = filename;
	source_time = source_filename.GetModificationTime();
	cpp_or_just_c = source_filename.GetExt().Lower()!="c";
	working_folder = filename.GetPath();
	if (project)
		binary_filename="binary_filename_not_seted";
	else
		binary_filename=source_filename.GetPathWithSep()+source_filename.GetName()+_T(BINARY_EXTENSION);
	SetReadOnly(false); // para que wxStyledTextCtrl::LoadFile pueda modificarlo
	ClearAll ();
	sin_titulo=!wxStyledTextCtrl::LoadFile(source_filename.GetFullPath());
	EmptyUndoBuffer();
	if (readonly_mode!=ROM_NONE) SetReadOnly(true);
	config_source.wrapMode = config->Init.wrap_mode==2||(config->Init.wrap_mode==1&&(lexer!=wxSTC_LEX_CPP||!config_source.syntaxEnable));
	SetWrapMode (config_source.wrapMode?wxSTC_WRAP_WORD: wxSTC_WRAP_NONE);
	SetLineNumbers();
	return true;
}

bool mxSource::SaveSource() {
	int lse = GetEndStyled();
	StartStyling(0,wxSTC_INDICS_MASK);
	SetStyling(GetLength(),0);
	StartStyling(lse,0x1F);
	if (lexer==wxSTC_LEX_CPP && config_source.avoidNoNewLineWarning && GetLine(GetLineCount()-1)!="")
		AppendText("\n");
	sin_titulo = false;
	bool ret=MySaveFile(source_filename.GetFullPath());
	SetSourceTime(source_filename.GetModificationTime());
	if (source_filename==config->Files.autocodes_file||(project&&source_filename==DIR_PLUS_FILE(project->path,project->autocodes_file)))
		Autocoder::GetInstance()->Reset(project?project->autocodes_file:"");
	return ret;
}

bool mxSource::SaveTemp (wxString fname) {
	bool mod = GetModify();
	bool ret=MySaveFile(fname);
	SetModify(mod);
	return ret; 
}

bool mxSource::SaveTemp () {
	bool mod = GetModify();
	int lse = GetEndStyled();
	StartStyling(0,wxSTC_INDICS_MASK);
	SetStyling(GetLength(),0);
	StartStyling(lse,0x1F);
	if (lexer==wxSTC_LEX_CPP && config_source.avoidNoNewLineWarning && GetLine(GetLineCount()-1)!="")
		AppendText("\n");
	if (sin_titulo)
		binary_filename=temp_filename.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR)+temp_filename.GetName()+_T(BINARY_EXTENSION);
	bool ret=MySaveFile(temp_filename.GetFullPath());
	SetModify(mod);
	return ret; 
}

bool mxSource::SaveSource (const wxFileName &filename) {
	int lse = GetEndStyled();
	StartStyling(0,wxSTC_INDICS_MASK);
	SetStyling(GetLength(),0);
	StartStyling(lse,0x1F);
	if (lexer==wxSTC_LEX_CPP && config_source.avoidNoNewLineWarning && GetLine(GetLineCount()-1)!="")
		AppendText("\n");
	if (MySaveFile(filename.GetFullPath())) {
		m_cem_ref.SetFName(filename.GetFullPath());
		source_filename = filename;
		working_folder = filename.GetPath();
		cpp_or_just_c = source_filename.GetExt().Lower()!="c";
		if (project)
			binary_filename=source_filename.GetPathWithSep()+source_filename.GetName()+".o";
		else
			binary_filename=source_filename.GetPathWithSep()+source_filename.GetName()+_T(BINARY_EXTENSION);
		sin_titulo = false;
		bool ret=MySaveFile(source_filename.GetFullPath());
		SetSourceTime(source_filename.GetModificationTime());
		return ret;
	}
	return false;
}

void mxSource::UpdateCalltipArgHighlight (int current_pos) {
	int p=calltip_brace+1, cur_arg=0, par=0, l=current_pos; // s y l son para II_*
	while (p<l) {
		int s;
		II_FRONT(p, II_SHOULD_IGNORE(p));
		char c=GetCharAt(p++);
		if (c==',' && par==0) { cur_arg++; }
		else if (c=='(' || c=='[' || c=='{') { par++; }
		else if (c==')' || c==']' || c=='}') { par--; }
	}
	calltip->SetArg(cur_arg);
}

void mxSource::OnUpdateUI (wxStyledTextEvent &event) {
	if (calltip_mode==MXS_AUTOCOMP && calltip && !wxStyledTextCtrl::AutoCompActive()) HideCalltip();
	int cl=GetCurrentLine();
	if (first_view) {
		ScrollToColumn(0);
		first_view=false;
	} else {
		if (calltip_mode==MXS_BALOON) HideCalltip();
		else if (calltip_mode==MXS_CALLTIP) {
			if (GetCurrentLine()!=calltip_line) {
				HideCalltip();
			} else {
				int cp = GetCurrentPos();
				if (cp<=calltip_brace) { 
					HideCalltip();
				} else {
					UpdateCalltipArgHighlight(cp);
				}
			}
		}
	}
	g_navigation_history.OnJump(this,cl);
	if (!config_source.lineNumber) main_window->status_bar->SetStatusText(wxString("Lin ")<<cl<<" - Col "<<GetCurrentPos()-PositionFromLine(cl),1);
	
	
	// tomado de pseint, para resaltar las partes a completar
//	if (GetStyleAt(p)&wxSTC_INDIC2_MASK) {
//		int p2=p;
//		while (GetStyleAt(p2)&wxSTC_INDIC2_MASK)
//			p2++;
//		while (GetStyleAt(p)&wxSTC_INDIC2_MASK)
//			p--;
//		int s1=GetAnchor(), s2=GetCurrentPos();
//		if (s1==s2) {
//			if (s1==p+1 && last_s1==p+1 && last_s2==p2) {
//				SetSelection(p,p);
//			} else {
//				SetAnchor(p+1);
//				SetCurrentPos(p2);
//			}
//		} else if (s1>s2) {
//			if (s1<p2) SetAnchor(p2);
//			if (s2>p+1) SetCurrentPos(p+1);
//		} else {
//			if (s2<p2) SetCurrentPos(p2);
//			if (s1>p+1) SetAnchor(p+1);
//		}
//		last_s1=GetSelectionStart(); last_s2=GetSelectionEnd();
//	}
	if (multi_sel) multi_sel.ApplyRectEdit(this);
}

/// aux function for FindTypeOf and OnCharAdded
template<int N> bool mxSource::TextRangeIs(int pos_start, const char (&word)[N]) {
	for(int i=pos_start,j=0;j<N-1;i++,j++) 
		if (GetCharAt(i)!=word[j])
			return false;
	return true;
}
/// aux function for FindTypeOf and OnCharAdded
template<int N> bool mxSource::TextRangeIs(int pos_start, int pos_end, const char (&word)[N]) {
	if (pos_end-pos_start!=N-1) return false;
	else return TextRangeIs(pos_start,word);
}

/// aux function for FindTypeOf and OnCharAdded
template<int N> bool mxSource::TextRangeWas(int pos_end, const char (&word)[N]) {
	if (pos_end<N-2) return false;
	else return TextRangeIs(pos_end-(N-2),word);
}



void mxSource::OnCharAdded (wxStyledTextEvent &event) {
	if (ignore_char_added) return;
//	RaiiRestoreValue<SingleList<mxSource::MacroAction>*> mask_macro_events(main_window->m_macro,nullptr);
	char chr = (char)event.GetKey();
	// la siguiente condicion verifica contra el estado interno de wx porque si el 
	// usuario estaba en un menu, se movi� r�pido a otro lugar y lanz� un segundo
	// menu, puede que en el medio no se haya ejecutado ning�n updateui como para
	// que mi calltip_mode se entere de ese cambio... y en ese caso el filtro reventar�a
	if (calltip_mode==MXS_AUTOCOMP && wxStyledTextCtrl::AutoCompActive()) { 
		if (!II_IS_KEYWORD_CHAR(chr)) HideCalltip();
		else if (config_source.autocompFilters) 
			g_code_helper->FilterAutocomp(this,GetTextRange(autocomp_helper.GetBasePos(),GetCurrentPos()));
	}
	if (config_source.autocloseStuff) {
		int pos=GetCurrentPos();
		if ((chr==']'||chr==')'|| chr=='\''||chr=='}'||chr=='\"') && GetCharAt(pos)==chr) {
			if (chr==')' && BraceMatch(pos)==calltip_brace) HideCalltip();
			SetTargetStart(pos);
			SetTargetEnd(pos+1);
			ReplaceTarget(""); 
			return;
		}
		if (chr=='<' && GetStyleAt(pos)==wxSTC_C_PREPROCESSOR && pos>GetLineIndentPosition(LineFromPosition(pos))+8&&GetTextRange(GetLineIndentPosition(LineFromPosition(pos)),GetLineIndentPosition(LineFromPosition(pos))+8)=="#include") { 
			InsertText(pos++,">" );
		} else if (chr=='(') { 
			InsertText(pos++,")" );
		} else if (chr=='{') {
			// ver si va o no adem�s el punto y coma
			int p_start = GetStatementStartPos(pos-2); // buscar donde empieza la declaraci�n
			if (GetStyleAt(p_start)==wxSTC_C_WORD && GetTextRange(p_start,p_start+8)=="template") { // si es template, saltear el template
				int l=pos,s; char c; // auxiliares para II_*
				p_start+=8;	II_FRONT(p_start,II_IS_NOTHING_4(p_start));
				if (GetCharAt(p_start)=='<') {
					int pos_close = BraceMatch(p_start);
					if (p_start!=wxSTC_INVALID_POSITION) {
						p_start=pos_close+1;
						II_FRONT(p_start,II_IS_NOTHING_4(p_start));
					}
				}
			}
			if (GetStyleAt(p_start)==wxSTC_C_WORD && (
				GetTextRange(p_start,p_start+5)=="class"
				|| GetTextRange(p_start,p_start+6)=="struct") )
					InsertText(pos++,"};");
			else InsertText(pos++,"}");
		} else if (chr=='[') { 
			InsertText(pos++,"]" );
		} else if (chr=='\"') { 
			InsertText(pos++,"\"" );
		} else if (chr=='\'') { 
			InsertText(pos++,"\'" );
		} 
	}
	
	if (config_source.smartIndent && config_source.syntaxEnable) {
		if (chr=='e') {
			// si es una 'e', vemos si decia 'else' solo en esa linea y contamos cuantos ifs para atras empezo e indentamos igual que el if correspondiente
			int e=GetCurrentPos(), p=PositionFromLine(GetCurrentLine());
			while (p<e && (GetCharAt(p)==' ' || GetCharAt(p)=='\t')) 
				p++;
			if (p+4==e && GetTextRange(p,e)=="else") {
				char c; int s;
				p=PositionFromLine(GetCurrentLine())-1;
				while (p>=0 && ((c=GetCharAt(p))==' ' || c=='\t'  || c=='\r'  || c=='\n' || (s=GetStyleAt(p))==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_PREPROCESSOR || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR))
					p--;
				if (p>0) {
					if (c=='}') {
						p=BraceMatch(p);
						if (p==wxSTC_INVALID_POSITION)
								return;
						p--;
						while (p>=0 && ((c=GetCharAt(p))==' ' || c=='\t'  || c=='\r'  || c=='\n' || (s=GetStyleAt(p))==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_PREPROCESSOR || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR))
							p--;
					}
					bool baux; e=0;
					while (p>0 && ((baux=!(GetCharAt(p)=='f' && GetCharAt(p-1)=='i' && GetStyleAt(p)==wxSTC_C_WORD)) || e!=0) ) {
						if (!baux)
							e--; 
						else if (p>2 && GetCharAt(p-2)=='l' && GetCharAt(p-1)=='s' && GetCharAt(p)=='e' && GetCharAt(p-3)=='e' && GetStyleAt(p)==wxSTC_C_WORD)
							 e++;
						p--;
					}
					if (p>0)
						SetLineIndentation(GetCurrentLine(),GetLineIndentation(LineFromPosition(p)));
				}
			}
			
		} else if (chr==':') {
			// si son dos puntos y decia public:, private:, protected:,  default: o case algo: quitamos uno al indentado
			int s,e=GetCurrentPos(), p=PositionFromLine(GetCurrentLine());
			Colourise(e-1,e);
			while (p<e && (GetCharAt(p)==' ' || GetCharAt(p)=='\t')) 
				p++;
			if ( (p+7==e && GetTextRange(p,e)=="public:") ||
				(p+8==e && GetTextRange(p,e)=="private:") ||
				(p+10==e && GetTextRange(p,e)=="protected:") ||
				(p+8==e && GetTextRange(p,e)=="default:") ||
				(p+6<e && GetTextRange(p,p+5)=="case " && (s=GetStyleAt(e))!=wxSTC_C_CHARACTER && s!=wxSTC_C_STRING && s!=wxSTC_C_STRINGEOL) 
				)  {
					char c; e=p-1;
					while (e>0 && (c=GetCharAt(e))!='{') {
						if (c=='}' && !II_SHOULD_IGNORE(e)) 
							if ((e=BraceMatch(e))==wxSTC_INVALID_POSITION) {
								e=GetLineIndentation(p=GetCurrentLine())-config_source.tabWidth;
								SetLineIndentation(p,e<0?0:e);
							}
						e--;
					}
					SetLineIndentation(GetCurrentLine(),GetLineIndentation(LineFromPosition(e)));
			}
			
		} else if (chr=='#') {
			int p=GetCurrentPos()-2, l=GetCurrentLine();
			int e=PositionFromLine(l);
			char c;
			while (e<=p && ((c=GetCharAt(p))==' ' || c=='\t'))
				p--;
			if (e>p)
				SetLineIndentation(GetCurrentLine(),0);
			
		} else if (chr=='{') {
			// llave que abre, si est� sola en la linea, corregirle el indentado
			int p=GetCurrentPos()-2, l=GetCurrentLine();
			int e=PositionFromLine(l);
			char c;
			while (e<=p && ((c=GetCharAt(p))==' ' || c=='\t')) p--;
			if (e>=p) {
				char c; int s;
				II_BACK(p,II_IS_NOTHING_4(p));
				if (c==')' && (p=BraceMatch(p))!=wxSTC_INVALID_POSITION) {
					p = GetStatementStartPos(p-1,true,true,false);
					CopyIndentation(l,p);
				}
			}
			
		} else if (chr=='}') {
			// si es una llave que cierra y esta sola, la colocamos a la altura de la que abre
			int p=GetCurrentPos(), s=PositionFromLine(GetCurrentLine());
			p-=2;
			while (p>=s && (GetCharAt(p)==' ' || GetCharAt(p)=='\t')) 
				p--;
			if (p<s) {
				p=GetCurrentPos()-1;
				Colourise(p,p+1);
				p=BraceMatch(p);
				if (p!=wxSTC_INVALID_POSITION) {
					int indent_level = GetLineIndentation(LineFromPosition(GetStatementStartPos(p-1,true)));
					SetLineIndentation(GetCurrentLine(),indent_level);
				}
				event.Skip();
			}
			return;
			
		} else if (chr == '\n') { // si es un enter, es un viaje!!!
			
			int current_pos = GetCurrentPos(), current_line = GetCurrentLine();
			int s, l=GetLength(); char c='\n'; // auxiliares para los II_*
			
			// curr_last: caracter no nulo previo al enter 
			int p_curr_last = current_pos-1;
			II_BACK(p_curr_last,II_IS_NOTHING_4(p_curr_last));
			if (!p_curr_last) return;
			char c_curr_last = c; // ultimo caracter antes del evento
			
			// next_char proximo caracter no nulo en la misma linea de current_pos (o \n)
			int p_next_char = current_pos;
			II_FRONT(p_next_char,II_IS_2(p_next_char,'\t',' '));
			char c_next_char = l==p_next_char?' ':c;
			
			if (c_next_char=='{') { // si estaba antes de la llave que abre, la llave va a la altura del prototipo (previa instr.)
				int p_prev_ind = GetStatementStartPos(p_curr_last,true);
				CopyIndentation(current_line,p_prev_ind,c_curr_last=='{');
				
			} else if (c_next_char=='}') { // si esta justo antes de una llave que cierra, indentarla igual que la que abre
				int p_otra_llave = BraceMatch(p_next_char);
				if (p_otra_llave==wxSTC_INVALID_POSITION) {
					int p_prev_ind = GetStatementStartPos(p_curr_last,true);
					int ind = GetLineIndentation(p_prev_ind)-config_source.tabWidth;
					SetLineIndentation(current_line,ind<0?0:ind);
				} else {
					int p_prev_ind = GetStatementStartPos(p_otra_llave-1,true);
					CopyIndentation(current_line,p_prev_ind);
					if (c_curr_last=='{' && LineFromPosition(p_curr_last)==current_line-1) { // si estaba justo entre las dos llaves {} poner un enter mas
						if (config_source.bracketInsertion) InsertText(PositionFromLine(current_line),"\n");
						CopyIndentation(current_line,p_prev_ind,true);
					}
				}
			} else if (c_next_char=='/' && TextRangeIs(p_next_char+1,"/\t") &&
			           current_line && PositionFromLine(current_line-1)==current_pos-1)
			{ // si era un comentario, de los que van a la izquierda (lo distingo por el tab), dejarlo a la izquierda
				; // no hacer nada
			
			} else if ( c_curr_last==';' || c_curr_last=='{' || c_curr_last=='}' ) { // nueva sentencia
				// prev_ind: donde empieza realmente la instruccion anterior (salteando indentado)
				if (c_curr_last=='}') p_curr_last++;
				int p_prev_ind = GetStatementStartPos(p_curr_last-1,true,true,c_curr_last=='{');
				while (GetStyleAt(p_prev_ind)==wxSTC_C_WORD && TextRangeIs(p_prev_ind,"else")) { // si es un else, ir al if
					--p_prev_ind;
					II_BACK(p_prev_ind,II_IS_NOTHING_4(p_prev_ind));
					if (GetCharAt(p_prev_ind)=='}') ++p_prev_ind;
					p_prev_ind = GetStatementStartPos(p_prev_ind-1,true);
				}
				bool increase_level = c_curr_last=='{' || (GetStyleAt(p_prev_ind)==wxSTC_C_WORD && ( // si abria una llave, o venia de una etiqueta, aumentar
										TextRangeIs(p_prev_ind,"public:")||TextRangeIs(p_prev_ind,"private:")||
										TextRangeIs(p_prev_ind,"protected:")||TextRangeIs(p_prev_ind,"case:")||
										TextRangeIs(p_prev_ind,"default:") ) );
				CopyIndentation(current_line,p_prev_ind,increase_level);
				
				if (c_curr_last=='{' && config_source.bracketInsertion && LineFromPosition(p_curr_last)==current_line-1) { // si estabamos despues de llave que abre, ver si agregar la que cierra
						int p_next_line = GetLineEndPosition(current_line); 
						l=GetLength(); II_FRONT(p_next_line,II_IS_NOTHING_4(p_next_line));
						char c_next_line = c;
						int ind_next = GetLineIndentation(LineFromPosition(p_next_line));
						int ind_cur = GetLineIndentation(LineFromPosition(p_prev_ind));
						if ( (p_next_line==l || (ind_cur > ind_next || (ind_cur==ind_next && c_next_line!='}'))) 
							&& !(GetStyleAt(p_next_line)==wxSTC_C_WORD 
									&& ( TextRangeIs(p_next_line,"public")||TextRangeIs(p_next_line,"protected")||TextRangeIs(p_next_line,"private")
										 ||TextRangeIs(p_next_line,"case")||TextRangeIs(p_next_line,"default") ) )
						) {
							UndoActionGuard undo_action(this);
							// ver primero si la llave ya estaba en la misma linea para simplemente bajarla
							int p_otra_llave = BraceMatch(p_curr_last);
							if (p_otra_llave!=wxSTC_INVALID_POSITION && LineFromPosition(p_otra_llave)==current_line) {
								InsertText(p_otra_llave,"\n");
								CopyIndentation(current_line+1,p_prev_ind);
							} else { 
								// no estaba, hay que agregarla, ver si va "}" o "};"
								int p_aux = p_curr_last-1;
								II_BACK(p_aux,II_IS_NOTHING_4(p_aux)); 
								wxString to_insert = "\n};";
								if (c==')') to_insert = "\n}";
								else if ( s==wxSTC_C_WORD) {
									//.. o un else, o un calificativo de metodo...
									if (TextRangeWas(p_aux,"const") || TextRangeWas(p_aux,"else") || 
										TextRangeWas(p_aux,"override") || TextRangeWas(p_aux,"explicit") )
									{
										to_insert = "\n}";
									} else if (TextRangeWas(p_aux,"do")) // si es "do" agregar tambien el "while(...);"
										to_insert = "\n} while();";
								} else if (s==wxSTC_C_OPERATOR && (c==']'||c=='&')) { // ...o para la sobrecarga por tipo de refencia, o atributos
									to_insert="\n}";
								}
										
								InsertText(GetLineEndPosition(current_line),to_insert);
								CopyIndentation(current_line+1,p_prev_ind);
							}
						}
					}
				
			} else { // continuacion de algo anterior
				// curr_beg: donde empieza la instruccion actual (con espacios y tabs)
				int p_curr_beg = GetStatementStartPos(p_curr_last,true,false,true); // el ultimo true es por los ifs anidados!
				char c_prev_last = GetCharAt(p_curr_beg?p_curr_beg-1:0);
				
				if (c_prev_last=='(') { // argumentos de funcion
					SetLineIndentation(current_line,GetColumn(p_curr_beg));
				
				} else {
					
					// curr_ind: donde realmente empieza la instruccion actual (saltando espacios y tabs)
					int p_curr_ind = p_curr_beg, l=current_pos;
					II_FRONT(p_curr_ind,II_IS_NOTHING_4(p_curr_ind));
					CopyIndentation(current_line,p_curr_ind,true);
					
					if (c_curr_last==',' && c_prev_last=='{' && p_curr_beg>2) { // si era enum, corregir
						int p_aux = GetStatementStartPos(p_curr_beg-2);
						if (p_aux && GetStyleAt(p_aux)==wxSTC_C_WORD && TextRangeIs(p_aux,"enum")) {
							CopyIndentation(current_line,p_aux,true);
						}
					} else if (c_curr_last=='>') { // si cerraba template, corregir (sigue el prototipo, pero al mismo nivel que el template)
						int p_curr_ind = p_curr_beg; II_FRONT(p_curr_ind,II_IS_NOTHING_4(p_curr_ind));
						if (GetStyleAt(p_curr_ind)==wxSTC_C_WORD && TextRangeIs(p_curr_ind,"template") ) { 
							CopyIndentation(current_line,p_curr_ind,false);
						}
					} else if (c_curr_last==',' && GetStyleAt(p_curr_ind)==wxSTC_C_WORD && TextRangeIs(p_curr_ind,"template")) { // si estamos en la lista de argumentos del template
						int p=p_curr_ind+8; II_FRONT(p,II_IS_NOTHING_4(p));
						if (GetCharAt(p)=='<') { // verificar que efectivamente estemos en la lista
							int p2 = BraceMatch(p);
							if (p2==wxSTC_INVALID_POSITION||p2>current_pos) {
								int pos_line_start = PositionFromLine(LineFromPosition(p));
								SetLineIndentation(current_line,p-pos_line_start+1);
							}
						}
					}
				}
			}

			wxStyledTextCtrl::GotoPos(GetLineIndentPosition(current_line));
			return;
		}
		
	} else if (chr=='\n') {
		int currentLine = GetCurrentLine();
		int lineInd = 0;
		if (currentLine > 0)
			lineInd = GetLineIndentation(currentLine - 1);
		if (lineInd == 0) return;
		SetLineIndentation (currentLine, lineInd);
		wxStyledTextCtrl::GotoPos(GetLineIndentPosition(currentLine));
	}
	
	if (calltip_mode==MXS_CALLTIP && chr==')') {
		int p=GetCurrentPos()-1;
		Colourise(p,p+1);
		if (BraceMatch(p)==calltip_brace) HideCalltip();
	} else if ( ((calltip_mode!=MXS_AUTOCOMP || chr=='.') && config_source.autoCompletion) || ((chr==',' || chr=='(') && config_source.callTips) ) {
		if (chr==':') {
			int p=GetCurrentPos();
			if (p>1 && GetCharAt(p-2)==':') {
				p=p-3;
				int s; char c;
				II_BACK(p,II_IS_NOTHING_2(p));
				if ((c=GetCharAt(p))=='>') {
					int lc=1;
					while (p-- && lc) {
						c=GetCharAt(p);
						if (c=='>' && !II_IS_NOTHING_4(p)) lc++;
						else if (c=='<' && !II_IS_NOTHING_4(p)) lc--;
					}
					II_BACK(p,II_IS_NOTHING_2(p));
					if (p<0) p=0;
				}
				wxString key=GetTextRange(WordStartPosition(p,true),WordEndPosition(p,true));
				if (key.Len()!=0)
					g_code_helper->AutocompleteScope(this,key,"",false,false);
			}
		} else if (chr=='>') {
			int p=GetCurrentPos()-2;
			if (GetCharAt(p)=='-') {
				int dims;
				wxString type = FindTypeOfByPos(p-1,dims);
				if (dims==SRC_PARSING_ERROR) {
					if (type.Len()!=0)
						ShowBaloon(type);
				} else if (dims==1)
					g_code_helper->AutocompleteScope(this,type,"",true,false);
				else if (type.Len()!=0 && dims==0)
					ShowBaloon(LANG(SOURCE_TIP_NO_DEREFERENCE,"Tip: Probablemente no deba desreferenciar este objeto."));
			}
		} else if (chr=='.') {
			bool shouldComp=true;
			char c;
			int p=GetCurrentPos()-2;
			if ((c=GetCharAt(p))>='0' && c<='9') {
				while ( (c=GetCharAt(p))>='0' && c<='9' ) {
					p--;
				}
				shouldComp = (c=='_' || ( (c|32)>='a' && (c|32)<='z' ) );
			}
			if (shouldComp) {
				int dims;
				wxString type = FindTypeOfByPos(p,dims);
				if (dims==SRC_PARSING_ERROR) {
					if (type.Len()!=0)
						ShowBaloon(type);
				} else	if (dims==0)
					g_code_helper->AutocompleteScope(this,type,"",true,false);
				else if (type.Len()!=0 && dims>0)
					ShowBaloon(LANG(SOURCE_TIP_DO_DEREFERENCE,"Tip: Probablemente deba desreferenciar este objeto."));
			}
		} else {
			int s,e=GetCurrentPos()-1,ctp;
			char c;
			if (chr=='(') {
				ctp=e;
				e--;
				II_BACK(e,II_IS_NOTHING_4(e));
			} else if (chr==',') {
				int p=e;
				c=GetCharAt(--p);
				while (true) {
					II_BACK(p, !II_IS_5(p,';','}','{','(',')') || II_SHOULD_IGNORE(p));
					if (c=='(') {
						e=p-1;
						II_BACK(e,II_IS_NOTHING_4(e));
						if (calltip_brace==p && calltip_mode==MXS_CALLTIP) {
							return;
						}
						ctp=p;
						chr='(';
						break;
					} else if (c==')') {
						if ( (p=BraceMatch(p))==wxSTC_INVALID_POSITION )
							break;
						p--;
					} else
						break;
				}
			} else if (chr=='@' || chr=='\\') {
				s=GetStyleAt(e-1);
				if (s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_COMMENTLINEDOC) {
					g_code_helper->AutocompleteDoxygen(this);
					return;
				}
			} else if (chr=='#') {
				int s=GetCurrentLine();
				s=PositionFromLine(s)+GetLineIndentPosition(LineFromPosition(s));
				if (s==e) {
					g_code_helper->AutocompletePreprocesorDirective(this);
					return;
				}
			}
			int p=WordStartPosition(e,true); int pos_key=p; // pos_key guarda la posici�n donde comienza la palabra a autocompletar
			if (e-p+1>=config->Help.min_len_for_completion || chr==',' || chr=='(') {
				wxString key = GetTextRange(p,e+1);
				if (last_failed_autocompletion.IsSameLocation(pos_key,key,parser->data_age)) return; // no intentar autocompletar nuevamente si ya se intent� un caracter atr�s
				s=GetStyleAt(p-1);
				if (p && s==wxSTC_C_PREPROCESSOR) {
					int pos_num = p-1; II_BACK(pos_num,II_IS_2(pos_num,'\t','\n'));
					if (GetCharAt(pos_num)=='#') {
						g_code_helper->AutocompletePreprocesorDirective(this,key);
						return;
					}
				}
				if (p && (s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR) && (GetCharAt(p-1)=='@' || GetCharAt(p-1)=='\\')) {
					g_code_helper->AutocompleteDoxygen(this,key);
					return;
				}
				int dims;
				p--;
				II_BACK(p,II_IS_NOTHING_4(p));
				if (c=='.') {
					wxString type = FindTypeOfByPos(p-1,dims);
					if (dims==0) {
						if (chr=='(' && config_source.callTips)
							g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
						else if ( II_IS_KEYWORD_CHAR(chr) )
							g_code_helper->AutocompleteScope(this,type,key,true,false);
					}
				} else if (c=='>' && GetCharAt(p-1)=='-') {
					p--;
					wxString type = FindTypeOfByPos(p-1,dims);
					if (dims==1) {
						if (chr=='(' && config_source.callTips)
							g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
						else if ( II_IS_KEYWORD_CHAR(chr) )
							g_code_helper->AutocompleteScope(this,type,key,true,false);
					}
				} else if (c==':' && GetCharAt(p-1)==':') {
					p-=2;
					II_BACK(p,II_IS_NOTHING_4(p));
					wxString type = GetTextRange(WordStartPosition(p,true),p+1);
					if (chr=='(' && config_source.callTips)
						g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
					else if ( II_IS_KEYWORD_CHAR(chr) )
						g_code_helper->AutocompleteScope(this,type,key,true,false);
				} else {
					if (chr=='(' && config_source.callTips) {
						if (!g_code_helper->ShowFunctionCalltip(ctp,this,FindScope(GetCurrentPos()),key,false)) {
							// mostrar calltips para constructores
							p=ctp-1;
							bool f;
							while ( p>0 && ( !II_IS_3(p,'(','{',';') || (f=II_SHOULD_IGNORE(p))) ) {
								if (!f && c==')' && (p=BraceMatch(p))==wxSTC_INVALID_POSITION)
									return;
								p--;
							}
							p++;
							II_FRONT_NC(p,II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p) || (s==wxSTC_C_WORD));
							int p1=p;
							II_FRONT_NC(p,(c=GetCharAt(p))=='_'||II_IS_KEYWORD_CHAR(c));
							if (!g_code_helper->ShowConstructorCalltip(ctp,this,GetTextRange(p1,p))) {
								if (!g_code_helper->ShowConstructorCalltip(ctp,this,key)) {
									// mostrar sobrecarga del operador()
									wxString type=FindTypeOfByKey(key,p1);
									if (type.Len()) {
										g_code_helper->ShowFunctionCalltip(ctp,this,type,"operator()",true);
									}
								}
							}
						}
					} else if ( II_IS_KEYWORD_CHAR(chr) ) {
						wxString args; int scope_start;
						wxString scope = FindScope(GetCurrentPos(),&args,false,&scope_start);
						g_code_helper->AutocompleteGeneral(this,scope,key,&args,scope_start);
					}
				}
			}
		}
	}
}

void mxSource::SetModify (bool modif) {
	if (modif) {
		int p=GetLength()?GetSelectionStart()-1:0;
		if (GetLength()&&p<1) p=1;
		SetTargetStart(p); 
		SetTargetEnd(p);
		ReplaceTarget(" ");
		SetTargetStart(p); 
		SetTargetEnd(p+1);
		ReplaceTarget("");
	} else 
		SetSavePoint();
}

void mxSource::MarkError (int line, bool focus) {
	int p=PositionFromLine(line);
	while (GetCharAt(p)==' ' || GetCharAt(p)=='\t')
		p++;
	MoveCursorTo(p,focus);
	EnsureVisibleEnforcePolicy(line);
	if (config->Init.autohide_panels) {
		int l0a=GetFirstVisibleLine();
		int l0b=VisibleFromDocLine(line);
		int l0=DocLineFromVisible(l0b)-(l0b-l0a);
		int nl=LinesOnScreen();
		if (line>l0+nl/2) {
			LineScroll(0,line-l0-nl/2+1);
		}
	}
}

void mxSource::MoveCursorTo(long pos, bool focus) {
	SetSelection(pos,pos);
	SetCurrentPos(pos);
	if (focus) SetFocus();
}

void mxSource::OnIndentSelection(wxCommandEvent &event) {
//	main_window->SetStatusText(wxString("Indentando..."));
	int min = LineFromPosition(GetSelectionStart());
	int max = LineFromPosition(GetSelectionEnd());
	Indent(min,max);
	if (min!=max)
		SetSelection(PositionFromLine(min),GetLineEndPosition(max));
//	main_window->SetStatusText(wxString(LANG(GENERAL_READY,"Listo")));
}

/**
* Corrige el indentado de las lineas min a max, incluyendo ambos extremos, 
* La indentacion de cada linea se determina segun la linea anterior, por lo
* que todo el bloque dependera de la linea min-1. Para que funciones 
* correctamente debe estar actualizado el coloreado (llamar a Colourize)
**/
void mxSource::Indent(int min, int max) {
	int ps1=GetSelectionStart() ,ps2=GetSelectionStart();
	int ls1=LineFromPosition(ps1), ls2=LineFromPosition(ps2);
	int ds1=ps1-GetLineIndentPosition(ls1), ds2=ps2-GetLineIndentPosition(ls2);
	if (ds1<0) ds1=0; 
	if (ds2<0) ds2=0;
	// para evitar que al llamar a charadd se autocomplete o cierre algo
	RaiiRestoreValue<int> restore_autocomp(config_source.autoCompletion,0);
	RaiiRestoreValue<bool> restore_autoclose(config_source.autocloseStuff,false);
	RaiiRestoreValue<bool> restore_autotext(config_source.autotextEnabled,false);
	RaiiRestoreValue<bool> restore_bracketinsertion(config_source.bracketInsertion,false);
	RaiiRestoreValue<bool> restore_syntaxenable(config_source.syntaxEnable,true);
	UndoActionGuard undo_action(this);
	if (!config_source.syntaxEnable) { SetLexer (wxSTC_LEX_CPP); Colourise(0,GetLength()); }
	if (min>max) { int aux=min; min=max; max=aux; }
	if (max>min && PositionFromLine(max)==GetSelectionEnd()) max--;
	wxStyledTextEvent evt, evt_n;
	evt_n.SetKey('\n');
	if (min==0)
		SetLineIndentation(0,0);
	for (int i=(min==0?1:min);i<=max;i++) {
		int s; char c; // para los II_*
		SetLineIndentation(i,0);
		int p=PositionFromLine(i);
		SetCurrentPos(p);
		OnCharAdded(evt_n);
		p=PositionFromLine(i);
		II_FRONT_NC(p, II_IS_2(p,' ','\t'));
		if (c=='#' && !II_IS_COMMENT(p)) {
			SetLineIndentation(i,0);
		} else if (c=='}' && !II_SHOULD_IGNORE(p)) {
			evt.SetKey('}');
			SetCurrentPos(p+1);
			OnCharAdded(evt);
		} else if (c=='{' && !II_SHOULD_IGNORE(p)) {
			evt.SetKey('{');
			SetCurrentPos(p+1);
			OnCharAdded(evt);
		} else if(c=='e' && GetStyleAt(p)==wxSTC_C_WORD && GetCharAt(p+1)=='l' && GetCharAt(p+2)=='s' && GetCharAt(p+3)=='e') {
			evt.SetKey('e');
			SetCurrentPos(p+4);
			OnCharAdded(evt);
		} else if (c=='p' && GetTextRange(p,p+8)=="private:" && !II_SHOULD_IGNORE(p)) {
			evt.SetKey(':');
			SetCurrentPos(p+8);
			OnCharAdded(evt);
		} else if (c=='p' && GetTextRange(p,p+7)=="public:" && !II_SHOULD_IGNORE(p)) {
			evt.SetKey(':');
			SetCurrentPos(p+7);
			OnCharAdded(evt);
		} else if (c=='p' && GetTextRange(p,p+10)=="protected:" && !II_SHOULD_IGNORE(p)) {
			evt.SetKey(':');
			SetCurrentPos(p+10);
			OnCharAdded(evt);
		} else if (c=='d' && GetTextRange(p,p+8)=="default:" && !II_SHOULD_IGNORE(p)) {
			evt.SetKey(':');
			SetCurrentPos(p+8);
			OnCharAdded(evt);
		} else if (c=='c' && GetTextRange(p,p+5)=="case " && !II_SHOULD_IGNORE(p)) {
			int a=GetLineEndPosition(i);
			p+=5;
			II_FRONT_NC(p,p<=a && (GetCharAt(p)!=':' || II_SHOULD_IGNORE(p)));
			if (p<=a) {
				evt.SetKey(':');
				SetCurrentPos(p+1);
				OnCharAdded(evt);
			}
		}
		Colourise(PositionFromLine(i),GetLineEndPosition(i));
	}
		
	if (!restore_syntaxenable.GetOriginalValue())
		SetStyle(false);
	else
		Colourise(0,GetLength());
	SetSelectionStart(GetLineIndentPosition(ls1)+ds1);
	SetSelectionEnd(GetLineIndentPosition(ls2)+ds2);
}

void mxSource::SetFolded(int level, bool folded) {
	int ss=GetSelectionStart(), se=GetSelectionEnd();
	if (ss==se) {
		ss=0; 
		se=GetLength()-1;
	}
	if (ss>se)
		Colourise(se,ss);
	else
		Colourise(ss,se);
	if (level)
		level+=1023;
	int min=LineFromPosition(ss);
	int max=LineFromPosition(se);
	if (min>max) { int aux=min; min=max; max=aux; }
	if (!(max>min && PositionFromLine(max)==se)) max++;
	for (int cl, line=min;line<=max;line++) {
		cl=(GetFoldLevel(line)&wxSTC_FOLDLEVELNUMBERMASK);
		if ((!level || cl==level) && (GetFoldLevel(line+1)&wxSTC_FOLDLEVELNUMBERMASK)>cl)
			if (GetFoldExpanded(line)==folded) {
				ToggleFold(line);
			}
	}
	EnsureVisibleEnforcePolicy(GetCurrentLine());
}

int mxSource::InstructionBegin(int p) {
	return GetLineIndentPosition(LineFromPosition(p));
}

void mxSource::SetStyle(bool color) {
	mxSourceBase::SetStyle(color?lexer:wxSTC_LEX_NULL);
	if (m_minimap) m_minimap->SetStyle(color?lexer:wxSTC_LEX_NULL);
	if ((config_source.syntaxEnable=color)) {
		switch (lexer) {
		case wxSTC_LEX_CPP:
			config_source.callTips = config->Source.callTips;
			config_source.autoCompletion = config->Source.autoCompletion;
//			config_source.stdCalltips = config->Source.stdCalltips;
//			config_source.stdCompletion = config->Source.stdCompletion;
//			config_source.parserCalltips = config->Source.parserCalltips;
//			config_source.parserCompletion = config->Source.parserCompletion;
			config_source.smartIndent = config->Source.smartIndent;
			config_source.indentPaste = config->Source.indentPaste;
			break;
		case wxSTC_LEX_HTML: case wxSTC_LEX_XML:
//			config_source.stdCalltips=config_source.stdCompletion=config_source.parserCalltips=config_source.parserCompletion=config_source.smartIndent=config_source.indentPaste=false;
			config_source.callTips = config_source.smartIndent = config_source.indentPaste = false;
			config_source.autoCompletion = 0;
			SetProperty ("fold.html","0");
			SetProperty ("fold.html.preprocessor", "0");
			break;
		case wxSTC_LEX_MAKEFILE:
//			config_source.stdCalltips=config_source.stdCompletion=config_source.parserCalltips=config_source.parserCompletion=config_source.smartIndent=config_source.indentPaste=false;
			config_source.callTips = config_source.smartIndent = config_source.indentPaste = false;
			config_source.autoCompletion = 0;
			break;
		case wxSTC_LEX_BASH:
//			config_source.stdCalltips=config_source.stdCompletion=config_source.parserCalltips=config_source.parserCompletion=config_source.smartIndent=config_source.indentPaste=false;
			config_source.callTips=config_source.smartIndent=config_source.indentPaste=false;
			config_source.autoCompletion=0;
			break;
		}
	} else {
		config_source.syntaxEnable=false;
	}
	Colourise(0,GetLength());
}


void mxSource::SetStyle(int a_lexer) {
	if (a_lexer!=lexer) {
		if (a_lexer==wxSTC_LEX_NULL) {
			config_source.syntaxEnable=false;
			SetStyle(false);
		} else {
			config_source.syntaxEnable=true;
			lexer=a_lexer;
			SetStyle(true);
		}
	}
}

void mxSource::SelectError(int indic, int p1, int p2) {
	if (p1>=GetLength()||p2>GetLength()) return;
	if (p1==p2) {
		if (GetCharAt(p2)==')') {
			p1 = BraceMatch(p2);
			if (p1==wxSTC_INVALID_POSITION) p1=p2;
			else {
				int s; char c; --p1; II_BACK(p1,II_IS_NOTHING_2(p1));
				p2=p1=WordStartPosition(p1,true); 
			}
		}
		p2=WordEndPosition(p1,true);
		while (p2>p1 && (GetCharAt(p2-1)==' ' || GetCharAt(p2-1)=='\t')) p2--;
		if (p2==p1) { GotoPos(p1); return; }
	}
	int lse = GetEndStyled();
//	StartStyling(0,wxSTC_INDICS_MASK);
//	SetStyling(GetLength(),0);
	StartStyling(p1,wxSTC_INDICS_MASK);
	if (indic==1)
		SetStyling(p2-p1,wxSTC_INDIC1_MASK);
	else
		SetStyling(p2-p1,wxSTC_INDIC0_MASK);
	GotoPos(p1);
	SetSelection(p1,p2);
	StartStyling(lse,0x1F);
}

bool mxSource::AddInclude(wxString header, wxString optional_namespace) {
	
	UndoActionGuard undo_action(this);

	bool using_namespace_present=optional_namespace.IsEmpty();
	bool header_present=false;
	
	int lp = GetCurrentPos() , cl = GetCurrentLine();
	wxString oHeader=header;
#ifdef __WIN32__
	header=header.MakeLower();
#endif
	wxString str;
	int s,p,p1,p2,insertion_line_number=0, flag=true, comment;
	int uncomment_line=0;
	for (int i=0;i<cl;i++) {
		p=GetLineIndentPosition(i);
		int le=GetLineEndPosition(i);
		if (GetTextRange(p,p+8)=="#include" || GetTextRange(p,p+10)=="//#include" || GetTextRange(p,p+11)=="// #include") {
			if ( ( comment = (GetTextRange(p,p+2)=="//") ) ) {
				p+=2;
				if (GetCharAt(p)==' ')
					p++;
			}
			if (flag) 
				insertion_line_number=i+1;
			p+=8;
			while (GetCharAt(p)==' ' || GetCharAt(p)=='\t')
				p++;
			p1=p++;
			while ((GetCharAt(p)!='"' || GetCharAt(p)!='>') && p<le)
				p++;
			p2=p;
			str=GetTextRange(p1,p2);
#ifdef __WIN32__
			str=str.MakeLower();
#endif
			if (str==header) {
				if (comment) 
					uncomment_line=i+1;
				else
					header_present=true;
			}
		} else if (le==p || II_SHOULD_IGNORE(p)) {
			if (insertion_line_number==0 && flag)
				insertion_line_number=1;
			else if (flag && GetCharAt(p)=='#') // para que saltee defines e ifdefs
				insertion_line_number=i+1;
		} else if (!optional_namespace.IsEmpty() && GetTextRange(p,p+5)=="using") {
			if (flag) {
				insertion_line_number=i;
				flag=false;
			}
			p+=5;
			while (GetCharAt(p)==' ' || GetCharAt(p)=='\t')
				p++;
			if (GetTextRange(p,p+9)=="namespace") {
				p+=9;
				while (GetCharAt(p)==' ' || GetCharAt(p)=='\t')
					p++;
				if (GetTextRange(p,p+optional_namespace.Len()+1)==optional_namespace+";")
					using_namespace_present=true;
			}
		} else if (!II_SHOULD_IGNORE(p)/* && GetTextRange(p,p+7)!="#define"*/)
			flag=false;
	}
	if (!header_present) {
		if (uncomment_line) {
			p = PositionFromLine(uncomment_line-1);
			SetTargetStart(p);
			while (GetCharAt(p)!='#')
				p++;
			SetTargetEnd(p);
			ReplaceTarget("");
		} else {
			p = PositionFromLine(insertion_line_number);
			wxString line = wxString("#include ")+oHeader+"\n";
			if (p<=lp)
				lp+=line.Len();
			InsertText(p,line);
			if (config_source.syntaxEnable)
				Colourise(p,p+line.Len());
			GotoPos(lp);
			insertion_line_number++;
		}
	}
	if (!using_namespace_present/* && header.Find(".")==wxNOT_FOUND && header.Last()!='\"'*/) {
		p = PositionFromLine(insertion_line_number);
		wxString line = wxString("using namespace ")+optional_namespace+";\n";
		if (p<=lp)
			lp+=line.Len();
		InsertText(p,line);
		if (config_source.syntaxEnable)
			Colourise(p,p+line.Len());
		GotoPos(lp);
	}
	
	wxYield(); // sin esto no se ve el calltip (posiblemente un problema con el evento OnUpdateUI)
	if (!header_present || !using_namespace_present) {
		int lse = GetEndStyled();
		StartStyling(0,wxSTC_INDICS_MASK);
		SetStyling(GetLength(),0);
		StartStyling(lse,0x1F);
		wxString baloon_message;
		if (!header_present) {
			if (uncomment_line)
				baloon_message = LANG2(SOURCE_UNCOMMENTED_FOR_HEADER,"Descomentada linea <{1}>: \"#include <{2}>\".",wxString()<<uncomment_line,oHeader);
			else
				baloon_message = LANG1(SOURCE_ADDED_HEADER,"Cabecera agregada: <{1}>.",oHeader);
		} 
		if (!using_namespace_present) {
			if (!baloon_message.IsEmpty()) baloon_message += "\n";
			baloon_message += LANG1(SOURCE_ADDED_USING_NAMESPACE,"Agregado \"using namespace <{1}>;\"",optional_namespace);
		}
		ShowBaloon(baloon_message);
		return true;
	} else {
		ShowBaloon(wxString(LANG(SOURCE_HEADER_WAS_ALREADY,"Sin cambios, ya estaba la cabecera "))<<oHeader);
		return false;
	}
	
}

void mxSource::OnPopupMenu(wxMouseEvent &evt) {
	if (GetMarginForThisX(evt.GetX())==MARGIN_NULL) OnPopupMenuInside(evt); 
	else OnPopupMenuMargin(evt);
}
	
void mxSource::OnPopupMenuMargin(wxMouseEvent &evt) {
	
	int x=10,p; for(int i=0;i<MARGIN_NULL;i++) { x+=GetMarginWidth(i); }
	int l=LineFromPosition( p=PositionFromPointClose(x,evt.GetY()) );
	int sls = LineFromPosition(GetSelectionStart()), sle = LineFromPosition(GetSelectionEnd());
	if (sle<sls) swap(sle,sls);
	if (GetCurrentLine()!=l && !(l>=sls&&l<=sle)) GotoPos(PositionFromLine(l));
	wxMenu menu("");
	
	BreakPointInfo *bpi=m_extras->FindBreakpointFromLine(this,l);
	if (bpi) {
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnDEBUG,mxID_DEBUG_TOGGLE_BREAKPOINT), LANG(SOURCE_POPUP_REMOVE_BREAKPOINT,"Quitar breakpoint"));
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_DEBUG_ENABLE_DISABLE_BREAKPOINT),LANG(SOURCE_POPUP_ENABLE_BREAKPOINT,"Habilitar breakpoint"))->Check(bpi->enabled);
	} else if (!IsEmptyLine(l))
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnDEBUG,mxID_DEBUG_TOGGLE_BREAKPOINT), LANG(SOURCE_POPUP_INSERT_BREAKPOINT,"Insertar breakpoint"));
	if (!debug->IsDebugging()||debug->CanTalkToGDB())
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnDEBUG,mxID_DEBUG_BREAKPOINT_OPTIONS));
	menu.AppendSeparator();
	int s=GetStyleAt(p);
	if (MarkerGet(l)&(1<<mxSTC_MARK_USER))
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_MARK_LINES), LANG(SOURCE_POPUP_REMOVE_HIGHLIGHT,"Quitar resaltado"));
	else 
		mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_MARK_LINES), LANG(SOURCE_POPUP_HIGHLIGHT_LINES,"Resaltar linea"));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_INDENT));
	if (STYLE_IS_COMMENT(s)) mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_UNCOMMENT));
	else mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_COMMENT));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_DUPLICATE_LINES));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_DELETE_LINES));
	if (l>1) mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_TOGGLE_LINES_UP));
	if (l+1<GetLineCount()) mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_TOGGLE_LINES_DOWN));
	
	main_window->PopupMenu(&menu);
	
}

void mxSource::OnPopupMenuInside(wxMouseEvent &evt, bool fix_current_pos) {
	
	// mover el cursor a la posici�n del click (a menos que haya una selecci�n y se clicke� dentro)
	int p1=GetSelectionStart(), p2=GetSelectionEnd();
	if (fix_current_pos && p1==p2) {
		int p = PositionFromPointClose(evt.GetX(),evt.GetY());
		if (p!=wxSTC_INVALID_POSITION)
			GotoPos(p);
	}

	wxMenu menu(""), *code_menu = new wxMenu("");
	
	int p=GetCurrentPos(); int s=GetStyleAt(GetSelectionEnd()-1);
	wxString key=GetCurrentKeyword(p);
	if (key.Len()!=0) {
		if (key[0]!='#') mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_SOURCE_GOTO_DEFINITION));
		if (!STYLE_IS_COMMENT(s) && !STYLE_IS_CONSTANT(s)) mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_HELP_CODE),LANG1(SOURCE_POPUP_HELP_ON,"Ayuda sobre \"<{1}>\"...",key));
		if (s==wxSTC_C_IDENTIFIER||s==wxSTC_C_GLOBALCLASS||s==wxSTC_C_DEFAULT) { // no se porque el primer char es default y los demas identifier????
//			mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_INSERT_HEADER),LANG1(SOURCE_POPUP_INSERT_INCLUDE,"Insertar #incl&ude correspondiente a \"<{1}>\"",key));
			mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_EDIT_HIGHLIGHT_WORD),LANG1(SOURCE_POPUP_HIGHLIGHT_WORD,"Resaltar identificador \"<{1}>\"",key));
			mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_EDIT_FIND_KEYWORD),LANG1(SOURCE_POPUP_FIND_KEYWORD,"Buscar \"<{1}>\" en todos los archivos",key));
			
		}
	}
	
	if (s==wxSTC_C_PREPROCESSOR || s==wxSTC_C_STRING) {
		if (p1==p2) {
			int pos=GetCurrentPos();
			p1=WordStartPosition(pos,true);
			p2=WordEndPosition(pos,true);
			while (GetCharAt(p1-1)=='.' || GetCharAt(p1-1)=='/' || GetCharAt(p1-1)=='\\' || GetCharAt(p1-1)==':')
				p1=WordStartPosition(p1-1,true);
			while (GetCharAt(p2)=='.' || GetCharAt(p2)=='/' || GetCharAt(p2)=='\\' || GetCharAt(p2)==':')
				p2=WordEndPosition(p2+1,true);
		}
		wxFileName the_one (sin_titulo?GetTextRange(p1,p2):DIR_PLUS_FILE(source_filename.GetPath(),GetTextRange(p1,p2)));
		if (wxFileName::FileExists(the_one.GetFullPath()))
			mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_FILE_OPEN_SELECTED),LANG1(SOURCE_POPUP_OPEN_SELECTED,"&Abrir \"<{1}>\"",GetTextRange(p1,p2)));
	}
	if (p1!=p2 && !m_highlithed_word.IsEmpty()) {
		bool have_hl=false, have_no_hl=false;
		for(int i=p1;i<p2;i++) {  
			if (GetStyleAt(i)==wxSTC_C_GLOBALCLASS) {
				have_hl=true; if (have_no_hl) break;
			} else  {
				have_no_hl=true; if (have_hl) break;
			}
		}
		if (have_hl&&have_no_hl) mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_EDIT_HIGHLIGHTED_WORD_EDITION));
	}
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnHIDDEN,mxID_WHERE_AM_I));
	PopulatePopupMenuCodeTools(*code_menu);
	mxUT::AddSubMenuToMenu(&menu,code_menu,LANG(MENUITEM_TOOLS_CODE,"&Generaci�n de c�digo"),"","");
	menu.AppendSeparator();
	
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_UNDO));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_REDO));
	menu.AppendSeparator();
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_CUT));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_COPY));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_PASTE));
	menu.AppendSeparator();
	if (STYLE_IS_COMMENT(s)) mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_UNCOMMENT));
	else mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_COMMENT));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_INDENT));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_BRACEMATCH));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_SELECT_ALL));
	
	main_window->PopupMenu(&menu, main_window->ScreenToClient(this->ClientToScreen(wxPoint(evt.GetX(),evt.GetY()))) );
}

void mxSource::PopulatePopupMenuCodeTools(wxMenu &menu) {
	int p=GetCurrentPos(); int s=GetStyleAt(GetSelectionEnd()-1);
	wxString key=GetCurrentKeyword(p);
	if (key.Len()!=0) {
		if (s==wxSTC_C_IDENTIFIER||s==wxSTC_C_GLOBALCLASS||s==wxSTC_C_DEFAULT) { // no se porque el primer char es default y los demas identifier????
			mxUT::AddItemToMenu(&menu,_menu_item_2(mnEDIT,mxID_EDIT_INSERT_HEADER),LANG1(SOURCE_POPUP_INSERT_INCLUDE,"Insertar #incl&ude correspondiente a \"<{1}>\"",key));
			int aux; wxString type = FindTypeOfByPos(p,aux,true);
			if ( aux==SRC_PARSING_ERROR || !type.Len() ) {
				mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_GENERATE_FUNCTION_DEC));
				mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_GENERATE_FUNCTION_DEF));
			}
		}
	}
	
	int p1=GetSelectionStart(), p2=GetSelectionEnd();
	// determinar si toda la seleccion corresponde a un mismo scope como para poder hacer refactory local con esas lineas como grupo
	bool single_scope = p1==p2;
	if (!single_scope) {
		if (p2<p1) swap(p1,p2); 
		int a1,a2; single_scope = true;
		for(int i=0;i<2;i++) {
			single_scope = single_scope &&GetCurrentScopeLimits(i?p1:p2,a1,a2,true); // scope mas interno de pX
			while (single_scope && (a1>=p1&&a2<=p2) ) { // mientras est� totalmente contenido... ir un scope m�s "arriba"
				single_scope = GetCurrentScopeLimits(a1-1,a1,a2,true);
			}
			single_scope = single_scope && ( (a1<=p1&&a2>=p2) ); // ahora el scope externo deber�a contener totalmente a p1-p2
		}
	}
	
	if (single_scope) mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_SURROUND_IF));
	if (single_scope) mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_SURROUND_WHILE));
	if (single_scope) mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_SURROUND_DO));
	if (single_scope) mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_SURROUND_FOR));
	mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_SURROUND_IFDEF));
	if (single_scope) mxUT::AddItemToMenu(&menu,_menu_item_2(mnTOOLS,mxID_TOOLS_CODE_EXTRACT_FUNCTION));
}

void mxSource::PopupMenuCodeTools() {
	wxMenu menu("");
	PopulatePopupMenuCodeTools(menu);
	wxPoint pos = this->ClientToScreen(PointFromPosition(GetCurrentPos()));
	pos.y += GetCharHeight();
	main_window->PopupMenu(&menu, main_window->ScreenToClient(pos));
}


int mxSource::FindTextEx(int pfrom, int pto, const wxString &text, int flags) {
	int p = FindText(pfrom,pto,text,flags);
	while ( p!=wxSTC_INVALID_POSITION && ShouldIgnoreByStyleEx(GetStyleAt(p)) )
		p = FindText(p+(pfrom<pto?1:-1),pto,text,flags);
	return p;
}
	
	
/**
* Averigua el tipo de una variable dentro de un contexto. 
* Primero busca en el c�digo del archivo hacia atras y mira lo que parezca
* una declaraci�n de variable. Si piensa que est� en alguna funci�n tambien
* mira los argumentos y anota el scope si la funci�n parece el m�todo de una
* clase, para que al final si no encontr� nada le pueda preguntar al parser
* por esa clase
**/
wxString mxSource::FindTypeOfByKey(wxString &key, int &pos, bool include_template_spec) {
	
	int dims=0;
	bool notSpace=true;
	bool init_pos=true;
	
	wxString ret="",space="";
	
	int p_from = pos;
	int p,s;
	char c;
	
	int p_ocur=wxSTC_INVALID_POSITION;
	int p_type=wxSTC_INVALID_POSITION;
	
	int e=pos-1;
	II_BACK(e,II_IS_NOTHING_4(e));
	if (c=='.') {
		e--;
		II_BACK(e,II_IS_NOTHING_4(e));
		int ddims=0;
		while (c==']') {
			ddims++;
			e=BraceMatch(e);
			if (e==wxSTC_INVALID_POSITION) {
				pos=SRC_PARSING_ERROR;
				return "";
			}
			e--; II_BACK(e,II_IS_NOTHING_4(e));
		}
		int dims=WordStartPosition(e,true);
		wxString space = GetTextRange(dims,e+1);
		wxString type = FindTypeOfByKey(space,dims,include_template_spec);
		dims-=ddims;
		if (dims==0) {
			pos=-1;
			type = g_code_helper->GetAttribType(type,key,pos);
			key = space;
			return type;
		}
	} else if (c=='>' && e!=0 && GetCharAt(e-1)=='-') {
		e-=2;
		II_BACK(e,II_IS_NOTHING_4(e));
		wxString space = FindTypeOfByPos(e,dims,include_template_spec);
		if (space.Len()) { // codigo nuevo, usar el otro FindTypeOf
			pos=dims-1; // pos es argumento de entrada(posicion) y salida(dimension)
			wxString type=g_code_helper->GetAttribType(space,key,pos);
			key=type; // key es argumento de entrada (nombre de var a buscar) y salida (scope)
			return type;
		} else {// fin codigo nuevo, empieza codigo viejo, si no funca que siga como antes (todo: analizar si vale la pena o si con el nuevo ya reemplaza todo)
			int dims=WordStartPosition(e,true);
			wxString space = GetTextRange(dims,e+1);
			wxString type = FindTypeOfByKey(space,dims,include_template_spec);
			if (dims==1) {
				pos=-1;
				type = g_code_helper->GetAttribType(type,key,pos);
				key = space;
				return type;
			}
		}
	} else if (c==':' && e!=0 && GetCharAt(e-1)==':') {
		e-=2;
		II_BACK(e,II_IS_NOTHING_4(e));
		int dims=WordStartPosition(e,true);
		wxString type = GetTextRange(dims,e+1);
		if (dims==0) {
			dims=-1;
			type = g_code_helper->GetAttribType(type,key,dims);
			key = "";
			return type;
		}
	}
	
	while (true) {
		
		c = GetCharAt(p_from);
		
		if (c==')' && !init_pos) { // ver si era una funci�n/m�todo
			// avanzar despues del par�ntesis y ver si abre una llave
			p=p_from+1; int l=GetLength();
			II_FRONT(p,II_IS_NOTHING_4(p))
			// saltear el const
			if (p+5<l&&GetCharAt(p)=='c'&&GetCharAt(p+1)=='o'&&GetCharAt(p+2)=='n'&&GetCharAt(p+3)=='s'&&GetCharAt(p+4)=='t'&&(GetCharAt(p+5)=='{'||II_IS_NOTHING_4(p+5))) {
				p+=5; II_FRONT(p,II_IS_NOTHING_4(p))
			}
			
			if (c!='{') {
				p_from = BraceMatch(p_from);
				if (p_from==wxSTC_INVALID_POSITION)
					break;
			} else {
				int p_to;
				do {
					p_to = BraceMatch(p_from);
					if (p_to==wxSTC_INVALID_POSITION) break;
					p=p_to-1;
					while (p>0 && II_IS_4(p,' ','\t','\n','\r'))
						p--;
					// ver que no sea un costructor de atributo o padre (fix 29/09)
					int p2=p;
					while  ( p>0 &&  ( (c>='a' && c<='z') ||  (c>='0' && c<='9') ||  (c>='A' && c<='Z') || c=='_' || II_SHOULD_IGNORE(p) ) )
						c=GetCharAt(--p);
					while (p>0 && II_IS_4(p,' ','\t','\n','\r'))
						p--;
					if (p==0 || ( c!=',' && (c!=':' || GetCharAt(p-1)==':') ) ) { p=p2; break; }
					p--;
					while (p>0 && II_IS_4(p,' ','\t','\n','\r'))
						p--;
					p_from=p;
				} while (true);
				if (p_to==wxSTC_INVALID_POSITION) break;
				
				if (GetStyleAt(p)!=wxSTC_C_WORD || (p>2 && GetTextRange(p-2,p+1)=="for")) { // si estamos en el prototipo de la funcion
					p=FindText(p_from,p_to,key,wxSTC_FIND_WHOLEWORD|wxSTC_FIND_MATCHCASE);
					if (p!=wxSTC_INVALID_POSITION) { // si es un parametro
						p_to=p;
						int template_level=0; // sino se confunde la , de un map<int,int> con el final del argumento
						while (p_to>0 && (!(II_IS_2(p_to,',','(')||(c==':'&&(GetCharAt(p_to-1)!=':'||GetCharAt(p_to+1)!=':')))||template_level)) {
							if (c=='>') template_level++;
							else if (c=='<') template_level--;
							p_to--;
						}
						p_type=p_to+1;
						p_ocur=p;
						// contar las dimensiones (asteriscos)
						p--;
						while (II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p)) {
							p--;
						}
						if (c=='&') c=GetCharAt(--p); // alias de tipo puntero (foo *&var)
						while (c=='*') {
							p--;
							dims++;
							while (II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p)) {
								p--;
							}
						}
						if (p_ocur!=p_type) break;
					}
					// buscar el scope de la funcion actual
					p=p_to-1;
					while (p>0 && ( II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p) ) ) {
						p--;
					}
					c=GetCharAt(p);
					while  ( p>0 && ( II_IS_KEYWORD_CHAR(c) || II_SHOULD_IGNORE(p) ) ) {
						c=GetCharAt(--p);
					}
					if (p>0&&c=='~') c=GetCharAt(--p); // por si era un destructor
					while (p>0 && ( II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p) ) ) {
						p--;
					}
					if (c==':' && GetCharAt(p-1)==':') {
						p-=2;
						while (p>0 && ( II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p) ) ) {
							p--;
						}
						int pend=p+1;
						p = WordStartPosition(p,true);
						if (notSpace) {
							space = GetTextRange(p,pend);
							notSpace=false;
						}
					}
				} else {
					p_from=p;
				}
			}
			
		} else	if (c=='}' && !init_pos) { // saltear todo el contenido de ese bloque
			p_from = BraceMatch(p_from);
			if (p_from==wxSTC_INVALID_POSITION)
				break;
			p_from--;
			// ver si era una funci�n, y en ese caso saltear tambien el prototipo
			II_BACK(p_from,II_IS_NOTHING_4(p_from));
			// saltear el const
			if (p_from>4&&GetCharAt(p_from-4)=='c'&&GetCharAt(p_from-3)=='o'&&GetCharAt(p_from-2)=='n'&&GetCharAt(p_from-1)=='s'&&GetCharAt(p_from)=='t'&&(GetCharAt(p_from-5)==')'||II_IS_NOTHING_4(p_from-5))) {
				p_from-=5; II_BACK(p_from,II_IS_NOTHING_4(p_from));
			}			
			if (c==')') {
				p_from = BraceMatch(p_from);
				if (p_from==wxSTC_INVALID_POSITION)
					break;
			}
		}
		
		init_pos = false;
		
		// reverse find ) or } from p_from
		int p_llave_c = FindTextEx(p_from,0,"}"), p_par_c = FindTextEx(p_from,0,")"), p_to = 0;
		if ( (p_llave_c!=wxSTC_INVALID_POSITION) != (p_par_c!=wxSTC_INVALID_POSITION) )
			p_to = p_llave_c!=wxSTC_INVALID_POSITION ? p_llave_c : p_par_c;
		else if (p_par_c!=wxSTC_INVALID_POSITION)
			p_to = p_llave_c>p_par_c ? p_llave_c : p_par_c;
		
		// reverse find the keyword in current "scope"
//		p_ocur = p = FindTextEx(p_from, p_to, key, wxSTC_FIND_WHOLEWORD|wxSTC_FIND_MATCHCASE);
		// why reverse??? in a single scope, the first one is the one
		p_ocur = p = FindTextEx(p_to,p_from, key, wxSTC_FIND_WHOLEWORD|wxSTC_FIND_MATCHCASE);
		if (p!=wxSTC_INVALID_POSITION) { // si se encuentra la palabra
			
			int p_statementstart = GetStartSimple(this,p_ocur);
			int p_typeend = GetTypeEnd(this,p_statementstart,GetLength());
			
			p = p_typeend;
			if (p!=wxSTC_INVALID_POSITION) {
				// ver si lo que sigue tiene cara de nombres de variable para la declaracion �?
				while (II_IS_NOTHING_4(p)) p++; // side-effect: setea c para el if
				if ( IsKeywordChar(c,false) || c=='&' || c=='*') {
					dims=0;
//					if (p1!=p2 && !TextRangeIs(p,p2,"else") && !TextRangeIs(p,p2,"delete")) { /// que hac�a este if???
						p=p_ocur-1;
						while (II_IS_NOTHING_4(p)) {
							p--;
						}
						if (c=='&' && GetCharAt(p-1)=='*') { c='*'; --p; } // por si es algo como "string *&p=..."
						while (c=='*') {
							p--;
							dims++;
							while (II_IS_NOTHING_4(p)) {
								p--;
							}
						}
						if (c==',' || p+1==p_typeend || c=='&') {
							p_type=p_statementstart;
							break;
						}
//					}
				}
			}
		}
		
		if (p_to==0)
			break;
		else
			p_from = p_to;
		
	}
	
	if (p_type!=wxSTC_INVALID_POSITION) { // significa que encontro donde se declara el tipo
		
		while (II_IS_NOTHING_4(p_type)) p_type++;
		
		int p_endtype = GetTypeEnd(this,p_type,GetLength());
		if (p_endtype!=mxINVALID_POS) {
			ret = GetTextRangeEx(this,p_type,p_endtype);
			if (!include_template_spec) 
				ret = StripQualifiers(StripTemplateArgs(StripNamespaces(ret)));
		}

		p_ocur+=key.Len();
		while (II_IS_NOTHING_4(p_ocur)) {
			p_ocur++;
		}
		while (GetCharAt(p_ocur)=='[') {
			dims++;
			p_ocur = BraceMatch(p_ocur);
			if (p_ocur==wxSTC_INVALID_POSITION)
				return ret;
			p_ocur++;
			while (II_IS_NOTHING_4(p_ocur)) {
				p_ocur++;
			}
		}
		pos=dims;
	} else {
		if (key=="this") {
			if ( (ret=space).Len()!=0) 
				dims=1;
		} else {
			wxString ans;
			if ( (space!="" && (ans=g_code_helper->GetAttribType(space,key,pos))!="") || (ans=g_code_helper->GetGlobalType(key,pos))!="" )
				ret=ans;
			else
				pos=SRC_PARSING_ERROR;
		}
	}
	key=space;
	return g_code_helper->UnMacro(ret,pos);
}

void mxSource::ShowBaloon(wxString str, int p) {
	if (p==-1) p = GetCurrentPos();
//	int cp = GetCurrentPos(); 
//	if (p==-1) p = cp;
//	// evitar que tape el cursor (reveer, parece solo tener sentido si se muestra en una posicion que no es la actual, cuando pasa esto?)
//	int cl = LineFromPosition(cp);
//	int l = LineFromPosition(p);
//	if (l!=cl) {
//		int dp = p-PositionFromLine(l);
//		p = PositionFromLine(cl)+dp;
//		if (LineFromPosition(p)!=cl) 
//			p = GetLineEndPosition(cl);
//	}
	int line_count = 1, max_lines = 25, spaces = 0;
	int x1 = PointFromPosition(p).x, char_w = TextWidth(0,"W");
	if (char_w<1) char_w=1;
	int l=str.Len(), ll = (GetSize().GetWidth()-x1)/(char_w)-1;
	if (ll>5) {
		int ac=0;
		for (int i = 0;i<l; i++) {
			if (str[i]=='\n') {
				ac=0; 
				if (++line_count==max_lines) {
					str = str.SubString(0,i)+"...";
					break;
				}
				spaces = 0;
				while (i+1<l&&str[i+1]==' ') { ++i; ++spaces; }
			}
			else {
				ac++;
				if (ac>ll) {
					int j=i;
					while (j && ( str[j]!=' ' && str[j]!=',' && str[j]!='(' && str[j]!=')' ) ) j--;
					if (j && !(j>2 && str[j]==' ' && str[j-1]==' ' && str[j-2]==' ' && str[j-3]=='\n') ) {
						i=j;
						str = str.SubString(0,j)+"\n"+wxString(' ',spaces+3).c_str()+str.Mid(j+1);
						++line_count;
						l+=4;
					}
				}
			}
		}
	}
	
	// mostrar utilizando el mecanismo de calltip de scintilla
	SetCalltipMode(MXS_BALOON);
	wxStyledTextCtrl::CallTipShow(p,str);
	// para que quer�amos un yield aca???
	ZLINF("Source","ShowBaloon wxYield:in");
	wxYield();
	ZLINF("Source","ShowBaloon wxYield:out");
}
	
wxString mxSource::FindTypeOfByPos(int p,int &dims, bool include_template_spec, bool first_call) {
	if (first_call)
		dims=0;
	int s; char c;
	// buscar donde comienza la expresion
	II_BACK(p,II_IS_NOTHING_4(p));
	
	while ( p>=0 && ( II_IS_NOTHING_2(p) || c==']' ) )  {
		if (c==']') {
			dims--;
			p = BraceMatch(p);
			if (p==wxSTC_INVALID_POSITION) {
				dims=SRC_PARSING_ERROR;
				return "La cantidad de corchetes no concuerda.";
			}
		}
		p--;
	}
	if (GetCharAt(p-3)=='t' && GetCharAt(p-2)=='h' && GetCharAt(p-1)=='i' && GetCharAt(p)=='s' && WordStartPosition(p-3,true)==p-3) {
		wxString scope = FindScope(p-3);
		if (scope.Len()) {
			dims++;
			return scope;
		}
	}
	if (c==')') { // si hay un parentesis puede que ya est� todo resuelto (cast)
		p = BraceMatch(p);
		if (p==wxSTC_INVALID_POSITION) {
			dims=SRC_PARSING_ERROR;
			return "Error: Se cerraron parentesis demas.";
		}
		int p2 = p-1;
		II_BACK(p2,II_IS_NOTHING_4(p2));
		if (c=='>') { // si habia un cast estilo c++
			// buscar hasta donde llegan los signos mayor y menor
			int cont=1; p=--p2;
			while (p2>0 && (c!='<' || cont!=0) ) {
				c=GetCharAt(p2);
				if (c=='>')
					cont++;
				else if (c=='<') 
					cont--;
				p2--;
			}
			if (c=='<' && cont==0) { // si se delimito correctamente
				// contar los asteriscos para informar las dimensiones
				p2++;
				while (p>p2 && ( II_IS_NOTHING_2(p) || c=='*') ) {
					if (c=='*')
						dims++;
					p--;
				}
				return g_code_helper->UnMacro(GetTextRange(p2+1,p+1),dims);
			}
		} else { 
			// puede ser un cast al estilo c: ejemplo ((lala)booga) y estabamos parados en el primer (, el tipo seria lala
			// o una llamada a metodo/funcion/constructor: ejemplo foo(bar) y estamos en el (
			// ver primero si es la segunda opcion, mirando que hay justo antes del (
			int p_par=p;
			p--; II_BACK(p, II_IS_NOTHING_4(p)); // ir a donde termina el nombre de la funcion
			int p_fin=p+1;
			// ir a donde empieza el nombre de la funcion
			p=WordStartPosition(p,true); c=GetCharAt(p);
			if (p<p_fin && (c<'0'||c>'9')) { // si puede ser un nombre de funcion o metodo
				int p0_name=p; wxString func_name=GetTextRange(p,p_fin), scope;
				--p; II_BACK(p,II_IS_NOTHING_4(p));
				if (GetCharAt(p)=='.') {
					--p; II_BACK(p,II_IS_NOTHING_4(p));
					scope=FindTypeOfByPos(p,dims,include_template_spec);
				} else if (p&& GetCharAt(p)==':' && GetCharAt(p-1)==':') {
					p-=2; II_BACK(p,II_IS_NOTHING_4(p));
					scope=GetTextRange(WordStartPosition(p,true),p+1);
				} else if (GetCharAt(p)=='>' && p && GetCharAt(p-1)=='-') {
					p-=2; II_BACK(p,II_IS_NOTHING_4(p));
					scope=FindTypeOfByPos(p,dims,include_template_spec); dims--;
				} else {
					scope = FindScope(p+1);
				}
				wxString type=g_code_helper->GetCalltip(scope,func_name,false,true);
				if (!type.Len()) { // sera sobrecarga del operator() ?
					type=FindTypeOfByKey(func_name,p0_name,include_template_spec);
					type=g_code_helper->GetCalltip(type,"operator()",true,true);
				}
				if (type.Len()) {
					while(type.Last()=='*') { dims++; type.RemoveLast(); }
					return type;
				}
			}
			
			// si no paso nada con metodo/funcion, probar si era cast
			p=p_par+1;
			II_FRONT_NC(p, II_IS_NOTHING_4(p));
			if (c=='(') {
				int p2=BraceMatch(p);
				p2++;
				II_FRONT_NC(p2,II_IS_NOTHING_4(p2));
				if (c=='(' || ((c|32)>='a' && (c|32)<='z') || c=='_' ) {
					int p2=BraceMatch(p)-1;
					while (p2>p && ( II_IS_NOTHING_2(p2) || c=='*') ) {
						if (c=='*')
							dims++;
						p2--;
					}
					return g_code_helper->UnMacro(GetTextRange(p+1,p2+1),dims);
				}
			}
				
		}
		
	} else { // si la cosa no estaba entre parentesis, podemos encontrarnos con indices entre corchetes o el nombre
		II_BACK(p, (II_IS_NOTHING_2(p) || II_IS_KEYWORD_CHAR(c)) && s!=wxSTC_C_WORD );
		p++;
	}
	if (GetCharAt(p)=='(')
		p++;
	while (II_IS_2(p,'&','*') || II_IS_NOTHING_4(p)) {
		if (!II_IS_NOTHING_4(p)) {
			if (c=='*')
				dims--;
			else if (c=='&')
				dims++;
		}
		p++;
	}
	if (c=='(') {
		p=BraceMatch(p);
		if (p==wxSTC_INVALID_POSITION) {
			dims=SRC_PARSING_ERROR;
			return "";
		}
		wxString ans = FindTypeOfByPos(p,dims,include_template_spec,false);
		return ans;
	} else {
		if ( ( (c|32)>='a' && (c|32)<='z' ) || c=='_' ) {
			s=p;
			wxString key=GetTextRange(p,WordEndPosition(p,true));
			wxString ans=FindTypeOfByKey(key,s,include_template_spec);
			if (ans.Len() && dims<0 && s==0) {
				wxString type=g_code_helper->GetCalltip(ans,"operator[]",true,true);				
				if (type.Len()) { 
					while(type.Last()=='*') { dims++; type.RemoveLast(); }
					dims++; ans=type;
				}
			}
			if (s==SRC_PARSING_ERROR) {
				dims=SRC_PARSING_ERROR;
				return "";
			}
			c = GetCharAt(p-1); if (c=='*') dims--; else if (c=='&') dims++;
			dims+=s;
			return ans;
		}
	}
	dims=SRC_PARSING_ERROR;
	return "";
}

/**
* @brief determina en qu� funci�n o clase est�mos actualmente
* 
* Se usa para saber el scope con el que consultar al �ndice de autocompletado,
* y para mostrar el contexto (WhereAmI). El primer uso va con full_scope en falso
* para que retorna nada m�s que el nombre del scope (Ej: "mi_clase"), mientras que
* el segundo va con full_scope en true para que incluya el nombre del m�todo
* (Ej: "mi_clase::mi_metodo") o el tipo de definicion (Ej: "class mi_clase").
* Los argumentos se retornan por separado porque se usan en los dos casos: en el
* primero para obtener los identificadores para usarlos al autocompletar, en el 
* segundo para mostrarlos como est�n. Se retornan como posiciones del c�digo
* que son las de los par�ntesis. Se retornan as� y no como wxString para poder
* analizarlos en el c�digo y utilizar as� el coloreado como ayuda. Si no hay
* argumentos args no se modifica, por lo que deber�a entrar con valores
* inv�lidos (como {-1,-1}) para saber desde afuera si el valor de retorno es real.
*
* En scope_start retorna las pos en algun punto del prototipo, o donde empieza la
* clase... Si es local_start==true es la funci�n/m�todo, sino puede ser el scope
* de la clase... (para el autocompletado necesito el de la clase, para refactory y 
* otras yerbas puedo necesitar el del m�todo. Ojo que la pos puede no ser
* el comienzo, sino estar a mitad del prototipo... Usar GetStatementStartPos para 
* tener el verdadero comienzo.
**/ 
wxString mxSource::FindScope(int pos, wxString *args, bool full_scope, int *scope_start, bool local_start) {
	if (scope_start) *scope_start=0;
	wxString scope, type;
	int l=pos,s;
	int first_p=-1; // guarda el primer scope, para buscar argumentos si es funcion
	char c;
	while (true) {
		int p_llave_a = FindText(pos,0,"{");
		while (p_llave_a!=wxSTC_INVALID_POSITION && II_SHOULD_IGNORE(p_llave_a))
			p_llave_a = FindText(p_llave_a-1,0,"{");
		int p_llave_c = FindText(pos,0,"}");
		while (p_llave_c!=wxSTC_INVALID_POSITION && II_SHOULD_IGNORE(p_llave_c))
			p_llave_c = FindText(p_llave_c-1,0,"}");
		if (p_llave_c==wxSTC_INVALID_POSITION && p_llave_a==wxSTC_INVALID_POSITION) {
			break;
		} else if (p_llave_c!=wxSTC_INVALID_POSITION && (p_llave_a==wxSTC_INVALID_POSITION || p_llave_c>p_llave_a) ) {
			pos=BraceMatch(p_llave_c);
			if (pos==wxSTC_INVALID_POSITION)
				break;
			else
				pos--;
		} else if (p_llave_a!=wxSTC_INVALID_POSITION && (p_llave_c==wxSTC_INVALID_POSITION || p_llave_c<p_llave_a) ) {
			int p=pos=p_llave_a-1;
			II_BACK(p,II_IS_NOTHING_4(p));
			if (p>4&&GetCharAt(p-4)=='c'&&GetCharAt(p-3)=='o'&&GetCharAt(p-2)=='n'&&GetCharAt(p-1)=='s'&&GetCharAt(p)=='t'&&(GetCharAt(p-5)==')'||II_IS_NOTHING_4(p-5))) {
				p-=5; II_BACK(p,II_IS_NOTHING_4(p)); // saltear ("const")
			}
			if (c==')') { // puede ser funcion
				int op=p; // p can be first_p
				p=BraceMatch(p);
				// sigue como antes
				if (p!=wxSTC_INVALID_POSITION) {
					p--;
					II_BACK(p,II_IS_NOTHING_4(p));
					op=p; first_p=p+1;
					p=WordStartPosition(p,true)-1;
					if (scope_start) *scope_start=p;
					if (full_scope) scope=GetTextRange(p+1,op+1); // nombre de un m�todo?
					II_BACK(p,II_IS_NOTHING_4(p));
					// el "GetCharAt(p)==','" se agrego el 29/09 para los constructores en constructores
					if (GetCharAt(p)==':' || GetCharAt(p)==',' || (p && GetCharAt(p)=='~' && GetCharAt(p-1)==':')) {
						if (GetCharAt(p)=='~') { p--; if (full_scope) scope=wxString("~")+scope; } // agregado para arreglar el scope de un destructor
						if (GetCharAt(p-1)==':') {
							p-=2;
							II_BACK(p,II_IS_NOTHING_4(p));
							wxString aux = g_code_helper->UnMacro(GetTextRange(WordStartPosition(p,true),p+1)); // nombre de la clase?
							if (scope_start) *scope_start=p;
							if (full_scope) scope=aux+"::"+scope; else { scope=aux; break; }
						} else { // puede ser constructor
							p = FindText(p,0,"::");
							if (p!=wxSTC_INVALID_POSITION) {
								int e=p+2;
								p--;
								II_BACK(p,II_IS_NOTHING_4(p));
								II_FRONT_NC(e,II_IS_NOTHING_4(e));
								if (GetTextRange(WordStartPosition(p,true),p+1)==GetTextRange(e,WordEndPosition(e,true))) {
									wxString aux=g_code_helper->UnMacro(GetTextRange(WordStartPosition(p,true),p+1)); // nombre de la clase?
									if (scope_start) *scope_start=p;
									if (full_scope) scope=aux+"::"+aux; else { scope=aux; break; }
								}
							}
						}
					}
				}
			} else { // puede ser clase o struct
				p=GetStatementStartPos(p,false,true,true);
				if (GetStyleAt(p)==wxSTC_C_WORD) {
					bool some=false;
					if (GetTextRange(p,p+8)=="template") {
						p+=8; II_FRONT(p,II_IS_NOTHING_4(p)); 
						if (c=='<') { 
							p = SkipTemplateSpec(this,p,l)+1;
							II_FRONT(p,II_IS_NOTHING_4(p)); 
						}
					}
					if (GetTextRange(p,p+6)=="struct")
					{ if (!type.Len()) type="struct"; p+=6; some=true; }
					else if (GetTextRange(p,p+5)=="class")
					{ if (!type.Len()) type="class"; p+=5; some=true; }
					else if (GetTextRange(p,p+p)=="namespace")
					{ if (!type.Len()) type="namespace"; p+=9; some=true; }
					if (some) {
						II_FRONT(p,II_IS_NOTHING_4(p));
						wxString aux=g_code_helper->UnMacro(GetTextRange(p,WordEndPosition(p,true)));
						if (scope_start && !local_start) *scope_start=p;
						if (full_scope) scope=aux+"::"+scope; else { scope=aux; break; }
					}
				}
			}
		}
	}
	if (full_scope && scope.EndsWith("::")) { // si no es metodo ni funcion
		scope=scope.Mid(0,scope.Len()-2);
		scope=type+" "+scope;
	} else if (first_p!=-1 && args) { // sino agregar argumentos
		int p=first_p;
		II_FRONT(p,II_IS_NOTHING_4(p));
		if (GetCharAt(p)=='(') {
			int p2=BraceMatch(p);
			if (p2!=wxSTC_INVALID_POSITION) {
				// los argumentos se agregan medio formateados (sin dobles espacios ni saltos de linea, y sin comentarios)
				bool prev_was_space;
				for(int i=p;i<=p2;i++) {
					if (!II_IS_COMMENT(i)) {
						char c=GetCharAt(i);
						if (c==' '||c=='\t'||c=='\n'||c=='\r') {
							if (!prev_was_space) { (*args)<<' '; prev_was_space=true; }
						} else { 
							prev_was_space=false; (*args)<<c;
						}
					}
				}
			}
		}
	}
	return scope;
}

void mxSource::OnToolTipTimeOut (wxStyledTextEvent &event) {
	HideCalltip();
}

void mxSource::OnToolTipTime (wxStyledTextEvent &event) {
//	if (!FindFocus() || main_window->notebook_sources->GetSelection()!=main_window->notebook_sources->GetPageIndex(this)) // no mostrar tooltips si no se tiene el foco
//	if (FindFocus()!=this) // no mostrar tooltips si no se tiene el foco
//		return;
	
	static int old_p = -1, count_p = 0;
	
	// no mostrar tooltips si no es la pesta�a del notebook seleccionada, o el foco no esta en esta ventana
	if (!main_window->IsActive() || main_window->focus_source!=this) return; 
	
	// no mostrar si el mouse no est� dentro del area del fuente
	wxRect psrc = GetScreenRect();
	wxPoint pmouse = wxGetMousePosition()-psrc.GetTopLeft();
	if (pmouse.x<0||pmouse.y<0) { old_p = -1; return; }
	if (pmouse.x>=psrc.GetWidth()||pmouse.y>=psrc.GetHeight()) { old_p = -1; return; }
	
	int p = event.GetPosition(); 
	if (old_p==p) { ++count_p; } else { old_p = p; count_p = 0; }
	
	// si esta en un margen....
	if (p==-1) {
		int x=event.GetX(), y=event.GetY();
		if (GetMarginForThisX(x)==MARGIN_BREAKS) {
			x=10; for(int i=0;i<MARGIN_NULL;i++) { x+=GetMarginWidth(i); }
			int l = LineFromPosition( PositionFromPointClose(x,y) );
			// error/warning
			if (MarkerGet(l)&((1<<mxSTC_MARK_ERROR)|(1<<mxSTC_MARK_WARNING))) {
				wxString errors_message = m_cem_ref.GetMessageForLine(this,l);
				if (!errors_message.IsEmpty()) {
					ShowBaloon(errors_message,PositionFromLine(l));
					return;
				}
			}
			// breakpoint annotation
			BreakPointInfo *bpi=m_extras->FindBreakpointFromLine(this,l);
			if (bpi && bpi->annotation.Len()) {
				ShowBaloon(bpi->annotation,PositionFromLine(l));
				return;
			}
		}
		return;
	}
	
	if (++count_p<_DWEEL_FACTOR_) { SetMouseDwellTime(_DWEEL_TIME_); return; } // SetMouseDwellTime is used to relaunch that when mouse doesn't move
	
	// si no esta depurando, buscar el tipo de dato del simbolo y mostrarlo
	if (!debug->IsDebugging()) {
		if (!config_source.toolTips)
			return;
		int s;
		if (II_SHOULD_IGNORE(p)) 
			return;
		int e = WordEndPosition(p,true);
		s = WordStartPosition(p,true);
		if (s!=e) {
			wxString key = GetTextRange(s,e);
			// buscar en la funcion/metodo
			wxString bkey=key, type = FindTypeOfByPos(e-1,s,true);
			if ( s!=SRC_PARSING_ERROR && type.Len() ) {
				while (s>0) {
					type<<_("*");
					s--;
				}
				ShowBaloon ( bkey +": "+type , p );
			} else { // buscar el scope y averiguar si es algo de la clase
				type = FindScope(s);
				if (type.Len()) {
					type = g_code_helper->GetAttribType(type,bkey,s);
					if (!type.Len())
						type=g_code_helper->GetGlobalType(bkey,s);
				} else {
					type=g_code_helper->GetGlobalType(bkey,s);
				}
				if (type.Len()) {
					while (s>0) {
						type<<_("*");
						s--;
					}
					ShowBaloon ( bkey +": "+type , p );
				}
			}
		}
	} else 
	// si se esta depurando, evaluar la inspeccion	
	if (debug->IsPaused()) {
		if (!config->Debug.inspect_on_mouse_over) return;
		int e = GetSelectionEnd();
		int s = GetSelectionStart();
		if (e==s || p<s || p>e) {
			e = WordEndPosition(p,true);
			s = WordStartPosition(p,true);
		}
		if (s!=e && LineFromPosition(s)==LineFromPosition(e)) {
			while ( s>2 && (GetTextRange(s-1,s)=="." || GetTextRange(s-2,s)=="->" || GetTextRange(s-2,s)=="::")) {
				int s2=s-1; if (GetTextRange(s-1,s)!=".") s2--;
				int s3 = WordStartPosition(s2,true);
				if (s3<s2) s=s3; else break; // que significaria el caso del break?? (sin el break podria ser loop infinito)
			}
			wxString key = GetTextRange(s,e);
			wxString ans = debug->InspectExpression(key);
			if (ans.Len() && ans!=key) {
				wxRect r=GetScreenRect();
				int x=event.GetX()+r.GetLeft(),y=event.GetY()+r.GetTop();
				ShowInspection(wxPoint(x,y),key,ans);
//				ShowBaloon ( key +": "+ ans , p ); // old method
			}
		}
	}
}

void mxSource::MakeUntitled(wxString ptext) {
	SetPageText(ptext);
	sin_titulo=true;
}

void mxSource::SetPageText(wxString ptext) {
	main_window->notebook_sources->SetPageText(main_window->notebook_sources->GetPageIndex(this),page_text=ptext);
}

void mxSource::OnSavePointReached (wxStyledTextEvent &event) {
	main_window->notebook_sources->SetPageText(main_window->notebook_sources->GetPageIndex(this),page_text);
}

void mxSource::OnSavePointLeft (wxStyledTextEvent &event) {
	main_window->notebook_sources->SetPageText(main_window->notebook_sources->GetPageIndex(this),page_text+"*");
}

void mxSource::OnKillFocus(wxFocusEvent &event) {
	if (!focus_helper.KillIsMasked()) HideCalltip(); 
	event.Skip();
}

void mxSource::OnSetFocus(wxFocusEvent &event) {
	ro_quejado=false;
	if (main_window) {
		if (main_window->focus_source!=this) {
			g_navigation_history.OnFocus(this);
			main_window->focus_source=this;
		}
	}
	event.Skip();
	if (!sin_titulo) CheckForExternalModifications();
}

void mxSource::OnEditAutoCompleteAutocode(wxCommandEvent &evt) {
	HideCalltip();
	int p=GetCurrentPos();
	int ws=WordStartPosition(p,true);
	g_code_helper->AutocompleteAutocode(this,GetTextRange(ws,p));
}


void mxSource::OnEditForceAutoComplete(wxCommandEvent &evt) {
	int p=GetCurrentPos();
	char chr = p>0?GetCharAt(p-1):' ';
	HideCalltip();
	int s=GetStyleAt(--p); char &c=chr;
	II_BACK(p,II_IS_NOTHING_4(p)); p++; // por si estamos en una linea en blanco, y hay que completar algo que viene de la anterior (como una , en una lista de argumentos para una funci�n)
	if (s==wxSTC_C_PREPROCESSOR) {
		int ws=WordStartPosition(p,true);
		if (chr=='#')
			g_code_helper->AutocompletePreprocesorDirective(this);
		else if (GetCharAt(ws-1)=='#')
			g_code_helper->AutocompletePreprocesorDirective(this,GetTextRange(ws,p));
		return;
	} else if (s==wxSTC_C_COMMENTDOC || s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR) {
		int ws=WordStartPosition(p,true);
		if (chr=='@' || chr=='\\')
			g_code_helper->AutocompleteDoxygen(this);
		else if (GetCharAt(ws-1)=='\\' || GetCharAt(ws-1)=='@')
			g_code_helper->AutocompleteDoxygen(this,GetTextRange(ws,p));
		return;
	}
	
	if (chr==':' && p>1 && GetCharAt(p-2)==':') {
		p-=3;
		int s; char c;
		II_BACK(p,II_IS_NOTHING_2(p));
		if ((c=GetCharAt(p))=='>') {
			int lc=1;
			while (p-- && lc) {
				c=GetCharAt(p);
				if (c=='>' && !II_IS_NOTHING_4(p)) lc++;
				else if (c=='<' && !II_IS_NOTHING_4(p)) lc--;
			}
			II_BACK(p,II_IS_NOTHING_2(p));
			if (p<0) p=0;
		}
		wxString key=GetTextRange(WordStartPosition(p,true),WordEndPosition(p,true));
		if (key.Len()!=0) {
			if (!g_code_helper->AutocompleteScope(this,key,"",false,false)
				&& (!g_code_helper->IsOptionalNamespace(key)||!g_code_helper->AutocompleteGeneral(this,key,"")) )
					ShowBaloon(wxString(LANG(SOURCE_NO_ITEMS_FOR_AUTOCOMPLETION,"No se encontraron elementos para autocompletar el ambito "))<<key);
		} else
			ShowBaloon(LANG(SOURCE_UNDEFINED_SCOPE_AUTOCOMPLETION,"No se pudo determinar el ambito a autocompletar"));
	} else if (chr=='>' && p>1 && GetCharAt(p-2)=='-') {
		p-=2;
		int dims;
		wxString type = FindTypeOfByPos(p-1,dims);
		if (dims==SRC_PARSING_ERROR) {
			ShowBaloon(LANG(SOURCE_UNDEFINED_SCOPE_AUTOCOMPLETION,"No se pudo determinar el ambito a autocompletar"));
		} else if (dims==1) {
			if (!g_code_helper->AutocompleteScope(this,type,"",true,false))
				ShowBaloon(wxString(LANG(SOURCE_NO_ITEMS_FOR_AUTOCOMPLETION,"No se encontraron elementos para autocompletar el ambito "))<<type);
		} else if (type.Len()!=0 && dims==0) {
			ShowBaloon(LANG(SOURCE_TIP_NO_DEREFERENCE,"Tip: Probablemente no deba desreferenciar este objeto."));
		} else
			ShowBaloon(LANG(SOURCE_UNDEFINED_SCOPE_AUTOCOMPLETION,"No se pudo determinar el ambito a autocompletar"));
	} else if (chr=='.') {
		bool shouldComp=true;
		char c;
		int s,p=GetCurrentPos()-2;
		II_BACK(p,II_IS_NOTHING_4(p));
		if ((c=GetCharAt(p))>='0' && c<='9') {
			while ( (c=GetCharAt(p))>='0' && c<='9' ) {
				p--;
			}
			shouldComp = (c=='_' || ( (c|32)>='a' && (c|32)<='z' ) );
		}
		if (shouldComp) {
			int dims;
			wxString type = FindTypeOfByPos(p,dims);
			if (dims==SRC_PARSING_ERROR) {
				ShowBaloon(LANG(SOURCE_UNDEFINED_SCOPE_AUTOCOMPLETION,"No se pudo determinar el ambito a autocompletar"));
			} else	if (dims==0) {
				if (!g_code_helper->AutocompleteScope(this,type,"",true,false))
					ShowBaloon(wxString(LANG(SOURCE_NO_ITEMS_FOR_AUTOCOMPLETION,"No se encontraron elementos para autocompletar el ambito "))<<type);
			} else if (type.Len()!=0 && dims>0)
				ShowBaloon(LANG(SOURCE_TIP_DO_DEREFERENCE,"Tip: Probablemente deba desreferenciar este objeto."));
			else
				ShowBaloon(LANG(SOURCE_UNDEFINED_SCOPE_AUTOCOMPLETION,"No se pudo determinar el ambito a autocompletar"));
		} else 
			ShowBaloon(LANG(SOURCE_CANT_AUTOCOMPLETE_HERE,"No se puede autocompletar en este punto"));
	} else {
		int s,e=GetCurrentPos()-1,ctp=0; // el cero en ctp es solo para silenciar un warning
		char c;
		if (chr=='(') {
			ctp=e;
			e--;
			II_BACK(e,II_IS_NOTHING_4(e));
		} else if (chr==',') {
			int p=e;
			c=GetCharAt(--p);
			while (true) {
				II_BACK(p, !II_IS_5(p,';','}','{','(',')') || II_SHOULD_IGNORE(p));
				if (c=='(') {
					e=p-1;
					II_BACK(e,II_IS_NOTHING_4(e));
					if (calltip_brace==p && calltip_mode==MXS_CALLTIP)
						return;
					ctp=p;
					chr='(';
					break;
				} else if (c==')') {
					if ( (p=BraceMatch(p))==wxSTC_INVALID_POSITION )
						break;
					p--;
				} else
					break;
			}
		}
		int p=WordStartPosition(e,true);
		wxString key = GetTextRange(p,e+1);
		if (p && GetCharAt(p-1)=='#' && GetStyleAt(p-1)==wxSTC_C_PREPROCESSOR) {
			g_code_helper->AutocompletePreprocesorDirective(this,key);
			return;
		}
		if ((e-p+1 && key.Len()>1) || !(key[0]==' ' || key[0]=='\t' || key[0]=='\n' || key[0]=='\r')) {
			int dims;
			p--;
			II_BACK(p,II_IS_NOTHING_4(p));
			if (c=='.') {
				wxString type = FindTypeOfByPos(p-1,dims);
				if (dims==0) {
					if (chr=='('/* && config_source.callTips*/)
						g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
					else if ( II_IS_KEYWORD_CHAR(chr) )
						g_code_helper->AutocompleteScope(this,type,key,true,false);
				}
			} else if (c=='>' && GetCharAt(p-1)=='-') {
				p--;
				wxString type = FindTypeOfByPos(p-1,dims);
				if (dims==1) {
					if (chr=='('/* && config_source.callTips*/)
						g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
					else if ( II_IS_KEYWORD_CHAR(chr) )
						g_code_helper->AutocompleteScope(this,type,key,true,false);
				}
			} else if (c==':' && GetCharAt(p-1)==':') {
				p-=2;
				II_BACK(p,II_IS_NOTHING_4(p));
				wxString type = GetTextRange(WordStartPosition(p,true),p+1);
				if (chr=='('/* && config_source.callTips*/)
					g_code_helper->ShowFunctionCalltip(ctp,this,type,key);
				else if ( II_IS_KEYWORD_CHAR(chr) )
					g_code_helper->AutocompleteScope(this,type,key,true,false);
			} else {
				if (chr=='('/* && config_source.callTips*/) {
					if (!g_code_helper->ShowFunctionCalltip(ctp,this,FindScope(GetCurrentPos()),key,false)) {
						// mostrar calltips para constructores
						p=ctp-1;
						bool f;
						while ( p>0 && ( !II_IS_3(p,'(','{',';') || (f=II_SHOULD_IGNORE(p))) ) {
							if (!f && c==')' && (p=BraceMatch(p))==wxSTC_INVALID_POSITION)
								return;
						p--;
						}
						p++;
						II_FRONT_NC(p,II_IS_NOTHING_4(p) || II_SHOULD_IGNORE(p) || (s==wxSTC_C_WORD));
						int p1 = p;
						II_FRONT_NC(p,(c=GetCharAt(p))=='_' || II_IS_KEYWORD_CHAR(c) );
						wxString key = GetTextRange(p1,p);
						if (!g_code_helper->ShowConstructorCalltip(ctp,this,key)) {
							// mostrar sobrecarga del operador()
							wxString type = FindTypeOfByKey(key,p1);
							if (type.Len()) {
								g_code_helper->ShowFunctionCalltip(ctp,this,type,"operator()",true);
							}
						}
					}
				} else if ( II_IS_KEYWORD_CHAR(chr) ) {
					wxString args; int scope_start;
					wxString scope = FindScope(GetCurrentPos(),&args, false,&scope_start);
					g_code_helper->AutocompleteGeneral(this,scope,key,&args,scope_start);
				}
			}
		} else {
			wxString scope = FindScope(GetCurrentPos());
			if (scope.Len())
				g_code_helper->AutocompleteScope(this,scope,"",true,true);
		}
	}
	if (calltip_mode==MXS_NULL)	ShowBaloon(LANG(SOURCE_NO_ITEMS_FOR_AUTOCOMPLETION,"No se encontraron opciones para autocompletar"),p);
	else if (calltip_mode==MXS_CALLTIP) UpdateCalltipArgHighlight(GetCurrentPos());
}

DiffInfo *mxSource::MarkDiffs(int from, int to, MXS_MARKER marker, wxString extra) {
	if (mxSTC_MARK_DIFF_NONE==marker) {
		for (int i=from;i<=to;i++) {
			MarkerDelete(i,mxSTC_MARK_DIFF_ADD);	
			MarkerDelete(i,mxSTC_MARK_DIFF_DEL);	
			MarkerDelete(i,mxSTC_MARK_DIFF_CHANGE);	
		}
		while (first_diff_info) delete first_diff_info;
		return nullptr;
	} else {
		int *handles = new int[to-from+1];
		for (int i=from;i<=to;i++)
			handles[i-from]=MarkerAdd(i,marker);
		return new DiffInfo(this,handles,to-from+1,marker,extra);
	}
}

/// @brief looks for the symbol under the cursor in the symbols tress (functions/methods/classes) and goes to its definition (or opens mxGotoFunctionDialog if there are options)
void mxSource::JumpToCurrentSymbolDefinition() {
	int pos = GetCurrentPos();
	int s=WordStartPosition(pos,true);
	int e=WordEndPosition(pos,true);
	wxString key = GetTextRange(s,e);
	if (key.Len()) new mxGotoFunctionDialog(key,main_window,GetFileName(false));
}

void mxSource::OnClick(wxMouseEvent &evt) {
	if (evt.ControlDown()) {
		int p=PositionFromPointClose(evt.GetX(),evt.GetY());
		SetSelectionStart(p); SetSelectionEnd(p);
		JumpToCurrentSymbolDefinition();
//	} else if (evt.AltDown()) {
//		int pos = PositionFromPointClose(evt.GetX(),evt.GetY());
//		int s=WordStartPosition(pos,true);
//		int e=WordEndPosition(pos,true);
//		wxString key = GetTextRange(s,e);
//		if (key.Len()!=0) { // puede ser una directiva de preprocesador
//			if (GetCharAt(s-1)=='#')
//				key = GetTextRange(s-1,e);
//		}
//		if (key.Len()) main_window->ShowQuickHelp(key); 
	} else {
		wxPoint point=evt.GetPosition();
		int ss=GetSelectionStart(), se=GetSelectionEnd(), p=PositionFromPointClose(point.x,point.y);
		if ( p!=wxSTC_INVALID_POSITION && ss!=se && ( (p>=ss && p<se) || (p>=se && p<ss) ) ) {
//			MarkerDelete(current_line,mxSTC_MARK_CURRENT);
			wxTextDataObject my_data(GetSelectedText());
			wxDropSource dragSource(this);
			dragSource.SetData(my_data);
			mxDropTarget::current_drag_source=this;
			mxDropTarget::last_drag_cancel=false;
			wxDragResult result = dragSource.DoDragDrop(wxDrag_AllowMove|wxDrag_DefaultMove);
			if (mxDropTarget::current_drag_source!=nullptr && result==wxDragMove) {
				mxDropTarget::current_drag_source=nullptr;
				SetTargetStart(ss); SetTargetEnd(se); ReplaceTarget("");
			} 
			else if (result==wxDragCancel && ss==GetSelectionStart()) {
				DoDropText(evt.GetX(),evt.GetY(),""); // para evitar que se congele el cursor
				SetSelection(p,p);
//				evt.Skip();
			} else {
				DoDropText(evt.GetX(),evt.GetY(),"");
			}
		} else
			evt.Skip();
	}
}
	
void mxSource::LoadSourceConfig() {
	config_source=config->Source;
	// set spaces and indention
	if (project) {
		config_source.tabWidth = project->tab_width;
		config_source.tabUseSpaces = project->tab_use_spaces;
	}
	SetTabWidth (config_source.tabWidth);
	SetUseTabs (!config_source.tabUseSpaces);
	SetIndent(0);
	SetTabIndents (true);
	SetBackSpaceUnIndents (true);
	SetIndent (config_source.indentEnable? config_source.tabWidth: 0);
	SetIndentationGuides(true);
	SetLineNumbers();
	SetMarginWidth (MARGIN_FOLD, config_source.foldEnable? 15: 0);
	if (config_source.edgeColumn>0) SetEdgeColumn(config_source.edgeColumn);
	SetEdgeMode(config_source.edgeColumn>0?wxSTC_EDGE_LINE: wxSTC_EDGE_NONE);
}

void mxSource::SetLineNumbers() {
	// establecer el margen para los nros de lineas
	wxString ancho("  X");
	int lnct=GetLineCount();
	while (lnct>10) {
		ancho+="X";
		lnct/=10;
	}
	SetMarginWidth (MARGIN_LINENUM, config_source.lineNumber?TextWidth (wxSTC_STYLE_LINENUMBER, ancho):0);
	SetViewWhiteSpace(config_source.whiteSpace?wxSTC_WS_VISIBLEALWAYS:wxSTC_WS_INVISIBLE);
	SetViewEOL(config_source.whiteSpace);
}

void mxSource::SetDiffBrother(mxSource *source) {
	diff_brother=source;
}

void mxSource::ApplyDiffChange() {
	int cl=GetCurrentLine();
	DiffInfo *di = first_diff_info;
	UndoActionGuard undo_action(this);
	while (di) {
		for (int i=0;i<di->len;i++) {
			if (MarkerLineFromHandle(di->handles[i])==cl) {
				if (di->marker==mxSTC_MARK_DIFF_ADD) {
					for (int i=0;i<di->len;i++) {
						cl = MarkerLineFromHandle(di->handles[i]);
						MarkerDeleteHandle(di->handles[i]);
						SetSelection(PositionFromLine(cl),PositionFromLine(cl));
						LineDelete();
					}
					if (diff_brother) diff_brother->MarkerDeleteHandle(di->brother->handles[0]);
				} else if (di->marker==mxSTC_MARK_DIFF_DEL) {
					MarkerDeleteHandle(di->handles[0]);
					InsertText(PositionFromLine(cl),di->extra<<"\n");
					if (diff_brother)
						for (int i=0;i<di->brother->len;i++)
							diff_brother->MarkerDeleteHandle(di->brother->handles[i]);
				} else if (di->marker==mxSTC_MARK_DIFF_CHANGE) {
					int ocl=MarkerLineFromHandle(di->handles[0]);
					MarkerDeleteHandle(di->handles[0]);
					for (int i=1;i<di->len;i++) {
						cl = MarkerLineFromHandle(di->handles[i]);
						MarkerDeleteHandle(di->handles[i]);
						SetSelection(PositionFromLine(cl),PositionFromLine(cl));
						LineDelete();
					}
					if (diff_brother)
						for (int i=0;i<di->brother->len;i++)
							diff_brother->MarkerDeleteHandle(di->brother->handles[i]);
					SetTargetStart(PositionFromLine(ocl));
					SetTargetEnd(GetLineEndPosition(ocl));
					ReplaceTarget(di->extra);
				}
				if (diff_brother) {
					delete di->brother;
					diff_brother->Refresh();
				}
				delete di; di=nullptr; break;
				
			}
		}
		if (di) di = di->next;
	}
	Refresh();
}

void mxSource::DiscardDiffChange() {
	int cl=GetCurrentLine();
	DiffInfo *di = first_diff_info;
	UndoActionGuard undo_action(this);
	while (di) {
		for (int i=0;i<di->len;i++) {
			if (MarkerLineFromHandle(di->handles[i])==cl) {
				for (int i=0;i<di->len;i++)
					MarkerDeleteHandle(di->handles[i]);
				if (diff_brother) {
					for (int i=0;i<di->brother->len;i++)
						diff_brother->MarkerDeleteHandle(di->brother->handles[i]);
					diff_brother->Refresh();
				}
				if (diff_brother) {
					delete di->brother;
					diff_brother->Refresh();
				}
				delete di; di=nullptr; break;
			}
		}
		if (di) di = di->next;
	}
	Refresh();
}

void mxSource::GotoDiffChange(bool forward) {
	int cl=GetCurrentLine();
	DiffInfo *di = first_diff_info;
	int lmin=-1,bmin=-1;
	while (di) {
		bool in=false;
		for (int i=0;i<di->len;i++) {
			if (MarkerLineFromHandle(di->handles[i])==cl) {
				in=true; break;
			}
		}
		int lb=MarkerLineFromHandle(di->handles[0]);
		if (!in) {
			if (forward) {
				if (lb>cl&&(lmin==-1||lb<lmin)) {
					if (diff_brother && di->bhandles) bmin=diff_brother->MarkerLineFromHandle(di->bhandles[0]);
					lmin=lb;
				}
			} else {
				if (lb<cl&&(lmin==-1||lb>lmin)) {
					if (diff_brother && di->bhandles) bmin=diff_brother->MarkerLineFromHandle(di->bhandles[0]);
					lmin=lb;
				}
			}
		}
		if (di) di = di->next;
	}
	if (lmin>=0) MarkError(lmin);
	if (bmin>=0) diff_brother->MarkError(bmin);
	else ShowBaloon(LANG(MAINW_DIFF_NO_MORE,"No se encontraron mas cambios"));
}

void mxSource::ShowDiffChange() {
	int cl=GetCurrentLine();
	DiffInfo *di = first_diff_info;
	UndoActionGuard undo_action(this);
	while (di) {
		for (int i=0;i<di->len;i++) {
			if (MarkerLineFromHandle(di->handles[i])==cl) {
				int l = MarkerLineFromHandle(di->handles[0]);
				ShowBaloon(di->extra,PositionFromLine(l));
				di=nullptr; break;
			}
		}
		if (di) di = di->next;
	}
	Refresh();
}

void mxSource::Reload() {
	m_extras->FromSource(this);
	RaiiRestoreValue<bool> restore_sintitulo(sin_titulo);
	LoadFile(GetFullPath());
	m_extras->ToSource(this);
}

void mxSource::AlignComments (int col) {
	UndoActionGuard undo_action(this);
	config_source.alignComments=col;
	int ss = GetSelectionStart();
	int se = GetSelectionEnd();
	if (ss>se) { int aux=ss; ss=se; se=aux; }
	bool sel = se>ss; char c;
	int line_ss=sel?LineFromPosition(ss):0, line_se=sel?LineFromPosition(se):GetLineCount();
	int p3,pl=PositionFromLine(line_ss);
	bool prev=false;
	for (int i=line_ss;i<line_se;i++) {
		int p1=pl, s;
		int p2=PositionFromLine(i+1);
		if (!II_IS_COMMENT(p1) || prev) {
			if (!II_IS_COMMENT(p1)) {
				while (p1<p2 && !II_IS_COMMENT(p1)) p1++;
				p3=p1;
				while (p3<p2 && II_IS_NOTHING_2(p3) ) p3++;
			} else {
				while (p1<p2 && II_IS_2(p1,' ','\t')) p1++;
				p3=p2;
			}
			if (p1<p2 && p3==p2) {
				prev=true;
				int cp=GetColumn(p1);
				if (cp<col) {
					InsertText(p1,wxString(wxChar(' '),col-cp));
				} else if (col<cp) {
					SetTargetEnd(p1);
					p1--;
					while (p1>pl && II_IS_NOTHING_2(p1)) p1--;
					SetTargetStart(p1+1);
					cp=GetColumn(p1);
					if (col>=cp)
						ReplaceTarget(wxString(wxChar(' '),col-cp));
					else
						ReplaceTarget(" ");
				} else
					prev=false;
				p2=PositionFromLine(i+1);
			} else {
				prev=false;
				if (p3<p2) { p2=p3;  i--; }
			}
		}
		pl=p2;
	}
}

void mxSource::RemoveComments () {
	UndoActionGuard undo_action(this);
	int ss = GetSelectionStart();
	int se = GetSelectionEnd();
	if (ss>se) { int aux=ss; ss=se; se=aux; }
	bool sel = se>ss;
	int line_ss=sel?LineFromPosition(ss):0, line_se=sel?LineFromPosition(se):GetLineCount();
	int p1,p2;
	for (int i=line_ss;i<line_se;i++) {

		int p3=ss=PositionFromLine(i), s;
		se=PositionFromLine(i+1);
		
		while (p3<se && !II_IS_COMMENT(p3)) p3++;
		if (p3>=se) continue;
		p1=p3;
		while (p3<se && II_IS_COMMENT(p3)) p3++;
		p2=p3;
		if (p1<=GetLineIndentPosition(i) && p2>se-2)  { // si es toda la linea de comentario, borrar entera
			p1=PositionFromLine(i);
			p2=PositionFromLine(i+1);
			line_se--;
		} else if (p2==se) {
			i++; p2--;
		}
		SetTargetEnd(p2);
		SetTargetStart(p1);
		ReplaceTarget("");
		i--;
	}
}

bool mxSource::IsComment(int pos) {
	int s;
	return II_IS_COMMENT(pos);
}


/**
* Lleva el cursor a una posici�n espec�fica, forzando su visualizaci�n
* Reemplaza al GotoPos original porque ese cuando se le da una posici�n
* que no est� al comienzo de la linea hace scroll horizontal para centrarla
* y es basatante incomodo (apareci� con wxWidgets-2.8.10?)
**/
void mxSource::GotoPos(int pos) {
	wxStyledTextCtrl::GotoPos(PositionFromLine(LineFromPosition(pos)));
	SetSelectionStart(pos);
	SetSelectionEnd(pos);
}

void mxSource::CheckForExternalModifications() {
	if (!source_filename.FileExists()) {
		SetModify(true);
		return;
	}
	static wxDateTime dt;
	dt = source_filename.GetModificationTime();
	if (dt==source_time) return;
	class SourceModifAction:public mxMainWindow::AfterEventsAction {
	public: 
		SourceModifAction(mxSource *who):AfterEventsAction(who){}
		void Run() override { m_source->ThereAreExternalModifications(); }
	};
	main_window->CallAfterEvents(new SourceModifAction(this));
//	ThereAreExternalModifications();
}

void mxSource::ThereAreExternalModifications() {
	wxDateTime dt = source_filename.GetModificationTime();
	if (dt==source_time) return; else SetSourceTime(dt);
	if (!source_time_dont_ask) {
		mxMessageDialog::mdAns res =
			mxMessageDialog(main_window,LANG(SOURCE_EXTERNAL_MODIFICATION_ASK_RELOAD,""
												 "El archivo fue modificado por otro programa.\n"
												 "Desea recargarlo para obtener las modificaciones?"))
				.Title(source_filename.GetFullPath()).ButtonsYesNo().IconWarning()
				.Check1("No volver a preguntar",false).Run();
		source_time_reload = res.yes;
		source_time_dont_ask = res.check1;
	}
	if (source_time_reload) {
		Reload();
	} else {
		SetModify(true);
	}
}

bool mxSource::MySaveFile(const wxString &fname) {
	wxFile file(fname, wxFile::write);
	if (!file.IsOpened())
		return false;
	bool success = file.Write(GetText(), *wxConvCurrent);
	file.Flush(); file.Close();
	if (success) SetSavePoint();
	return success;
}
	
void mxSource::OnModifyOnRO (wxStyledTextEvent &event) {
	if (readonly_mode==ROM_DEBUG) {
		ro_quejado=true;
		mxMessageDialog::mdAns ans =
			mxMessageDialog(main_window,LANG(DEBUG_CANT_EDIT_WHILE_DEBUGGING,""
											 "Por defecto, no se puede modificar el fuente mientras se encuentra\n"
											 "depurando un programa, ya que de esta forma pierde la relaci�n que\n"
											 "existe entre la informaci�n que brinda el depurador a partir del\n"
											 "archivo binario, y el fuente que est� visualizando. Puede configurar\n"
											 "este comportamiento en la seccion Depuracion del cuadro de\n"
											 "Preferencias (desde menu Archivo)"))
				.Check1(LANG(DEBUG_ALLOW_EDIT_WHILE_DEBUGGING,"Permitir editar igualemente"),config->Debug.allow_edition)
				.Title(LANG(GENERAL_WARNING,"Advertencia")).IconWarning().Run();
		if (ans.check1) {
			config->Debug.allow_edition=true;
			SetReadOnlyMode(ROM_NONE);
			event.Skip();
		}
	} else {
		event.Skip();
	}
}

bool mxSource::IsEmptyLine(int l, bool ignore_comments, bool ignore_preproc) {
	int p = PositionFromLine(l), pf = (l==GetLineCount()-1)?GetLength():PositionFromLine(l+1);
	while (p<pf) {
		char c; int s = 0;
		if ( ! ( II_IS_4(p,' ','\t','\n','\r') 
			|| (ignore_comments && II_IS_COMMENT(p))
			|| (ignore_preproc && s==wxSTC_C_PREPROCESSOR)
			) ) return false;
		p++;
	}
	return true;
}

void mxSource::OnKeyDown(wxKeyEvent &evt) {
	int key_code = evt.GetKeyCode();
	if (key_code == WXK_MENU) {
		wxMouseEvent evt2;
		wxPoint pt = PointFromPosition(GetCurrentPos());
		evt2.m_x = pt.x; evt2.m_y=pt.y+TextHeight(0);
		OnPopupMenuInside(evt2,false);
		return;
	}
	if (calltip_mode==MXS_AUTOCOMP && evt.GetKeyCode()==WXK_BACK && config_source.autocompFilters) {
		/*evt.Skip();*/
		int cp = GetCurrentPos()-1;
		SetTargetStart(cp); SetTargetEnd(cp+1); ReplaceTarget(""); // manually delete character, event.Skip whould hide autocompletion menu
		if (cp>=autocomp_helper.GetUserPos())
			g_code_helper->FilterAutocomp(this,GetTextRange(autocomp_helper.GetBasePos(),cp),true);
		else HideCalltip();
		return;
	}
	if (config_source.autocloseStuff && evt.GetKeyCode()==WXK_BACK) {
		int p=GetCurrentPos();
		if (p) {
			char c2=GetCharAt(p);
			char c1=GetCharAt(p-1);
			if (
				(c1=='('&&c2==')')||
				(c1=='['&&c2==']')||
				(c1=='{'&&c2=='}')||
				(c1=='\''&&c2=='\'')||
				(c1=='\"'&&c2=='\"')
			) {
				SetTargetStart(p); SetTargetEnd(p+1); ReplaceTarget("");
			}
		}
	}
	if (evt.GetKeyCode()==WXK_ESCAPE) {
		if (multi_sel) multi_sel.End(this);
		if (calltip_mode!=MXS_NULL) {
			HideCalltip();
		} else {
			wxCommandEvent evt;
			main_window->OnEscapePressed(evt);
		}
	} else if (config_source.autotextEnabled && evt.GetKeyCode()==WXK_TAB && ApplyAutotext()){
		return;
	} else if (evt.GetKeyCode()==WXK_RETURN && multi_sel && multi_sel.ProcessEnter()) {
			multi_sel.End(this); return;
	} else {
		evt.Skip();
		// autocompletion list selected item could have been changed
		if (calltip_mode==MXS_AUTOCOMP && config_source.autocompTips) 
			autocomp_helper.Restart();
	}
}

void mxSource::SplitFrom(mxSource *orig) {
	LoadFile(orig->source_filename);
	treeId = orig->treeId;
	never_parsed=false; sin_titulo=false; 
	if (m_owns_extras) delete m_extras;
	m_extras=orig->m_extras;
	m_owns_extras=false;
	SetDocPointer(orig->GetDocPointer());
	next_source_with_same_file=orig;
	mxSource *iter=orig;
	while (iter->next_source_with_same_file!=orig) 
		iter=iter->next_source_with_same_file;
	iter->next_source_with_same_file=this;
	GotoPos(orig->GetCurrentPos());
}

void mxSource::SetSourceTime(wxDateTime stime) {
	source_time=stime;
	mxSource *iter=next_source_with_same_file;
	while (iter!=this) {
		iter->source_time=stime;
		iter=iter->next_source_with_same_file;
	}
}

wxString mxSource::WhereAmI() {
	int cp=GetCurrentPos(); wxString args;
	wxString res = FindScope(cp,&args,true); res<<args;
#ifdef _ZINJAI_DEBUG
	res<<"\n"<<cp;
#endif
	return res;
}

bool mxSource::ApplyAutotext() {
	return Autocoder::GetInstance()->Apply(this);
}

void mxSource::SetColours(bool also_style) {
	SetSelBackground(true,g_ctheme->SELBACKGROUND);
	StyleSetForeground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_FORE);
	StyleSetBackground (wxSTC_STYLE_DEFAULT, g_ctheme->DEFAULT_BACK);
	StyleSetForeground (wxSTC_STYLE_LINENUMBER, g_ctheme->LINENUMBER_FORE);
	StyleSetBackground (wxSTC_STYLE_LINENUMBER, g_ctheme->LINENUMBER_BACK);
	StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, g_ctheme->INDENTGUIDE);
	SetFoldMarginColour(true,g_ctheme->FOLD_TRAMA_BACK);
	SetFoldMarginHiColour(true,g_ctheme->FOLD_TRAMA_FORE);
	StyleSetForeground(wxSTC_STYLE_INDENTGUIDE, g_ctheme->INDENTGUIDE);

	CallTipSetBackground(g_ctheme->CALLTIP_BACK);
	CallTipSetForeground(g_ctheme->CALLTIP_FORE);
	
	SetCaretForeground (g_ctheme->CARET);
	MarkerDefine (wxSTC_MARKNUM_FOLDER,        wxSTC_MARK_BOXPLUS, g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDEROPEN,    wxSTC_MARK_BOXMINUS, g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDERSUB,     wxSTC_MARK_VLINE,     g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDEREND,     wxSTC_MARK_CIRCLEPLUS, g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_CIRCLEMINUS, g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNERCURVE,     g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
	MarkerDefine (wxSTC_MARKNUM_FOLDERTAIL,    wxSTC_MARK_LCORNERCURVE,     g_ctheme->FOLD_BACK,g_ctheme->FOLD_FORE);
//	MarkerDefine(mxSTC_MARK_CURRENT,wxSTC_MARK_BACKGROUND,g_ctheme->CURRENT_LINE,g_ctheme->CURRENT_LINE);
//	SetCaretLineVisible(true); SetCaretLineBackground(g_ctheme->CURRENT_LINE); SetCaretLineBackAlpha(50);
	SetCaretLineVisible(true); SetCaretLineBackground(g_ctheme->CURRENT_LINE); SetCaretLineBackAlpha(35);
	MarkerDefine(mxSTC_MARK_USER,wxSTC_MARK_BACKGROUND,g_ctheme->USER_LINE,g_ctheme->USER_LINE);
	// markers
	
	if (g_ctheme->inverted) {
		MarkerDefine(mxSTC_MARK_DIFF_ADD,wxSTC_MARK_BACKGROUND,"DARK GREEN","DARK GREEN");
		MarkerDefine(mxSTC_MARK_DIFF_DEL,wxSTC_MARK_ARROW,"WHITE","RED");
		MarkerDefine(mxSTC_MARK_DIFF_CHANGE,wxSTC_MARK_BACKGROUND,"BROWN","BROWN");
	} else {
		MarkerDefine(mxSTC_MARK_DIFF_ADD,wxSTC_MARK_BACKGROUND,"Z DIFF GREEN","Z DIFF GREEN");
		MarkerDefine(mxSTC_MARK_DIFF_DEL,wxSTC_MARK_ARROW,"BLACK","RED");
		MarkerDefine(mxSTC_MARK_DIFF_CHANGE,wxSTC_MARK_BACKGROUND,"Z DIFF YELLOW","Z DIFF YELLOW");
	}
	MarkerDefine(mxSTC_MARK_BAD_BREAKPOINT,wxSTC_MARK_CIRCLE, g_ctheme->DEFAULT_BACK, "LIGHT GRAY");
	MarkerDefine(mxSTC_MARK_BREAKPOINT,wxSTC_MARK_CIRCLE, g_ctheme->DEFAULT_BACK, "RED");
	MarkerDefine(mxSTC_MARK_HISTORY,wxSTC_MARK_SHORTARROW, g_ctheme->DEFAULT_FORE, "LIGHT GRAY");
	MarkerDefine(mxSTC_MARK_EXECPOINT,wxSTC_MARK_SHORTARROW, g_ctheme->DEFAULT_FORE, "Z GREEN");
	MarkerDefine(mxSTC_MARK_FUNCCALL,wxSTC_MARK_SHORTARROW, g_ctheme->DEFAULT_FORE, "YELLOW");
	MarkerDefine(mxSTC_MARK_STOP,wxSTC_MARK_SHORTARROW, g_ctheme->DEFAULT_FORE, "RED");
	
//	MarkerDefine(mxSTC_MARK_ERROR,wxSTC_MARK_BOXMINUS, g_ctheme->DEFAULT_BACK, "RED");
//	MarkerDefine(mxSTC_MARK_WARNING,wxSTC_MARK_BOXMINUS, g_ctheme->DEFAULT_BACK,"Z DARK YELLOW");
	MarkerDefine(mxSTC_MARK_WARNING,wxSTC_MARK_DOTDOTDOT, "Z DARK YELLOW","Z DARK YELLOW");
	MarkerDefine(mxSTC_MARK_ERROR,wxSTC_MARK_DOTDOTDOT, "RED", "RED");
	MarkerDefine(mxSTC_MARK_WARNING,wxSTC_MARK_ARROWS, "Z DARK YELLOW","Z DARK YELLOW");
	MarkerDefine(mxSTC_MARK_ERROR,wxSTC_MARK_ARROWS, "RED", "RED");
	
	if (also_style) SetStyle(config_source.syntaxEnable);
	if (m_minimap) m_minimap->SetStyle(lexer);
	
}

/**
* @brief Returns the path (dirs, without filename) of the file as debugger knows it (temp_filename if untitled, source_filename else)
*
* @param for_user  indicates that this path is for user, not for some internal ZinjaI operation, so if source is untitled it will use user's home instead of temp dir
**/
wxString mxSource::GetPath(bool for_user) {
	if (sin_titulo) {
		if (for_user)
			return wxFileName::GetHomeDir();
		else
			return temp_filename.GetPath();
	} else {
			return source_filename.GetPath();
	}
}


/**
* @brief Returns the full path of the file as debugger knows it (temp_filename if untitled, source_filename else)
**/
wxString mxSource::GetFullPath() {
	if (sin_titulo) return temp_filename.GetFullPath();
	else return source_filename.GetFullPath();
}

wxString mxSource::GetFileName(bool with_extension) {
	if (with_extension) {
		if (sin_titulo) return temp_filename.GetFullName();
		else return source_filename.GetFullName();
	} else {
		if (sin_titulo) return temp_filename.GetName();
		else return source_filename.GetName();
	}
}

/**
* @brief Save the code in source_filename (or in temp_filename if untitled) and returns its full path
*
* To be called from tools that need to parse it (cppcheck)
**/
wxString mxSource::SaveSourceForSomeTool() {
	if (sin_titulo) SaveTemp(); else SaveSource();
	return GetFullPath();
}

/**
* @brief return config_runnign.compiler_options parsed (variables replaced and subcommands executed, current_toolchain must be setted)
**/
wxString mxSource::GetCompilerOptions(bool parsed) {
	wxString comp_opts = cpp_or_just_c?config_running.cpp_compiler_options:config_running.c_compiler_options;
	if (parsed) {
		wxArrayString args; 
		mxUT::Split(comp_opts,args,false,true);
		for(unsigned int i=0;i<args.GetCount();i++) args[i]=current_toolchain.FixArgument(cpp_or_just_c,args[i]);
		comp_opts=mxUT::UnSplit(args);
		mxUT::ParameterReplace(comp_opts,"${ZINJAI_DIR}",config->zinjai_dir);
		mxUT::ParameterReplace(comp_opts,"${MINGW_DIR}",current_toolchain.mingw_dir);
		comp_opts = mxUT::ExecComas(working_folder.GetFullPath(),comp_opts);
	}
	return comp_opts;
}

void mxSource::SetCompilerOptions(const wxString &comp_opts) {
	if (cpp_or_just_c) 
		config_running.cpp_compiler_options = comp_opts;
	else 
		config_running.c_compiler_options = comp_opts;
}

bool mxSource::IsCppOrJustC() {
	return cpp_or_just_c;
}

wxFileName mxSource::GetBinaryFileName ( ) {
	if (project && !sin_titulo) 
		return DIR_PLUS_FILE(project->GetTempFolder(),source_filename.GetName()+".o");
	else
		return binary_filename;
}

void mxSource::OnPainted (wxStyledTextEvent & event) {
	char c; int p=GetCurrentPos();
	if ((c=GetCharAt(p))=='(' || c==')' || c=='{' || c=='}' || c=='[' || c==']') {
		MyBraceHighLight(p,BraceMatch(p));
	} else if ((c=GetCharAt(p-1))=='(' || c==')' || c=='{' || c=='}' || c=='[' || c==']') {
		int m=BraceMatch(p-1);
		if (m!=wxSTC_INVALID_POSITION)
			MyBraceHighLight(p-1,m);
		else
			MyBraceHighLight();
	} else
		MyBraceHighLight();
	event.Skip();
	if (mxGCovSideBar::HaveInstance()) mxGCovSideBar::GetInstance().Refresh(this);
	if (m_minimap) m_minimap->Refresh(this);
}

/**
* Esta funcion esta para evitar el flickering que produce usar el bracehighlight
* del stc cuando se llama desde el evento de udateui. A cambio, para que igual sea
* instantaneo se llama desde el evento painted, y para evitar que reentre mil veces
* se guardan las ultimas posiciones y no se vuelve a llamar si son las mismas.
* El problema es que est� recalculando el BraceMatch en cada paint.
**/
void mxSource::MyBraceHighLight (int b1, int b2) {
	if (b1==brace_1&&b2==brace_2) return;
	brace_1=b1; brace_2=b2;
	if (b2==wxSTC_INVALID_POSITION) BraceBadLight (b1);
	else BraceHighlight (b1,b2);
	Refresh(false);
}

void mxSource::OnFindKeyword(wxCommandEvent &event) {
	main_window->FindAll(GetCurrentKeyword());
}

void mxSource::HighLightWord(const wxString &word) {
	SetKeyWords(3,m_highlithed_word=word);
	Colourise(0,GetLength());
}

void mxSource::HighLightCurrentWord() {
	HighLightWord(GetCurrentKeyword());
}

void mxSource::OnHighLightWord (wxCommandEvent & event) {
	HighLightCurrentWord();
}

void mxSource::OnDoubleClick (wxStyledTextEvent & event) {
	event.Skip();
	HighLightCurrentWord();
}

void mxSource::UpdateExtras ( ) {
	m_extras->FromSource(this);
}

void mxSource::SetReadOnlyMode (ReadOnlyModeEnum mode) {
	if (mode>ROM_SPECIALS) {
		if (mode==ROM_ADD_DEBUG) {
			if (readonly_mode==ROM_NONE) readonly_mode=ROM_DEBUG;
			else if (readonly_mode==ROM_PROJECT) readonly_mode=ROM_PROJECT_AND_DEBUG;
		} else if (mode==ROM_DEL_DEBUG) {
			if (readonly_mode==ROM_DEBUG) readonly_mode=ROM_NONE;
			else if (readonly_mode==ROM_PROJECT_AND_DEBUG) readonly_mode=ROM_PROJECT;
		} else if (mode==ROM_ADD_PROJECT) {
			if (readonly_mode==ROM_NONE) readonly_mode=ROM_PROJECT;
			else if (readonly_mode==ROM_DEBUG) readonly_mode=ROM_PROJECT_AND_DEBUG;
		} else if (mode==ROM_DEL_PROJECT) {
			if (readonly_mode==ROM_PROJECT) readonly_mode=ROM_NONE;
			else if (readonly_mode==ROM_PROJECT_AND_DEBUG) readonly_mode=ROM_DEBUG;
		}
	} else {
		readonly_mode=mode;
	}
	SetReadOnly(readonly_mode!=ROM_NONE);
}

void mxSource::OnMacroAction (wxStyledTextEvent & evt) {
	if (main_window->m_macro&&(*main_window->m_macro)[0].msg==1) {
		main_window->m_macro->Add(MacroAction(evt.GetMessage(),evt.GetWParam(),evt.GetLParam()));
	}
}

wxString mxSource::GetCurrentKeyword (int pos) {
	if (pos==-1) pos=GetCurrentPos();
	int s=WordStartPosition(pos,true);
	if (GetCharAt(s-1)=='#') s--;
	int e=WordEndPosition(pos,true);
	return GetTextRange(s,e);
}

int mxSource::GetMarginForThisX (int x) {
	int x0=GetMarginWidth(MARGIN_LINENUM);
	if (x<x0) return MARGIN_LINENUM;
	x0+=GetMarginWidth(MARGIN_BREAKS);
	if (x<x0) return MARGIN_BREAKS;
	x0+=GetMarginWidth(MARGIN_FOLD);
	if (x<x0) return MARGIN_FOLD;
	return MARGIN_NULL;
}

void mxSource::UserReload ( ) {
	if (!sin_titulo && GetModify()) {
		main_window->notebook_sources->SetSelection(main_window->notebook_sources->GetPageIndex(this));
		if ( mxMessageDialog(this,LANG(MAINW_CHANGES_CONFIRM_RELOAD,""
									   "Hay Cambios sin guardar, desea recargar igualmente la version del disco?"))
				.Title(GetFullPath()).ButtonsYesNo().IconQuestion().Run().no ) 
		{
			return;
		}
	}
	SetSourceTime((sin_titulo?temp_filename:source_filename).GetModificationTime());
	Reload();
	parser->ParseFile(GetFullPath());
}

void mxSource::ShowCallTip (int brace_pos, int calltip_pos, const wxString & s) {
	focus_helper.Mask();
	int current_line = GetCurrentLine(); calltip_line = LineFromPosition(calltip_pos);
	if (calltip_line!=current_line) { // move calltip down if cursor is not in calltip's line
		int calltip_offset = calltip_pos-PositionFromLine(calltip_line);
		calltip_pos=PositionFromLine(current_line)+calltip_offset;
		int current_line_end_pos = GetLineEndPosition(current_line);
		if (calltip_pos>current_line_end_pos) calltip_pos = current_line_end_pos;
		calltip_line = current_line;
	}
	SetCalltipMode(MXS_CALLTIP);
	last_failed_autocompletion.Reset(); 
	if (!calltip) calltip = new mxCalltip(this);
	calltip_brace = brace_pos;
	calltip->Show(calltip_pos,s);
}

void mxSource::HideCalltip () {
	switch (calltip_mode) { 
		case MXS_NULL: break;
		case MXS_INSPECTION: HideInspection(); break;
		case MXS_CALLTIP: calltip->Hide(); break;
		case MXS_BALOON: wxStyledTextCtrl::CallTipCancel(); break;
		case MXS_AUTOCOMP: wxStyledTextCtrl::AutoCompCancel(); if (calltip) calltip->Hide(); break;
	}
	calltip_mode = MXS_NULL;
}

void mxSource::ShowAutoComp (int typed_len, const wxString &keywords_list, bool is_filter) { 
	SetCalltipMode(MXS_AUTOCOMP);
	last_failed_autocompletion.Reset(); 
	focus_helper.Mask();
#ifdef _STC_HAS_ZASKARS_RESHOW
	if (is_filter) wxStyledTextCtrl::AutoCompReShow(typed_len,keywords_list);
	else 
#endif
		wxStyledTextCtrl::AutoCompShow(typed_len,keywords_list);
	int pbase = GetCurrentPos()-typed_len;
	wxPoint pt1=PointFromPosition(pbase);
	wxPoint pt2=GetScreenPosition();
	if (calltip_mode==MXS_AUTOCOMP && config_source.autocompTips) 
		autocomp_helper.Start(pbase,is_filter?-1:(pbase+typed_len), pt1.x+pt2.x, pt1.y+pt2.y);
}


void mxSource::OnAutocompSelection(wxStyledTextEvent &event) {
	if (calltip) calltip->Hide();
	calltip_mode = MXS_NULL;
}


void mxSource::OnTimer(wxTimerEvent &event) {
	wxTimer *timer = reinterpret_cast<wxTimer*>(event.GetEventObject());
	if (focus_helper.IsThisYourTimer(timer)) { 
		focus_helper.Unmask();
	} else if (autocomp_helper.IsThisYourTimer(timer)) {
		if (!wxStyledTextCtrl::AutoCompActive()) { // si se cancelo el menu (por ejemplo, el usuario siguio escribiendo la palabra y ya no hay conicidencias)
			if (calltip_mode==MXS_AUTOCOMP) calltip_mode=MXS_NULL; // solo si el modo es autocomp, porque puede ya haber lanzado un calltip real
		}
		if (calltip_mode!=MXS_AUTOCOMP) return;
		wxString help_text = g_autocomp_list.GetHelp(AutoCompGetCurrent());
		if (!help_text.Len()) { if (calltip) calltip->Hide(); return; }
		if (!calltip) calltip = new mxCalltip(this);
		int autocomp_max_len = g_autocomp_list.GetMaxLen();
		focus_helper.Mask();
		calltip->Show(autocomp_helper.GetX(),autocomp_helper.GetY(),autocomp_max_len,help_text);
	}
}

void mxSource::OnEditMakeLowerCase (wxCommandEvent & event) {
	LowerCase();
}

void mxSource::OnEditMakeUpperCase (wxCommandEvent & event) {
	UpperCase();
}



/// @todo: ver si es mejor sacar todo esto y delegarselo a scintilla
///
/// nota importante: despues de implementar la edicion en multiples lineas
/// para el scintilla de wx2.8, veo que el de 3.0 ya lo tiene resuelto 
/// (ver SetAdditionalSelectionTyping)... cuando logre migrar a wx3 tendre
/// que cambiar esto :(
void mxSource::MultiSelController::InitRectEdit (mxSource *src, bool keep_rect_select) {
	this->Reset();
	// get ordered selection limits
	int beg = src->GetSelectionStart(), end = src->GetSelectionEnd(); if (beg>end) swap(beg,end);
	// get lines range
	int line_from = src->LineFromPosition(beg), line_to = src->LineFromPosition(end);
	// check for multiline selection
	int cbeg = src->GetColumn(beg), cend = src->GetColumn(end);
	end = src->FindColumn(line_from,cend);
	int word_width = end-beg;
	if (line_from==line_to || word_width<0) return;
	for(int i=line_from+1;i<=line_to;i++) {
		int p1 = src->FindColumn(i,cbeg);
		int p2 = src->FindColumn(i,cend);
		if (p2-p1!=word_width) continue;
		this->AddPos(src,i,p1);
	}
	// rember original text and positions in order to detect changes later
	this->SetEditRegion(src,line_from,beg,end);
	// enable rectangular edition, and remember if selection is kept (in that case, first keystroke will modify all lines before ApplyRectEdit)
	if (!keep_rect_select) src->SetSelection(beg,beg+m_ref_str.Len());
	this->BeginEdition(src,keep_rect_select,!keep_rect_select);
}

void mxSource::MultiSelController::ApplyRectEdit (mxSource *src) {
	// si cambia de linea, se termina la edicion
	if (src->GetCurrentLine()!=m_line) {
		this->End(src); return;
	}
	// cur = pos actual, lbeg y lend son las pos globales de inicio y fin de la linea editada
	int cur = src->GetCurrentPos();
	// [pbeg,pend) son las pos actuales donde empieza y termina la zona de edicion
	int pbline = src->PositionFromLine(m_line), peline = src->GetLineEndPosition(m_line);
	int pbeg = pbline+m_offset_beg, pend = peline-m_offset_end;
	// si se salio de la zona de edicion, termina la edicion
	if (cur<pbeg || cur>pend) {
		this->End(src);
		return;
	}
	// new_str es el nuevo contenido en esa linea, ref_str el viejo, ambos para toda la zona de edicion
	wxString new_str = src->GetTextRange(pbeg,pend), &ref_str = m_ref_str;
	if (m_keep_highlight && new_str.Len()) { src->HighLightCurrentWord(); }
	// lr y ln son los largos de ambos contenidos
	int i=0, lr=ref_str.Len(), ln=new_str.Len(); 
	// acotar la parte modificada, avanzando desde afuera hacia adentro mientras no haya cambio
	while (i<lr && i<ln && ref_str[i]==new_str[i]) { i++; }
	while (lr>i && ln>i && ref_str[lr-1]==new_str[ln-1]) { lr--; ln--; }
	// si no cambio nada, no hacer nada
	if (i==lr && lr==ln) return;
	// cortar las partes que son diferentes de cada cadena de ref
	wxString sfrom = ref_str.Mid(i,lr-i), sto = new_str.Mid(i,ln-i);
	UndoActionGuard undo_action(src);
	int dbeg = pbeg+i; /*dend = pbeg+sfrom.Len();*/ // ahora dbeg y dend acotan solo la parte modificada, en terminos de la cadena original
	int doffset = (dbeg-pbline) - m_offset_beg;
	int prev_line = m_line, delta_for_next = sto.Len()-sfrom.Len(); // si hay dos ediciones en la misma linea, la primera cambia el offset de la segunda
	// para cada linea de la seleccion...
	for(unsigned int i=0;i<m_positions.size();i++) {
		MultiSelController::EditPos &rs = m_positions[i];
		if (prev_line==rs.line) {
			rs.offset += delta_for_next;
		} else {
			prev_line = rs.line;
			delta_for_next = 0;
		};
		// obtener posiciones para la linea actual, segun columnas
		int tbeg = src->PositionFromLine(rs.line)+rs.offset+doffset;
		// ? -> si realmente habia un seleccion rectangular, la edicion ya borro lo seleccionado, por eso contraer a 0 el area original
		int tend = tbeg+ ( m_was_rect_select ? 0 : sfrom.Len() ); 
		// reemplazar desde tbeg a tend, con sto
		src->SetTargetStart(tbeg); src->SetTargetEnd(tend); src->ReplaceTarget(sto);
		delta_for_next += sto.Len()-sfrom.Len();
		if (tbeg<pbeg) { pbeg+=sto.Len()-sfrom.Len(); pend+=sto.Len()-sfrom.Len(); } // por si la que editamos no es la primera que aparece
	}
	// la selecci�n ya no ser� rectangular
	m_was_rect_select=false;
	// guardar la linea modificada como nueva referencia para la pr�xima edici�n
	this->SetEditRegion(src,m_line,pbeg,pend);	
}

void mxSource::OnClickUp(wxMouseEvent & evt) {
	evt.Skip();
	if (evt.AltDown() && SelectionIsRectangle()) multi_sel.InitRectEdit(this,true);
}

void mxSource::OnEditRectangularEdition (wxCommandEvent & evt) {
	multi_sel.InitRectEdit(this,false);
}

void mxSource::HideInspection ( ) {
	if (!inspection_baloon) return;
	inspection_baloon->Destroy();
	inspection_baloon = nullptr;
}

void mxSource::ShowInspection (const wxPoint &pos, const wxString &exp, const wxString &val) {
	focus_helper.Mask();
	SetCalltipMode(MXS_INSPECTION);
	wxPoint p2(pos.x-5, pos.y-5); // para que el mouse quede dentro del inspection_baloon
	inspection_baloon = new mxInspectionBaloon(p2,exp,val);
}

//void mxSource::OnMouseWheel (wxMouseEvent & event) {
//	if (event.ControlDown()) {
//		if (event.m_wheelRotation>0) {
//			ZoomIn();
//		} else {
//			ZoomOut();
//		}
//	} else
//		event.Skip();
//}

int mxSource::GetStatementStartPos(int pos, bool skip_coma, bool skip_white, bool first_stop) {
	int s, l=pos, pos_skip=-1; // l y s son auxiliares para II_*
	char c=GetCharAt(pos); 
	II_BACK_NC(pos,II_IS_NOTHING_4(pos)); // retroceder hasta el primer caracter no ignorable
	l=pos;
	while (pos>0 && (c!='{'&&c!='('&&c!='['&&/*c!='<'&&*/c!=';'&&(c!=','||skip_coma))) {
		if (c==')'||c=='}'||c==']'/*||c=='>'*/) {
			int pos_match = BraceMatch(pos);
			if (pos_match!=wxSTC_INVALID_POSITION) {
				if (c=='}') --pos_match;
				II_BACK(pos_match,II_IS_NOTHING_4(pos_match));
				// todo el lio que sigue es para evitar seguir para atras cuando es funcion como en "void foo(bla){bla}"... cortar en el foo
				if (c==')' || ( s==wxSTC_C_WORD && 
				   (TextRangeWas(pos_match,"const") || TextRangeWas(pos_match,"override") 
					|| TextRangeWas(pos_match,"explicit")) ) ) 
				{
					int p_func_name = BraceMatch(pos_match);
					if (p_func_name!=wxSTC_INVALID_POSITION) {
						p_func_name--; II_BACK_NC(p_func_name,II_IS_NOTHING_4(p_func_name));
						if (s==wxSTC_C_IDENTIFIER||s==wxSTC_C_GLOBALCLASS) { // para evitar if,while,for,lambdas, something else?
							II_BACK(p_func_name,(c=GetCharAt(p_func_name))&&II_IS_KEYWORD_CHAR(c));
							II_BACK(p_func_name,II_IS_NOTHING_4(p_func_name));
							if (c!=','&&c!=':'&&c!='{') { // que no sea una lista de inicializadores en un constructor
								break; // si era el par de llaves de una funcion, no seguir.... faltar�a contemplar "namespace bla {...}"
							} else 
								pos_match = p_func_name+1; // sin esto se mete a analizar dentro del par�ntesis
							// el '{' del if anterior est� para cuando no era algo v�lido, por ejemplo, era un if mal escrito y por eso parecia un identificador
						}
					}
				}
				pos=pos_match-1;
			}
		}
		pos_skip=pos; --pos;
		II_BACK(pos,II_IS_NOTHING_4(pos)); // SIDE-EFFECT!! esto actualiza c y s
		if (s==wxSTC_C_WORD) {
			if (first_stop && (TextRangeWas(pos,"do")||TextRangeWas(pos,"while")||TextRangeWas(pos,"if")||TextRangeWas(pos,"for"))) {
				pos_skip = WordStartPosition(pos,true);
				pos=-1;
				break;
			} else if (TextRangeWas(pos,"public")||TextRangeWas(pos,"protected")||TextRangeWas(pos,"private")
					   ||TextRangeWas(pos,"case")||TextRangeWas(pos,"default") )
			{
				int pos_dos_puntos = pos+1; // buscar los ':'
				II_FRONT(pos_dos_puntos,II_IS_NOTHING_4(pos_dos_puntos)||!(II_IS_2(pos_dos_puntos,':','{')));
				if (c!=':'||GetCharAt(pos_dos_puntos+1)==':') { // ojo con "class boo : public lala {", por eso arriba corto en la '{';
					continue;
				}
				if (pos_dos_puntos<l) { // si los ':' estan antes del pos de entrada, la instruccion que se busca es la siguiente
					pos_skip = -1;
					pos = pos_dos_puntos+1;
				} else { // sino es esta (el "public:", "case x:" o "lo-que-sea:")
					pos_skip = WordStartPosition(pos,true);
					pos = -1;
				}
				break;
			} else {
				pos = WordStartPosition(pos,true);
				pos_skip = pos;
			}
		}
	}
	if (skip_white) {
		if (pos_skip==-1) {
			pos_skip=pos+1;
			II_FRONT(pos_skip,II_IS_NOTHING_4(pos_skip));
		}
		return pos_skip;
	} else {
		if (pos==-1) {
			pos = pos_skip-1;
			II_BACK(pos,II_IS_NOTHING_4(pos));
		}
		if (pos) ++pos;
		return pos;
	};
}

bool mxSource::GetCurrentCall (wxString &ftype, wxString &fname, wxArrayString &args, int pos) {
	int s=GetStyleAt(pos); if (s!=wxSTC_C_IDENTIFIER&&s!=wxSTC_C_GLOBALCLASS&&s!=wxSTC_C_DEFAULT) --pos;
	s=GetStyleAt(pos); if (s!=wxSTC_C_IDENTIFIER&&s!=wxSTC_C_GLOBALCLASS&&s!=wxSTC_C_DEFAULT) return false; // if it is not an identifier, cannot be a function call
	char c; int l=GetLength(), p0=pos,p1=pos+1;
	II_BACK(p0,(c=GetCharAt(p0))&&II_IS_KEYWORD_CHAR(c)); p0++; // p0 is where the name starts
	II_FRONT(p1,(c=GetCharAt(p1))&&II_IS_KEYWORD_CHAR(c)); // p1 is right after where the name ends
	int p2 = p1; II_FRONT(p2,II_IS_NOTHING_2(p2)); // p2 should be where the arguments start
	if (c!='(') return false; 
	int p3 = BraceMatch(p2); // p5 is where arguments list ends
	
	// get arguments list
	wxString one_arg;
	int p=p1+1;
	do {
		if (!II_IS_4(p,' ','\n','\r','\t') && !II_IS_COMMENT(p)) {
			c=GetCharAt(p);
			if (c=='(') {
				p=BraceMatch(p); // no hace falta verificar por pos invalida, porque p5 tiene match, eso garantiza que adentro todos tienen
			} else if (c==','||p==p3) {
				if (one_arg!="") {
					wxString type;
					// see if the actual arg is expression or identifier (only the second could be the formal parameter's name)
					unsigned int oalen = one_arg.Len();
					for(unsigned int j=0;j<oalen;j++) {
						bool is_keyword_char = IsKeywordChar(one_arg[j]);
						if (j==0 && one_arg[0]>='0' && one_arg[0]<='9') is_keyword_char=false;
						if (!is_keyword_char) {
							type = LocalRefactory::GetLiteralType(one_arg);
							if (!type.Len()) {
								// get something for the name
								unsigned int pos_name = one_arg.Len();
								while (pos_name>0 && 
										(IsKeywordChar(one_arg[pos_name-1]) || one_arg[pos_name-1]=='[' || one_arg[pos_name-1]==']')
									  ) pos_name--; // get the name
								if (pos_name==one_arg.Len()) one_arg="???"; 
								else {
									one_arg=one_arg.Mid(pos_name);
									one_arg.Replace("[","");
									one_arg.Replace("]","");
								}
							} else {
								one_arg="???";
							}
							break;
						}
					}
					// find out argument type
					if (one_arg!="" && type=="") {
						int dims=0;
						type = FindTypeOfByPos(p-1,dims,true);
						if (type=="") type="???";
						else if (dims>0) { // array or ptr
							one_arg = wxString("*")+one_arg;
							while (--dims>0) if (one_arg.Last()==']') one_arg = wxString("*")+one_arg; else one_arg<<"[]";
						} else if (dims<0) type="???";
					}
					one_arg = type+" "+one_arg;
					args.Add(one_arg);
					one_arg.Clear();
				}
			} else {
				one_arg<<c;
			};
		}
	} while (p++<p3);
	
	fname = GetTextRange(p0,p1);
	
	// get return type
	p0--; II_BACK(p0,II_IS_NOTHING_4(p0)); ftype="???";
	
	if (c=='(') { // function call???
		--p0; II_BACK(p0,II_IS_NOTHING_4(p0));
		if (s==wxSTC_C_WORD && ( TextRangeWas(p0,"while")||TextRangeWas(p0,"if") )) ftype="bool";
		; // we don't handle this yet
	
	} else if (s==wxSTC_C_WORD && TextRangeWas(p0,"return")) { // return value for current scope
		int scope_start = -1;
		wxString scope_args, scope = FindScope(pos,&scope_args,true,&scope_start);
		if (scope!="") {
			int p = GetStatementStartPos(scope_start);
			--scope_start; II_BACK(scope_start,II_IS_NOTHING_4(scope_start));
			if (GetStyleAt(p)==wxSTC_C_WORD && TextRangeIs(p,"template")) { // saltear template
				p+=8; II_FRONT(p,II_IS_NOTHING_4(p));
				if (c=='<') p=SkipTemplateSpec(this,'<',scope_start);
				if (p==wxSTC_INVALID_POSITION) p=scope_start;
			}
			int pend = p, p_last_blank=-1;
			while((c=GetCharAt(pend))!='(' || c=='{') { // avanzar hasta donde empiezan los argumentos
				if (II_IS_NOTHING_4(pend)) {
					p_last_blank = pend;
				}
				pend++;
			}
			if (p_last_blank!=-1) {
				II_BACK(p_last_blank,II_IS_NOTHING_4(p_last_blank));
				ftype = GetTextRange(p,p_last_blank+1);
			}
		};
		
	} else if (c=='=') { // assigment, lvalue type
		int p=p0-1; II_BACK(p,II_IS_NOTHING_4(p));
		wxString key = GetCurrentKeyword(p);
		ftype = FindTypeOfByKey(key,++p,true);
		if (ftype=="") ftype="???";
	} else if (c==';'||c=='{'||c=='}') {
		ftype="void";
	};
	
	return true;
}

void mxSource::OnEditHighLightedWordEdition (wxCommandEvent & evt) {
	int pbeg = GetSelectionStart(), pend = GetSelectionEnd();
	if (pbeg==pend) { HighLightCurrentWord(); return; }
	if (pbeg>pend) swap(pbeg,pend);
	if (m_highlithed_word.IsEmpty() || !config_source.syntaxEnable) return;
	multi_sel.Reset();
	int p0, p1;
	bool first = true;
	for(int i=pbeg;i<pend;i++) { 
		if (GetStyleAt(i)==wxSTC_C_GLOBALCLASS) {
			int i1 = i+m_highlithed_word.Len();
			if (first) {
				multi_sel.SetEditRegion(this,LineFromPosition(i),i,i1);
				first = false; p0=i; p1=i1;
			} else {
				multi_sel.AddPos(this,LineFromPosition(i),i);
			};
			i=i1-1;
		}
	}
	if (!first) { SetSelectionStart(p0); SetSelectionEnd(p1); }
	if (multi_sel.HasPositions()) multi_sel.BeginEdition(this,false).SetKeepHighligth();
}

void mxSource::MultiSelController::SetEditRegion(mxSource *src, int line, int pbeg, int pend) {
	m_line = line; 
	m_offset_beg = pbeg-src->PositionFromLine(line);
	m_offset_end = src->GetLineEndPosition(line)-pend;
	m_ref_str = src->GetTextRange(pbeg,pend);
}

mxSource::MultiSelController &mxSource::MultiSelController::BeginEdition(mxSource *src, bool rectangular, bool notify) { 
	m_was_rect_select = rectangular;
	m_is_on = true; 
	if (notify) main_window->SetStatusText(LANG(MAINW_PRESS_ESC_TO_FINISH_RECT_EDIT,"Presione ESC o mueva el cursor de texto a otra linea para volver al modo de edici�n normal."));
	return *this;
}


void mxSource::MultiSelController::End(mxSource *src) {
	if (m_keep_highlight) { 
		src->SetKeyWords(3,src->m_highlithed_word=""); 
		src->Colourise(0,src->GetLength()); 
	}
	m_is_on=false;
	main_window->SetStatusText(LANG(GENERAL_READY,"Listo"));
	if (m_on_end) {
		m_on_end->Run();
		delete m_on_end;
		m_on_end = nullptr;
	}
}

void mxSource::ReloadErrorsList ( ) {
	if (m_cem_ref.IsOk()) return;
	MarkerDeleteAll(mxSTC_MARK_ERROR);
	MarkerDeleteAll(mxSTC_MARK_WARNING);
	if (!m_cem_ref.Update()) return;
	const vector<CompilerErrorsManager::CEMError> &v = m_cem_ref.GetErrors();
	for(size_t i=0;i<v.size();i++) { 
		int b0_line = v[i].line-1;
		bool is_error = v[i].is_error;
		if (is_error && (MarkerGet(b0_line)&(1<<mxSTC_MARK_WARNING))) MarkerDelete(b0_line,mxSTC_MARK_WARNING);
		int marker_handler = MarkerAdd(b0_line,is_error?mxSTC_MARK_ERROR:mxSTC_MARK_WARNING);
		m_cem_ref.RegisterMarker(marker_handler,b0_line);
	}
}

int mxSource::FixErrorLine (int line) {
	return m_cem_ref.FixLineNumber(this,line);
}

void mxSource::SetNotInTheProjectAnymore ( ) {
	if (!m_owns_extras) {
		m_owns_extras = true;
		m_extras = new SourceExtras();
	}
	treeId.Unset();
	SetTreeItem(treeId);
}

void mxSource::SetTreeItem (const wxTreeItemId & item) {
	treeId = item;
	for( mxSource *iter=next_source_with_same_file; iter!=this; iter=iter->next_source_with_same_file) {
		iter->treeId = item;
	}
}

mxMiniSource *mxSource::GetMinimap(mxMiniMapPanel *panel) {
	if (!m_minimap) m_minimap = make_unique<mxMiniSource>(panel,this);
	return m_minimap.get();
}


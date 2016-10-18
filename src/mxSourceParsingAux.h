#ifndef MXSOURCEPARSINGAUX_H
#define MXSOURCEPARSINGAUX_H

#include "mxSource.h"

#define mxINVALID_POS wxSTC_INVALID_POSITION

#define II_SHOULD_IGNORE(p) ((s=GetStyleAt(p))==wxSTC_C_COMMENT || s==wxSTC_C_COMMENTLINE || s==wxSTC_C_CHARACTER || s==wxSTC_C_STRING || s==wxSTC_C_STRINGEOL || s==wxSTC_C_COMMENTLINEDOC || s==wxSTC_C_COMMENTDOC || s==wxSTC_C_PREPROCESSOR || s==wxSTC_C_COMMENTDOCKEYWORD || s==wxSTC_C_COMMENTDOCKEYWORDERROR)

/// @brief returns true if the given style corresponds to comment, literal or preprocesso
inline bool ShouldIgnoreByStyle(int style) {
	return style==wxSTC_C_COMMENT || style==wxSTC_C_COMMENTLINE || style==wxSTC_C_COMMENTLINEDOC || style==wxSTC_C_COMMENTDOC 
		|| style==wxSTC_C_COMMENTDOCKEYWORD || style==wxSTC_C_COMMENTDOCKEYWORDERROR || style==wxSTC_C_PREPROCESSOR;
}

/// @brief returns true if the given style corresponds to comment, preprocesso, or literal (that last one is the diff with the no ex version)
inline bool ShouldIgnoreByStyleEx(int style) {
	return style==wxSTC_C_CHARACTER || style==wxSTC_C_STRING || style==wxSTC_C_STRINGEOL || ShouldIgnoreByStyle(style);
}

/// @brief returns true if the given char is a space, tab or newline
inline bool IsWhiteCharOrEndl(char chr) {
	return chr=='\n'||chr=='\r'||chr==' '||chr=='\t';
}

/// @brief Given a char, says if that one can be part of a keyword or identifier (true limits to non-numbers)
inline bool IsKeywordChar (char chr, bool is_the_first_char=false) {
	return (chr>='a'&&chr<='z')||(chr>='A'&&chr<='Z')||chr=='_'||(!is_the_first_char&&(chr>='0'&&chr<='9'));
}

/// aux function for FindTypeOf
inline int SkipTemplateSpec(mxSource *src, int pos_start, int pos_max=0) {
	if (!pos_max) pos_max = src->GetLength();
	int tplt_deep = 1, p=pos_start; // s es para II_IS_COMMENT
	while (++p<pos_max && tplt_deep!=0) {
		if (!ShouldIgnoreByStyle(src->GetStyleAt(p))) {
			char c = src->GetCharAt(p);
			if (c=='>')
				tplt_deep--;
			else if (c=='<')
				tplt_deep++;
		}
	}
	if (!tplt_deep) return p;
	else return mxINVALID_POS;
}

/// sabiendo donde termina una lista de argumentos del template (pos_start, '>'), 
/// encuentra donde empieza (retorna la pos *antes* del '<')
inline int SkipTemplateSpecBack(mxSource *src, int pos_start) {
	int tplt_deep = 1, p=pos_start; // s es para II_IS_COMMENT
	while (--p>=0 && tplt_deep!=0) {
		if (!ShouldIgnoreByStyle(src->GetStyleAt(p))) {
			char chr = src->GetCharAt(p);
			if (chr=='<')
				tplt_deep--;
			else if (chr=='>')
				tplt_deep++;
		}
	}
	if (!tplt_deep) return p;
	else return mxINVALID_POS;
}












/// @brief given a position, advances it skipping whites, eols, comments and preprocessor defs
inline int SkipWhitesAndComments(mxSource *src, int pos, int max_pos) {
	while (pos<max_pos && 
		     ( IsWhiteCharOrEndl(src->GetCharAt(pos)) || 
			   ShouldIgnoreByStyle(src->GetStyleAt(pos) ) ) ) ++pos;
	return pos;
}

/// @brief given a position, goes backwards until finds a '{', ';', '(', '}' or a single ':', and then 
///        forewards skipping whites. The idea is to get to an statement's begin (very fast, but not perfect).
inline int GetStartSimple(mxSource *src, int pos) {
	int orig_pos = pos;
	while (pos>0) {
		int style = src->GetStyleAt(pos);
		if (!ShouldIgnoreByStyle(style)) {
			char chr = src->GetCharAt(pos);
			if (chr==':') {
				if(pos>0&&src->GetCharAt(pos-1)==':') --pos;
				else { ++pos; break; }
			} else if (chr=='{' || chr=='(' || chr==';' || chr=='}') {
				++pos; break;
			} else if (chr==')'||chr=='}') {
				pos = src->BraceMatch(pos);
				if (pos==mxINVALID_POS) 
					return mxINVALID_POS;
			}
		}
		--pos;
	}
	return SkipWhitesAndComments(src,pos,orig_pos);
}

/// @brief given the position where a typename starts, findout where it ends,
///        considering scopes, templates args, qualifiers, et al; 
///        but NOT *, & and other per-identifier stuff
inline int GetTypeEnd(mxSource *src, int pos, int max_pos) {
	int pos_wordend = src->WordEndPosition(pos,true);
	wxString word = src->GetTextRange(pos,pos_wordend);
	int pos_nexttoken = SkipWhitesAndComments(src,pos_wordend,max_pos);
	if (word=="const"||word=="constexpr"||word=="mutable"||word=="volatile"||
		word=="static"||word=="inline"||word=="typename"||word=="extern"||
		word=="signed"||word=="unsigned") 
	{
			return GetTypeEnd(src,pos_nexttoken,max_pos);
	}
	char chr = src->GetCharAt(pos_nexttoken);
	if (chr=='<') {
		pos_wordend = SkipTemplateSpec(src,pos_nexttoken,max_pos);
		pos_nexttoken = SkipWhitesAndComments(src,pos_wordend,max_pos);
		chr = src->GetCharAt(pos_nexttoken);
	}
	if (chr==':') return GetTypeEnd(src,SkipWhitesAndComments(src,pos_nexttoken+2,max_pos),max_pos);
	return pos_wordend;
}

/// @brief given a type specification, removes all leading scopes
inline wxString StripNamespaces(const wxString &type) {
	int pfrom = 0;
	for(size_t i=1;i<type.Len();i++)
		if (type[i]==':' && type[i-1]==':') pfrom = i+1;
	return type.Mid(pfrom);
}

/// auxiliar funcion for the real StripTemplateArgs
inline void stStripTemplateArgs(wxString &type, int p) {
	int nesting = 1, p0 = p++;
	do {
		if (type[p]=='<') ++nesting;
		else if (type[p]=='>') --nesting;
		++p;
	} while (p<int(type.Len()) && nesting!=0);
	type.erase(p0,p-p0);
}

/// @brief given a type specification, removes all template actual arguments
inline wxString StripTemplateArgs(const wxString &type) {
	wxString ret = type;
	for(size_t i=0;i<ret.Len();i++)
		if (ret[i]=='<') stStripTemplateArgs(ret,i--);
	return ret;
}

/// @brief given a type specification, removes all leading "qualifiers" o "decorators".
///        Any leading word (simple word followed by a whitespace) will be consiedered "qualifier"
inline wxString StripQualifiers(const wxString &type) {
	int pfrom = 0;
	for(size_t i=0;i<type.Len();i++)
		if (type[i]==' ') pfrom = i+1;
		else if (!IsKeywordChar(type[i])) break;
	return type.Mid(pfrom);
}

/// @brief similar to wxSource::GetTextRange but skipping comments, eols, and extra spaces
inline wxString GetTextRangeEx(mxSource *src, int from, int to) {
	wxString ret = src->GetTextRange(from,to);
	bool first = true; int delta=0;
	for(int is=from,ir=0;is<to;is++,ir++) {
		if (IsWhiteCharOrEndl(src->GetCharAt(is))||ShouldIgnoreByStyle(src->GetStyleAt(is))) {
			if (first) { ret[ir] = ' '; first = false; }
			else  delta++;
		} else first = true;
		if (delta) ret[ir-delta] = ret[ir];
	}
	return ret.Mid(0,ret.Len()-delta);
}

#endif

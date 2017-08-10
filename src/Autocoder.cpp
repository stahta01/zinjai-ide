#include <wx/textfile.h>
#include "Autocoder.h"
#include "mxSource.h"
#include "mxUtils.h"
#include "ProjectManager.h"
#include "mxSourceParsingAux.h"

Autocoder *Autocoder::m_instance = nullptr;

Autocoder::Autocoder() {
	Reset();
}

void Autocoder::Reset(wxString pfile) {
	Clear();
	if (!LoadFromFile(config->Files.autocodes_file)) {
		LoadFromFile(config->GetZinjaiSamplesPath("autocodes"));
	}
	if (pfile.Len()) LoadFromFile(pfile);
}

void Autocoder::Clear() {
	m_list.clear();
	m_description.Clear();
}

bool Autocoder::LoadFromFile(wxString filename) {
	wxTextFile fil(filename);
	if (!fil.Exists()) return false;
	fil.Open(); auto_code ac; wxString name;
	for ( wxString str = fil.GetFirstLine(); !fil.Eof(); str = fil.GetNextLine() ) {
		if (str.StartsWith("#autocode ")) {
			while (ac.code.Last()=='\r'||ac.code.Last()=='\n') ac.code.RemoveLast();
			if (name.Len()) m_list[name]=ac;
			else m_description=ac.description;
			ac.Clear();
			int i=10, l=str.Len();
			while (i<l && (str[i]==' '||str[i]=='\t')) i++;
			int li=i; char c;
			while (i<l && (c=str[i]) && ((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_')) i++;
			name=str.Mid(li,i-li);
			while (str[i]==' '||str[i]=='\t') i++;
			if (i<l) {
				if (str[i]=='(') {
					++i;
					while (true) {
						while (str[i]==' '||str[i]=='\t') i++;
						li=i;
						while(i<l && str[i]!=',' && str[i]!=')') i++;
						wxString par=str.Mid(li,i-li);
						while (par.Len() && (par.Last()==' '||par.Last()=='\t')) par.RemoveLast();
						ac.args.Add(par);
						li=++i;	
						if (i==l || str[i]==')') break;
					}
				}
				if (i+1<l && str[i]=='/' && str[i+1]=='/') {
					i+=2;
					while (str[i]==' '||str[i]=='\t') i++;
					ac.description=str.Mid(i);
				} else {
					ac.code=str.Mid(i);
				}
			}
		} else {
			if (ac.code.Len())
				ac.code<<"\n"<<str;
			else if (str.Len())
				ac.code<<str;
		}
	}
	while (ac.code.Last()=='\r'||ac.code.Last()=='\n') ac.code.RemoveLast();
	if (name.Len()) m_list[name]=ac;
	return true;
}

//bool Autocoder::SaveToFile(wxString filename) {
//	if (!filename.Len()) filename=DIR_PLUS_FILE(config->config_dir,"autocodes");
//	wxTextFile fil(filename);
//	if (fil.Exists()) {
//		if (!fil.Open()) return false;
//	} else {
//		if (!fil.Create()) return false;
//	}
//	fil.Clear();
//	
//	fil.AddLine(m_description);
//	fil.AddLine("");
//		
//	HashStringAutoCode::iterator it;
//	for( it = m_list.begin(); it != m_list.end(); ++it ) {
//		wxString head("#autocode ");
//		head<<it->first;
//		if (it->second.args.GetCount()) {
//			head<<"(";
//			for (unsigned int i=0;i<it->second.args.GetCount();i++) 
//				head<<it->second.args[i]<<",";
//			head.RemoveLast(); head<<")";
//		}
//		if (it->second.description.Len())
//			head<<" // "<<it->second.description;
//		fil.AddLine(head);
//		fil.AddLine(it->second.code);
//		fil.AddLine("");
//	}
//	fil.Write();
//	fil.Close();
//	return true;
//}


bool Autocoder::Apply(mxSource *src, auto_code *ac, bool args) {
	wxString code=ac->code;
	if (ac->args.GetCount()) {
		if (!args) return false; // faltan los argumentos
		// armar la lista de argumentos
		wxArrayString array;
		wxString str=src->GetTextRange(src->GetTargetStart(),src->GetTargetEnd());
		int i=0, l=str.Len(); str[l-1]=',';
		while (i<l&&str[i]!='(') i++; 
		int parentesis=0,li=++i;
		while (i<l) {
			if (str[i]=='\'') {
				i++; if (str[i]=='\\') i++;
				i++;
			} else if (str[i]=='\"') {
				i++; 
				while (str[i]!='\"') {
					if (str[i]=='\\') i++; 
					i++;
				}
			} else if (str[i]=='(') {
				parentesis++;
			} else if (str[i]==')') {
				parentesis--;
			} else if (parentesis==0 && str[i]==',') {
				array.Add(str.Mid(li,i-li));li=i+1;
			}
			i++;
		}
		if (array.GetCount()!=ac->args.GetCount()) return false; // cantidad de parametros incorrecta
		// recorrer palabra por palabra de code y tratar de reemplazar
		i=0;l=code.Len();li=0; int r;
		while (i<=l) {
			char c=i<l?code[i]:'*';
			if (!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')) {
				if (li!=i) {
					r=ac->args.Index(code.Mid(li,i-li));
					if (r!=wxNOT_FOUND) {
						if (li>1&&code[li-1]=='#'&&code[li-2]=='#') { 
							li-=2; 
							code=code.Mid(0,li)+array[r]+code.Mid(i);
							i+=array[r].Len()+2-(i-li);
						} else if (li>0&&code[li-1]=='#') { 
							li--; 
							code=code.Mid(0,li)+"\""+array[r]+"\""+code.Mid(i);
							i+=array[r].Len()+2-(i-li);
						} else {
							code=code.Mid(0,li)+array[r]+code.Mid(i);
							l+=array[r].Len()-(i-li);
							i+=array[r].Len()-(i-li);
						}
					}
				}
				li=i+1;
			}
			i++;
		}
	} else 
		if (args) return false; // sobran los argumentos
	int p;
	while ( (p=code.find("#typeof("))!=wxNOT_FOUND ) {
		int p2 = p+8; while(p2<int(code.Len())&&code[p2]!='#') ++p2;
		if (p2==int(code.Len())) break;
		int aux_pos = src->GetCurrentPos();
		wxString key = code.Mid(p+8,p2-p-9);
		wxString type = StripQualifiers( src->FindTypeOfByKey(key,aux_pos,true) );
		code=code.Mid(0,p)+(type.Len()?type:"???")+code.Mid(p2+1);
	}
	int pos_cursor=code.Find("#here#");
	if (pos_cursor!=wxNOT_FOUND) code=code.Mid(0,pos_cursor)+code.Mid(pos_cursor+6);
	src->ignore_char_added=true;
	src->ReplaceTarget(src->GetTextRange(src->GetTargetStart(),src->GetTargetEnd())+"\t"); // para que ctrl+z sí ponga el tab
	src->ReplaceTarget(code);
	src->ignore_char_added=false;
	// ubicar seleccion e indentar si es necesario (solo las linea agregadas, la primera no)
	int l0=src->LineFromPosition(src->GetTargetStart());
	int l1=src->LineFromPosition(src->GetTargetEnd());
	if (pos_cursor!=wxNOT_FOUND)
		src->SetSelection(src->GetTargetStart()+pos_cursor,src->GetTargetStart()+pos_cursor);
	else
		src->SetSelection(src->GetTargetEnd(),src->GetTargetEnd());
	src->BraceHighlight(wxSTC_INVALID_POSITION,wxSTC_INVALID_POSITION);
	if (src->config_source.syntaxEnable) {
		src->Colourise(src->GetTargetStart(),src->GetTargetEnd());
		if (l0<l1) src->Indent(l0+1,l1);
	}
	return true;
}


bool Autocoder::Apply(mxSource *src) {	
	wxString line=src->GetLine(src->GetCurrentLine());
	int st=src->PositionFromLine(src->GetCurrentLine());
	int i=src->GetCurrentPos()-st-1; int s=i;
	if (i<0) return false;
	char c=line[i]; bool args=false;
	if (c==')') {
		int m=src->BraceMatch(src->GetCurrentPos()-1);
		if (m==wxSTC_INVALID_POSITION) return false;
		i=m-st;	if (i<0) return false; // tiene que estar todo en una sola linea
		if (i<s-1) args=true;
		c=line[--i];
	} 
	if ((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_') {
		int e=i+1;
		while (i>=0 && (c=line[i]) && ((c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_')) i--;
		i++; c=line[i]; if (c>='0'&&c<='9') return false;  
//		cerr<<line.Mid(i,e-i)<<endl;
		HashStringAutoCode::iterator it = m_list.find(line.Mid(i,e-i));
		if (it==m_list.end()) {
			char cping[6]="pung"; cping[1]='i';
			mxSource::UndoActionGuard undo_action(src);
			if (line.Mid(i,e-i)==cping) { // :)
				src->SetTargetStart(st+i); src->SetTargetEnd(st+s+1);
				cping[4]='\t'; cping[5]='\0';
				src->ReplaceTarget(cping);
				src->SetTargetStart(st+i); src->SetTargetEnd(st+s+2);
				src->ReplaceTarget("pong");
				return true;
			} else if (line.Mid(i,e-i)=="tic") { // X)
				src->SetTargetStart(st+i); src->SetTargetEnd(st+s+1);
				src->ReplaceTarget("tic\t");
				src->SetTargetStart(st+i); src->SetTargetEnd(st+s+2);
				src->ReplaceTarget("tac");
				return true;
			}
			return false;
		}
		src->SetTargetStart(st+i); src->SetTargetEnd(st+s+1);
		return Apply(src,&(it->second),args);
	}
	return false;
}

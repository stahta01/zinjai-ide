#include "CompilerErrorsManager.h"
#include "Cpp11.h"
#include <wx/treectrl.h>
#include "ConfigManager.h"
#include "Toolchain.h"
#include "mxMainWindow.h"
#include "compiler_strings.h"

#define CEM_SWAP_STRING_FLAG "<<cem swap>>"
#define CEM_KEEP_STRING_FLAG "<<cem keep>>"
#include "mxCompiler.h"

CompilerErrorsManager *errors_manager = nullptr;

CompilerErrorsManager::CompilerErrorsManager(CompilerTreeStruct &tree) 
	: m_timestamp(0), m_tree(tree) { Reset(false); }

void CompilerErrorsManager::Initialize (CompilerTreeStruct &tree) {
	if (errors_manager!=nullptr) return;
	errors_manager = new CompilerErrorsManager(tree);
}

void CompilerErrorsManager::Reset(bool project_mode) {
	m_full_output.Clear(); ++m_timestamp; m_errors_set.clear();
	mxSource *src = main_window ? main_window->GetCurrentSource() : nullptr;
	if (src) src->ReloadErrorsList();
	m_num_errors = m_num_warnings = 0;
	m_tree.treeCtrl->SetItemText(m_tree.errors,LANG(MAINW_CT_ERRORS,"Errores"));
	m_tree.treeCtrl->SetItemText(m_tree.warnings,LANG(MAINW_CT_WARNINGS,"Advertencias"));
	m_tree.treeCtrl->DeleteChildren(m_tree.errors);
	m_tree.treeCtrl->DeleteChildren(m_tree.warnings);
	m_tree.treeCtrl->DeleteChildren(m_tree.all);
	m_tree.treeCtrl->Collapse(m_tree.errors);
	m_tree.treeCtrl->Collapse(m_tree.warnings);
	if (project_mode) m_tree.treeCtrl->Expand(m_tree.all);
	else              m_tree.treeCtrl->Collapse(m_tree.all);
}

void CompilerErrorsManager::AddZinjaiError(bool is_error, const wxString & message) {
	m_full_output.Add(wxString("! ") << message);
	m_tree.treeCtrl->AppendItem(is_error?m_tree.errors:m_tree.warnings,message,is_error?5:7);
	++(is_error?m_num_errors:m_num_warnings);
	ShowCompilerTreePanel();
}

void CompilerErrorsManager::AddExtraOutput(CEMState &cem_state, const wxString & message) {
	cem_state.full_output.Add(message);
}

void CompilerErrorsManager::ShowCompilerTreePanel() {
	// show errors count in results tree
	wxString serrors = LANG(MAINW_CT_ERRORS,"Errores"); serrors<<" (";
	if (m_num_errors<=config->Init.max_errors) serrors<<m_num_errors;
	else                                       serrors<<config->Init.max_errors<<"+";
	serrors<<")"; m_tree.treeCtrl->SetItemText(m_tree.errors,serrors);
	if (m_num_errors) m_tree.treeCtrl->Expand(m_tree.errors);
	// show warnings count in results tree
	wxString swarnings = LANG(MAINW_CT_WARNINGS,"Advertencias"); swarnings<<" (";
	if (m_num_warnings<=config->Init.max_errors) swarnings<<m_num_warnings;
	else                                         swarnings<<config->Init.max_errors<<"+";
	swarnings<<")"; m_tree.treeCtrl->SetItemText(m_tree.warnings,swarnings);
	if (m_num_warnings) m_tree.treeCtrl->Expand(m_tree.warnings);
	if (m_num_errors||m_num_warnings) main_window->ShowCompilerTreePanel();
}

bool CompilerErrorsManager::FinishStep (CEMState &cem_state) {
	if (cem_state.HaveError()) DoAddError(cem_state);
	else if (cem_state.HaveNotes()) cem_state.parsing_was_ok = false;
	m_num_errors += cem_state.num_errors;
	m_num_warnings += cem_state.num_warnings;
	for(unsigned int i=0;i<cem_state.full_output.GetCount();i++)
		m_full_output.Add(cem_state.full_output[i]);
	if (cem_state.AddedSomething()) {
		ShowCompilerTreePanel();
		mxSource *src = main_window->GetCurrentSource();
		if (src) src->ReloadErrorsList();
	}
	return cem_state.parsing_was_ok;
}

bool CompilerErrorsManager::CompilationFinished () {
	mxSource *src = main_window->GetCurrentSource();
	if (src) src->ReloadErrorsList();
	ShowCompilerTreePanel();
	return true;
}

CompilerErrorsManager::CEMState CompilerErrorsManager::CommonInit(const wxString &command, bool really_compiling) {
	CEMState state;
	state.all_item = AddTreeMessageNode(m_tree.all,command,2);
	state.really_compiling = really_compiling;
	state.parsing_was_ok = true;
	state.is_error = true; 
	state.num_errors = state.num_warnings = 0;
	state.full_output.Add("");
	state.full_output.Add(wxString("> ")+command);
	state.full_output.Add("");
	return state;
}

CompilerErrorsManager::CEMState CompilerErrorsManager::InitCompiling(const wxString &command) {
	return CommonInit(command,true);
}

CompilerErrorsManager::CEMState CompilerErrorsManager::InitLinking(const wxString &command) {
	return CommonInit(command,false);
}

CompilerErrorsManager::CEMState CompilerErrorsManager::InitOtherStep(const wxString &command) {
	return CommonInit(command,false);
}

CompilerErrorsManager::CEMState CompilerErrorsManager::InitExtern(const wxString &command) {
	return CommonInit(command,false);
}

/// hace más legibles errores reemplazando templates y tipos conocidos (como allocators, char_traits, etc)
static void UnSTD(wxString &line) {
	int p;
	while ((p=line.find("[with "))!=wxNOT_FOUND) { // buscar un argumento actual para plantilla
		int p0=p;  // pos donde inicia el argumento actual "[with booga=...."
		int p1=p;  // pos donde termina el argumento actual "...]"
		int lev=0; // auxiliar para ignorar corchetes anidados
		p+=6; // salteat el "[with " y buscar el corchete que cierra
		while (p1<int(line.Len())&&(lev>0||line[p1]!=']')) {
			if (line[p1]=='[') lev++;
			else if (line[p1]==']') lev--;
			p1++;
		}
		int pos_last_igual=p0; 
		int pos_igual=0; // posición donde encuentra un = que corresponda a esa lista de argumentos
		while (pos_last_igual<p1 && (pos_igual=line.SubString(pos_last_igual+1,p1).Find("="))!=wxNOT_FOUND) {
			pos_igual+=pos_last_igual+1; // para que quede como pos absoluta
			pos_last_igual=pos_igual;
			
			// buscar que hay a la izquierda del igual
			int pre_fin=pos_igual-1;  // pos donde termina el termino de la izquierda
			while (pre_fin>0&&line[pre_fin]==' ') pre_fin--;
			int pre_ini=pre_fin; // pos donde empieza el termino de la izquierda
			lev=0; // no deberia ser necesario
			while (pre_ini>=p0+6&&(lev>0||(line[pre_ini]!='['&&line[pre_ini]!=','))) {
				if (line[pre_ini]=='>'||line[pre_ini]==')') lev++;
				else if (line[pre_ini]=='<'||line[pre_ini]=='(') lev--;
				pre_ini--;
			}
			pre_ini++; 
			while (pre_ini<pre_fin&&line[pre_ini]==' ') pre_ini++;
			wxString type_in=line.SubString(pre_ini,pre_fin); // tipo que sera reemplazado
			
			// buscar que hay a la derecha del igual
			int pos_ini=pos_igual+1; // pos donde empieza el termino de la derecha
			while (pos_ini<p1&&line[pos_ini]==' ') pos_ini++;
			int pos_fin=pos_ini; // pos donde termina el termino de la derecha
			lev=0; // no deberia ser necesario
			while (pos_fin<p1&&(lev>0||(line[pos_fin]!=']'&&line[pos_fin]!=','))) {	
				if (line[pos_fin]=='<'||line[pos_fin]=='(') lev++;
				else if (line[pos_fin]==')'||line[pos_fin]=='>') lev--;
				pos_fin++;
			}
			pos_fin--;
			while (pos_fin>pos_ini&&line[pos_fin]==' ') pos_fin--;
			wxString type_out=line.SubString(pos_ini,pos_fin); // tipo con el cual se reemplazará
			
			int p_in=0, diff=type_out.size()-type_in.size();
			while ((p=line.SubString(p_in,p0).Find(type_in))!=wxNOT_FOUND && p+p_in<p0) {
				p+=p_in; // pos absoluta donde reemplazar
				line.replace(p,type_in.size(),type_out);
				p0+=diff; p1=diff; pos_last_igual+=diff; // corregir las pos posteriores que interesen mantener
				p_in=p+type_out.size(); // para que la proxima busqueda empiece despues (porque puede aparecer "[with t=t]")
			}
			
		}
		line.erase(p0,p1-p0+1);
	}
	
	// quitar los allocators y otras cosas
	while ((p=line.Find("std::allocator<"))!=wxNOT_FOUND) { 
		int p2=p+15, lev=0;
		// buscar donde empieza (puede haber un const y espacios antes)
		while (p>1&&line[p-1]==' ') p--;
		if (p>5 && line.Mid(p-5,5)=="const") {
			p-=5;
			while (p>1 && line[p-1]==' ') p--;
		}
		// buscar donde termina el argumento de su template
		while (p2<int(line.Len())&&(line[p2]!='>'||lev>0)) {
			if (line[p2]=='<') lev++;
			else if (line[p2]=='>') lev--;
			p2++;
		}
		// saltar espacios y &
		while (p2<int(line.Len())&&(line[p2+1]==' '||line[p2+1]=='&')) p2++; 
		// quitar una coma si estaba en una lista de argumentos
		if (line[p-1]==',') { p--; while (p>1&&line[p-1]==' ') p--; }
		else if (line[p2]==',') { p2++; while (p2<int(line.Len())&&line[p2]==' ') p2++; }
		line=line.Mid(0,p)+line.Mid(p2+1);
	}
	while ((p=line.Find("std::basic_string<"))!=wxNOT_FOUND) { 
		int p2=p+18, lev=0;
		while (p>1&&line[p-1]==' ') p--;
		if (p>5 && line.Mid(p-5,5)=="const") {
			p-=5;
			while (p>1 && line[p-1]==' ') p--;
		}
		while (p2<int(line.Len())&&(line[p2]!='>'||lev>0)) {
			if (line[p2]=='<') lev++;
			else if (line[p2]=='>') lev--;
			p2++;
		}
		line=line.Mid(0,p)+"string"+line.Mid(p2+1);
	}
	// cambiar los "basic_strig" por "string"
	p=0; int p2;
	while ((p2=line.Mid(p).Find("basic_string"))!=wxNOT_FOUND) { 
		p+=p2;
		if (p>0&&((line[p-1]|32)<'a'||(line[p-1]|32)>'z')&&((line[p+12]|32)<'a'||(line[p+12]|32)>'z'))
			line.erase(p,6);
		else
			p+=12;
	}
	// quitar los "std::"
	p=0;
	while ((p2=line.Mid(p).Find("std::"))!=wxNOT_FOUND) { 
		p+=p2;
		if (p>0&&((line[p-1]|32)<'a'||(line[p-1]|32)>'z'))
			line.erase(p,5);
		else
			p+=5;
	}
	line.Replace("typename::rebind<char>::other::size_type","size_t");
}

wxString GetNiceErrorLine(const wxString &error_line) {
	wxString nice_error_line;
	// acortar el nombre de archivo
	int l = error_line.Len(), p=0;
	if ( l>2 && error_line[1]==':' ) p=2;
	while (p<l && error_line[p]!=':') p++;
	if (p<l) {
		while (p>=0 && error_line[p]!='/' && error_line[p]!='\\') p--;
		nice_error_line = error_line.Mid(p+1);
	} else 
		nice_error_line = error_line;
	if (config->Init.beautify_compiler_errors && !current_toolchain.IsExtern()) UnSTD(nice_error_line);
	return nice_error_line;
}

wxTreeItemId CompilerErrorsManager::AddTreeErrorNode(wxTreeItemId &where, const wxString &full_error, const wxString &nice_error, int icon) {
	return m_tree.treeCtrl->AppendItem(where,nice_error,icon,-1,new mxCompilerItemData(full_error));
}
wxTreeItemId CompilerErrorsManager::AddTreeMessageNode(wxTreeItemId &where, const wxString &message, int icon) {
	return m_tree.treeCtrl->AppendItem(where,message,icon);
}

static wxString GetMessage(const wxString &full_line, bool keep_loc = false) {
	wxString retval = full_line;
	if (keep_loc) return full_line;
	// extract file name, line number, and column number
	int l = full_line.Len(), p = 0;
	while(++p<l) {
		if (full_line[p-1]==':' && full_line[p]==' ') {
			retval = full_line.Mid(p+1);
			l-=p+1;
			break;
		}
	}
	// extract file name the final "[-Wsomething]"
	if (l && retval[l-1]==']') {
		p = l-1;
		while (--p>0) {
			if (retval[p]=='[') {
				if (retval[p+1]=='-' && retval[p+2]=='W')
					retval = retval.Mid(0,p);
				break;
			}
		}
	}
	return retval;
}

void CompilerErrorsManager::DoAddError (CEMState &cem_state) {
	wxString main_full = cem_state.full_error;
	wxString main_nice = GetNiceErrorLine(main_full);
	wxTreeItemId main_item 
		= AddTreeErrorNode( cem_state.is_error?m_tree.errors:m_tree.warnings,
						   main_full, main_nice, cem_state.is_error?4:3);
	++(cem_state.is_error?cem_state.num_errors:cem_state.num_warnings);
	wxString margin_error = GetMessage(main_nice,false);
	
	wxString prev_nice;
	wxTreeItemId prev_item; bool have_prev_item = false;
	for(size_t i=0;i<cem_state.notes.size();i++) { 
		wxString &note_full = cem_state.notes[i].message;
		wxString note_nice = GetNiceErrorLine(note_full);
		int flags = cem_state.notes[i].flags;
		if (flags&CAR_EL_FLAG_NESTED) {
			if (have_prev_item) 
				AddTreeErrorNode(prev_item,note_full,note_nice,5);
			else
				prev_item = AddTreeErrorNode(main_item,note_full,note_nice,5);
			have_prev_item = true;
		} else {
			have_prev_item = false;
			AddTreeErrorNode(main_item,note_full,note_nice,5);
		}
		if (flags&CAR_EL_FLAG_SWAP) {
			ErrorLineParts note_prts(note_full);
			wxString margin_note = GetMessage(main_nice) + "\n   "
								   +GetMessage((flags&CAR_EL_FLAG_USE_PREV)?prev_nice:note_nice);
			CEMError aux_err(note_prts.line,margin_note,cem_state.is_error);
			m_errors_set[note_prts.fname].push_back(aux_err);
		}
		margin_error << "\n   " << GetMessage(note_nice,flags&CAR_EL_FLAG_KEEP_LOC);
		prev_nice = note_nice;
	}
	
	ErrorLineParts main_prts(main_full);
	CEMError aux_err(main_prts.line,margin_error,cem_state.is_error);
	m_errors_set[main_prts.fname].push_back(aux_err);
	
//	if (parts.line!=-1) {
//		vector<CEMError> &v = m_errors_set[parts.fname];
//		v.push_back(CEMError(parts.line,GetMessage(nice_error),is_error));
//		cem_state.last_vector_ptr = &v;
//		cem_state.last_vector_idx = v.size()-1;
//	}
//	m_tree.treeCtrl->AppendItem(cem_state.all_item,error_line,6,-1);
//	cem_state.last_is_ok = true;
//	
//	
//	if (!cem_state.pending_lines.IsEmpty()) { // agregar los hijos que estaban pendientes
//		bool next_swaps = false, next_keeps_loc = false;
//		wxString previous_error_message;
//		wxTreeItemId tree_item = cem_state.last_item;
//		for(unsigned int i=0;i<cem_state.pending_lines.GetCount();i++) {
//			if (cem_state.pending_lines[i] == CEM_SWAP_STRING_FLAG) { next_swaps = true; continue; }
//			if (cem_state.pending_lines[i] == CEM_KEEP_STRING_FLAG) { next_keeps_loc = true; continue; }
//			nice_error = GetNiceErrorLine(cem_state.pending_lines[i]);
//			wxTreeItemId new_item = AddTreeErrorNode(tree_item,cem_state.pending_lines[i],nice_error,5);
//			cem_state.GetLastMessage() << "\n    "<<GetMessage(nice_error);
//			if (next_swaps) {
//				ErrorLineParts parts(cem_state.pending_lines[i]);
//				wxString prev_error = cem_state.pending_lines[i>1?i-2:i];
//				bool is_error = !error_line.Contains(EN_COMPOUT_WARNING) && !error_line.Contains(ES_COMPOUT_WARNING);
//				if (parts.line!=-1) m_errors_set[parts.fname].push_back(CEMError(parts.line,GetMessage(prev_error)+"\n   "+GetMessage(error_line),is_error));
//			}
//			previous_error_message = nice_error;
//			if (i==0) tree_item = new_item;
//			next_keeps_loc = next_swaps = false;
//		}
//		cem_state.pending_lines.Clear();
//	}
	cem_state.ClearError();
}

void CompilerErrorsManager::AddToFullOutput(CompilerErrorsManager::CEMState &cem_state, const wxString &error_line) {
	cem_state.full_output.Add(error_line);
	AddTreeMessageNode(cem_state.all_item,error_line,6);
}

void CompilerErrorsManager::AddError (CEMState & cem_state, bool is_error, const wxString &error_line) {
	AddToFullOutput(cem_state,error_line);
	if (cem_state.HaveError()) DoAddError(cem_state);
	cem_state.SetError(is_error,error_line);
}

void CompilerErrorsManager::AddNoteForLastOne (CEMState & cem_state, const wxString &error_line, int flags) {
	AddToFullOutput(cem_state,error_line);
	if (!cem_state.HaveError()) cem_state.parsing_was_ok = false;
	else cem_state.AddNote(error_line,flags);
//	if (!cem_state.last_is_ok) return false;
//	wxString nice_error = GetNiceErrorLine(error_line);
//	cem_state.GetLastMessage() << "\n    " << GetMessage(nice_error);
//	AddTreeErrorNode(cem_state.last_item,error_line,nice_error,5);
//	if (and_swap) {
//		// won't swap anymore, but will mark in margin
//		wxString error_parent = m_tree.treeCtrl->GetItemText(cem_state.last_item);
//		ErrorLineParts parts(error_line);
//		bool is_error = !error_parent.Contains(EN_COMPOUT_WARNING) && !error_parent.Contains(ES_COMPOUT_WARNING);
//		if (parts.line!=-1) m_errors_set[parts.fname].push_back(CEMError(parts.line,GetMessage(nice_error)+"\n   "+GetMessage(error_parent),is_error));
//	}
//	return true;
}

void CompilerErrorsManager::AddNoteForNextOne (CEMState & cem_state, const wxString &error_line, int flags) {
	AddToFullOutput(cem_state,error_line);
	if (cem_state.HaveError()) DoAddError(cem_state);
	cem_state.AddNote(error_line,flags);
//	cem_state.last_is_ok = false;
//	if (and_swap) cem_state.pending_lines.Add(CEM_SWAP_STRING_FLAG);
//	if (keep_loc) cem_state.pending_lines.Add(CEM_KEEP_STRING_FLAG);
//	cem_state.pending_lines.Add(error_line);
//	return true;
}

wxString CEMReference::GetMessageForLine(mxSource *src, int src_line) {
	m_aux_error_msg.Clear();
	for(int i=0;i<m_markers.GetSize();i++) { 
		int foo = src->MarkerLineFromHandle(m_markers[i].marker_handler);
		if (src->MarkerLineFromHandle(m_markers[i].marker_handler)==src_line)
			GetMessageForLine(m_markers[i].original_error_line,false);
	}
	return m_aux_error_msg;
}

int CEMReference::FixLineNumber(mxSource *src, int error_line) const {
	if (!IsOk()) return error_line;
	for(int i=0;i<m_markers.GetSize();i++) {
		if (m_markers[i].original_error_line==error_line) {
			int src_line = src->MarkerLineFromHandle(m_markers[i].marker_handler);
			if (src_line!=-1) return src_line+1;
		}
	}
	return error_line;
}

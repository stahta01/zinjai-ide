#ifndef COMPILERERRORSMANAGER_H
#define COMPILERERRORSMANAGER_H
#include <wx/string.h>
#include <wx/treebase.h>
#include <map>
#include <vector>
#include "Cpp11.h"
using namespace std;

class CompilerTreeStruct;

//! Informacion asociada a un item del arbol de resultados de compilacion, para guardar los que no se ve (por ejemplo el path completo)
class mxCompilerItemData:public wxTreeItemData {
public:
	wxString file_info;
	mxCompilerItemData(wxString fi):file_info(fi) {};
	void Set(wxString &s) { file_info=s; }
};

struct ErrorLineParts {
	long line, column;
	wxString fname;
//	wxString message;
	ErrorLineParts(const wxString &full_error) {
		int l = full_error.Len(), p1 = 0;
		if (full_error.Len()>2 && full_error[1]==':') p1=2;
		while (p1<l && full_error[p1]!=':') p1++;
		fname = full_error.Mid(0,p1);
		int p2 = p1+1;
		while (p2<l && full_error[p2]!=':') p2++;
		if (!full_error.Mid(p1+1,p2-p1-1).ToLong(&line)) line = -1;
		int p3 = p2+1;
		if (p3<l && (full_error[p3]>='0'&&full_error[p3]<='9')) {
			while (p3<l && full_error[p3]!=':') p3++;
			if (!full_error.Mid(p2+1,p3-p2-1).ToLong(&column)) column = -1;
//			++p3;
		} else column = -1;
//		message = full_error.Mid(p3+1);
	}
};

class CompilerErrorsManager {
public:

	struct CEMError {
		int line;
		wxString message;
		bool is_error;
		CEMError() : line(-1) {}
		CEMError(int l, const wxString &msg, bool error) : line(l), message(msg), is_error(error) {}
	};
	
	struct CEMState {
		// overall process state
		wxTreeItemId all_item;
		wxArrayString full_output;
		bool really_compiling, parsing_was_ok;
		int num_errors, num_warnings;
		bool AddedSomething() const { return num_errors+num_warnings>0; }
		// current errors state
		wxString full_error;
		bool is_error; // or warning
		struct CEMLine {
			int flags;
			wxString message;
			CEMLine(const wxString &str, int f = 0) : flags(f), message(str) {}
		};
		vector<CEMLine>notes;
		void SetError(bool error, const wxString &message) {
			is_error = error; full_error = message;
		}
		void AddNote(const wxString &note, int flags) {
			notes.push_back(CEMLine(note,flags));
		}
		void ClearError() { full_error.Clear(); notes.clear(); }
		bool HaveError() const { return !full_error.IsEmpty(); }
		bool HaveNotes() const { return !notes.empty(); }
	};
	
private:
	map<wxString,vector<CEMError> > m_errors_set;
	
	unsigned int m_timestamp;
	friend class CEMReference;
	
	CompilerTreeStruct &m_tree;
	int m_num_errors, m_num_warnings;
	
	wxArrayString m_full_output;
	
	wxTreeItemId AddTreeErrorNode(wxTreeItemId &where, const wxString &full_error, const wxString &nice_error, int icon);
	wxTreeItemId AddTreeMessageNode(wxTreeItemId &where, const wxString &message, int icon);
	void ShowCompilerTreePanel();
	
	CompilerErrorsManager(CompilerTreeStruct &tree);
	
	// non-copyable
	CompilerErrorsManager& operator=(const CompilerErrorsManager &tree);
	CompilerErrorsManager(const CompilerErrorsManager &tree);
	
	CEMState CommonInit(const wxString &command, bool really_compiling);
	void AddToFullOutput(CEMState &cem_state, const wxString &error_line);
	void DoAddError(CEMState &cem_state);
	
public:
	static void Initialize(CompilerTreeStruct &tree);
	
	bool GetCurrentTimeStamp() const { return m_timestamp; }
	
	void Reset(bool project_mode);
	
	const wxArrayString &GetFullOutput() const { return m_full_output; }
	
	CEMState InitCompiling(const wxString &command);
	CEMState InitLinking(const wxString &command);
	CEMState InitOtherStep(const wxString &command);
	CEMState InitExtern(const wxString &command);
	
	void AddZinjaiError(bool is_error, const wxString &message);
	void AddExtraOutput(CEMState &cem_state, const wxString &error_line);
	void AddError(CEMState &cem_state, bool is_error, const wxString &error_line);
	void AddNoteForLastOne(CEMState &cem_state, const wxString &error_line, int flags);
	void AddNoteForNextOne(CEMState &cem_state, const wxString &error_line, int flags);
	
	bool FinishStep(CEMState &cem_state);
	
	bool CompilationFinished();
	
};

extern CompilerErrorsManager *errors_manager;

class CEMReference {
	vector<CompilerErrorsManager::CEMError> *m_errors;
	unsigned int m_timestamp;
	wxString m_fname, m_aux_error_msg;
public:
	CEMReference() : m_errors(nullptr), m_timestamp(0) {}
	void SetFName(const wxString &fname) { m_fname = fname; }
	bool Update() { 
		m_timestamp = errors_manager->m_timestamp;
		map<wxString,vector<CompilerErrorsManager::CEMError> >::iterator it 
			= errors_manager->m_errors_set.find(m_fname);
		m_errors = it==errors_manager->m_errors_set.end() ? nullptr : &(it->second);
		return m_errors!=nullptr;
	}
	bool IsOk() const { return m_errors && errors_manager->m_timestamp==m_timestamp; }
	vector<CompilerErrorsManager::CEMError> &GetErrors() const { return *m_errors; }
	wxString GetMessageForLine(int l) {
		m_aux_error_msg.Clear();
		if (IsOk()) {
			const vector<CompilerErrorsManager::CEMError> &v = GetErrors();
			for(size_t i=0;i<v.size();i++) { 
				if (l == v[i].line) {
					if (m_aux_error_msg.IsEmpty()) m_aux_error_msg << v[i].message;
					else                           m_aux_error_msg << "\n" << v[i].message;
				}
			}
		}
		return m_aux_error_msg;
	}
};

#endif



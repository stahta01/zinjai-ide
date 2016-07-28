#ifndef MXLIBTOBUILDWINDOW_H
#define MXLIBTOBUILDWINDOW_H
#include <wx/dialog.h>
#include "Cpp11.h"
#include "mxCommonConfigControls.h"

class project_configuration;
class project_library;
class wxTextCtrl;
class wxComboBox;
class wxListBox;
class wxCheckBox;
class mxProjectConfigWindow;

class mxLibToBuildWindow : public mxDialog {
private:
	mxProjectConfigWindow *m_parent;
	bool m_constructed;
	project_configuration *m_configuration;
	project_library *m_lib;
	wxTextCtrl *m_name;
	wxTextCtrl *m_path;
	wxTextCtrl *m_filename;
	wxTextCtrl *m_extra_link;
	wxListBox *m_sources_in;
	wxListBox *m_sources_out;
	wxCheckBox *m_default_lib;
	wxCheckBox *m_do_link;
	wxComboBox *m_type;
public:
	static wxString new_name;
	mxLibToBuildWindow(mxProjectConfigWindow *aparent, project_configuration *conf, project_library *alib=nullptr);
	void OnHelpButton(wxCommandEvent &evt);
	void OnOkButton(wxCommandEvent &evt);
	void OnInButton(wxCommandEvent &evt);
	void OnOutButton(wxCommandEvent &evt);
	void OnCancelButton(wxCommandEvent &evt);
	void OnCombo(wxCommandEvent &evt);
	void SetFName();
	DECLARE_EVENT_TABLE();
};

#endif


#ifndef MX_TIPS_WINDOW_H
#define MX_TIPS_WINDOW_H

#include <wx/dialog.h>
#include <wx/arrstr.h>
#include "mxUtils.h"
#include "Cpp11.h"
#include "raii.h"

class mxBitmapButton;
class wxBoxSizer;
class wxPanel;
class mxMainWindow;
class wxButton;
class wxRadioBox;
class wxCheckBox;
class wxComboBox;
class wxListBox;
class wxTextCtrl;
class wxStaticText;

enum WizardModes { 
	WM_Empty = 0,
	WM_Simple = 1,
	WM_Project = 2,
	WM_Import = 3,
	WM_Null
};

class mxNewWizard : public wxDialog {
public:
	
	WizardModes current_wizard_mode;

	bool only_for_project; ///< indica si el cuadro se abrio para la opcion "Nuevo..." (general) o "Nuevo Proyecto..."
	BoolFlag mask_folder_change_events; ///< para evitar mover el radiobutton al cambiar el texto de la carpeta de proyecto por un click en el mismo radio button
	
	int size_w, size_h;
	
	wxRadioBox *onproject_radio;
	wxStaticText *onproject_label;
	wxStaticText *onproject_inherit_label;
	wxStaticText *onproject_inherit_include;
	wxTextCtrl *onproject_name;
	wxCheckBox *onproject_const_def;
	wxCheckBox *onproject_const_copy;
	wxCheckBox *onproject_dest;
	wxComboBox *onproject_inherit_visibility[5];
	wxTextCtrl *onproject_inherit_class[5];
	
	wxRadioBox *start_radio;
	wxStaticText *start_tooltip;
	wxCheckBox *templates_check;
	wxListBox *templates_list;

	wxTextCtrl *project_full_path;
	wxTextCtrl *project_name;
	wxRadioBox *project_folder_radio;
	wxTextCtrl *project_folder_path;
	wxCheckBox *project_check;
	wxCheckBox *project_current_files;
	wxCheckBox *project_dir_files;
	wxCheckBox *project_folder_create;
	wxListBox *project_list;
	wxArrayString project_templates;	
	int project_default;
	
	wxBoxSizer *sizer_for_panel;
	wxPanel *panel;
	mxBitmapButton *nextButton;
	mxBitmapButton *cancelButton;
 
	wxPanel *panel_onproject;
	void CreatePanelOnProject();
	void ShowPanelOnProject();
	wxPanel *panel_start;
	void CreatePanelStart();
	void ShowPanelStart(bool show=false);
	wxPanel *panel_templates;
	void CreatePanelTemplates();
	void ShowPanelTemplates();
	wxPanel *panel_project_1;
	void CreatePanelProject1();
	void ShowPanelProject1();
	wxPanel *panel_project_2;
	void CreatePanelProject2();
	void ShowPanelProject2();

private:
	mxNewWizard(mxMainWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxALWAYS_SHOW_SB | wxALWAYS_SHOW_SB | wxDEFAULT_FRAME_STYLE | wxSUNKEN_BORDER);
public:
	static mxNewWizard *GetInstance();
	static void DeleteInstance() { if (mxNewWizard::instance) mxNewWizard::instance->Destroy(); }
	~mxNewWizard() { mxNewWizard::instance=nullptr; }

	void OnButtonNext(wxCommandEvent &event);
	void OnButtonCancel(wxCommandEvent &event);
	void OnButtonHelp(wxCommandEvent &event);
	void OnButtonFolder(wxCommandEvent &event);
	void OnStartRadio(wxCommandEvent &event);
	void OnProjectRadio(wxCommandEvent &event);
	void OnProjectPathRadio(wxCommandEvent &event);
	void OnClose(wxCloseEvent &event);
	void RunWizard(wxString how="simple");
	
	void ProjectCreate();
	void OnProjectCreate();
	
	void OnOnProjectNameChange(wxCommandEvent &evt);
	void OnProjectFolderCheck(wxCommandEvent &evt);
	void OnProjectNameChange(wxCommandEvent &evt);
	void OnProjectFolderChange(wxCommandEvent &evt);
	void UpdateProjectFullPath();
	
	void OnProjectFilesOpenCheck(wxCommandEvent &evt);
	void OnProjectFilesDirCheck(wxCommandEvent &evt);
	
	void OnButtonNewFilePath(wxCommandEvent &evt);

private:
	static mxNewWizard *instance;
	
	void SetCurrentPanel(wxPanel *new_panel);
	
	DECLARE_EVENT_TABLE()

};

#endif

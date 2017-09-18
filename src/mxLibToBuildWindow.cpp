#include "mxLibToBuildWindow.h"
#include "ids.h"
#include "ProjectManager.h"
#include "Language.h"
#include "mxSizers.h"
#include "mxArt.h"
#include "mxBitmapButton.h"
#include "mxHelpWindow.h"
#include "mxMessageDialog.h"
#include "mxProjectConfigWindow.h"
#include "mxCommonConfigControls.h"

BEGIN_EVENT_TABLE(mxLibToBuildWindow, wxDialog)
	
	EVT_BUTTON(mxID_LIBS_IN,mxLibToBuildWindow::OnInButton)
	EVT_BUTTON(mxID_LIBS_OUT,mxLibToBuildWindow::OnOutButton)
	EVT_BUTTON(wxID_CANCEL,mxLibToBuildWindow::OnCancelButton)
	EVT_BUTTON(wxID_OK,mxLibToBuildWindow::OnOkButton)
	EVT_BUTTON(mxID_HELP_BUTTON,mxLibToBuildWindow::OnHelpButton)
	EVT_TEXT(wxID_ANY,mxLibToBuildWindow::OnCombo)
END_EVENT_TABLE()
	
wxString mxLibToBuildWindow::new_name;
	
mxLibToBuildWindow::mxLibToBuildWindow(mxProjectConfigWindow *parent, project_configuration *conf, project_library *lib) : 
	mxDialog(parent,LANG(LIBTOBUILD_CAPTION,"Generar biblioteca")), 
	m_parent(parent), m_constructed(false), m_configuration(conf), m_lib(lib)
{
	CreateSizer sizer(this);
	
	sizer.BeginText(LANG(LIBTOBUILD_NAME,"Nombre")).Value(m_lib?m_lib->libname:"").EndText(m_name);
	sizer.BeginText(LANG(LIBTOBUILD_DIR,"Directorio de salida")).Value(m_lib?m_lib->m_path:"${TEMP_DIR}").EndText(m_path);
	sizer.BeginText(LANG(LIBTOBUILD_FILE,"Archivo de salida")).ReadOnly().EndText(m_filename);
	sizer.BeginText(LANG(LIBTOBUILD_EXTRA_LINK,"Opciones de enlazado")).Value(m_lib?m_lib->extra_link:"").EndText(m_extra_link);
	
	sizer.BeginCombo(LANG(LIBTOBUILD_TYPE,"Tipo de biblioteca"))
		.Add(LANG(LIBTOBUILD_DYNAMIC,"Dinamica")).Add(LANG(LIBTOBUILD_STATIC,"Estatica"))
		.Select(m_lib?(m_lib->is_static?1:0):0).EndCombo(m_type);
		
	wxSizer *src_sizer = new wxBoxSizer(wxHORIZONTAL);
	wxSizer *szsrc_buttons = new wxBoxSizer(wxVERTICAL);
	szsrc_buttons->Add(new wxButton(this,mxID_LIBS_IN,">>>",wxDefaultPosition,wxSize(50,-1)),sizers->BA10_Exp0);
	szsrc_buttons->Add(new wxButton(this,mxID_LIBS_OUT,"<<<",wxDefaultPosition,wxSize(50,-1)),sizers->BA10_Exp0);
	wxSizer *szsrc_in = new wxBoxSizer(wxVERTICAL);
	szsrc_in->Add(new wxStaticText(this,wxID_ANY,LANG(LIBTOBUILD_SOURCES_IN,"Fuentes a incluir")),sizers->Exp0);
	m_sources_in = new wxListBox(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,0,nullptr,wxLB_SORT|wxLB_EXTENDED|wxLB_NEEDED_SB);
	m_sources_in->SetMinSize(wxSize(250,50));
	szsrc_in->Add(m_sources_in,sizers->Exp1);
	wxSizer *szsrc_out = new wxBoxSizer(wxVERTICAL);
	szsrc_out->Add(new wxStaticText(this,wxID_ANY,LANG(LIBTOBUILD_SOURCES_OUT,"Fuentes a excluir")),sizers->Exp0);
	m_sources_out = new wxListBox(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,0,nullptr,wxLB_SORT|wxLB_EXTENDED|wxLB_NEEDED_SB);
	m_sources_out->SetMinSize(wxSize(250,50));
	szsrc_out->Add(m_sources_out,sizers->Exp1);
	src_sizer->Add(szsrc_out,sizers->Exp1);
	src_sizer->Add(szsrc_buttons,sizers->Center);
	src_sizer->Add(szsrc_in,sizers->Exp1);
	sizer.GetSizer()->Add(src_sizer,sizers->BA10_Exp1);
	
	sizer.BeginCheck(LANG(LIBTOBUILD_DEFAULT,"Biblioteca por defecto para nuevos fuentes")).Value(m_lib?m_lib->default_lib:false).EndCheck(m_default_lib);
	sizer.BeginCheck(LANG(LIBTOBUILD_DO_LINK,"Enlazar en el ejecutable del proyecto")).Value(m_lib?m_lib->do_link:true).EndCheck(m_do_link);
	
	sizer.BeginBottom().Help().Cancel().Ok().EndBottom(this);
	sizer.SetAndFit();
	
	project->AssociateLibsAndSources(m_configuration);

	for(LocalListIterator<project_file_item*> item(&project->files.sources); item.IsValid(); item.Next()) {
		if (m_lib && item->GetLibrary()==m_lib)
			m_sources_in->Append(item->GetRelativePath());
		else if (!item->GetLibrary())
			m_sources_out->Append(item->GetRelativePath());
		
	}
	
	m_constructed = true;
	SetFName();
	m_name->SetFocus();
	ShowModal();
}

void mxLibToBuildWindow::OnOkButton(wxCommandEvent &evt) {
	new_name = m_name->GetValue();
	if (new_name.Len()==0) {
		mxMessageDialog(this,LANG(LIBTOBUILD_NAME_MISSING,"Debe completar el nombre."))
			.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
		return;
	}
	if (!m_lib) {
		if (project->GetLibToBuild(m_configuration,m_name->GetValue())) {
			mxMessageDialog(this,LANG(LIBTOBUILD_NAME_REPEATED,"Ya existe una biblioteca con ese nombre."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			return;
		}
		m_lib = project->AppendLibToBuild(m_configuration);
	}
	m_lib->libname = new_name;
	m_lib->m_path = m_path->GetValue();
//	lib->filename = filename->GetValue();
	m_lib->extra_link = m_extra_link->GetValue();
	m_lib->is_static = m_type->GetSelection()==1;
	m_lib->do_link = m_do_link->GetValue();
	m_lib->default_lib = m_default_lib->GetValue();
	if (m_lib->default_lib) {
		for(JavaVectorIterator<project_library> it(m_configuration->libs_to_build);it.IsValid();it.Next()) { 
			if (it!=m_lib) it->default_lib = false;
		}
	}
	for (LocalListIterator<project_file_item*> fi(&project->files.sources);
		 fi.IsValid() ; fi.Next()) 
	{
		if (m_sources_in->FindString(fi->GetRelativePath())!=wxNOT_FOUND) {
#ifndef __WIN32__
			if (!fi->IsInALibrary()) fi->ForceRecompilation(); // por el fPIC
#endif
			fi->SetLibrary(m_lib);
		} else if (fi->GetLibrary() == m_lib) {
#ifndef __WIN32__
			if (!fi->IsInALibrary()) fi->ForceRecompilation(); // por el fPIC
#endif
			fi->UnsetLibrary();
		}
	}
	project->SaveLibsAndSourcesAssociation(m_configuration);
	m_lib->need_relink = true;
	if (m_lib->do_link) m_parent->linking_force_relink->SetValue(true);
	Close();
}

void mxLibToBuildWindow::OnCancelButton(wxCommandEvent &evt) {
	new_name = "";
	Close();
}

void mxLibToBuildWindow::OnHelpButton(wxCommandEvent &evt) {
	mxHelpWindow::ShowHelp("lib_to_build.html",this);
}

void mxLibToBuildWindow::OnOutButton(wxCommandEvent &evt) {
	m_sources_out->SetSelection(wxNOT_FOUND);
	for (int i=m_sources_in->GetCount();i>=0;i--)
		if (m_sources_in->IsSelected(i)) {
			m_sources_out->Append(m_sources_in->GetString(i));
			m_sources_out->Select(m_sources_out->FindString(m_sources_in->GetString(i)));
			m_sources_in->Delete(i);
		}
}

void mxLibToBuildWindow::OnInButton(wxCommandEvent &evt) {
	m_sources_in->SetSelection(wxNOT_FOUND);
	for (int i=m_sources_out->GetCount();i>=0;i--)
		if (m_sources_out->IsSelected(i)) {
			m_sources_in->Append(m_sources_out->GetString(i));
			m_sources_in->Select(m_sources_in->FindString(m_sources_out->GetString(i)));
			m_sources_out->Delete(i);
		}
}

void mxLibToBuildWindow::OnCombo(wxCommandEvent &evt) {
	wxObject *w = evt.GetEventObject();
	if (w==m_name || w==m_type || w==m_path)
		SetFName();
}
void mxLibToBuildWindow::SetFName() {
	if (!m_constructed) return;
	bool is_static = m_type->GetSelection()==1;
	wxString fname = DIR_PLUS_FILE(m_path->GetValue(),wxString("lib")<<m_name->GetValue());
	mxUT::ParameterReplace(fname,"${TEMP_DIR}",project->active_configuration->temp_folder);
#ifdef __WIN32__
	fname << (is_static?".a":".dll");
#else
	fname << (is_static?".a":".so");
#endif
	m_filename->SetValue(fname);
}

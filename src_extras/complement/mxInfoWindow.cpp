#include "mxInfoWindow.h"
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include "ComplementArchive.h"
#include <wx/stattext.h>
#include <wx/msgdlg.h>
#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include "Application.h"
#include "../../src/version.h"

BEGIN_EVENT_TABLE(mxInfoWindow,wxFrame)
	EVT_BUTTON(wxID_OK,mxInfoWindow::OnButtonOk)
	EVT_BUTTON(wxID_CANCEL,mxInfoWindow::OnButtonCancel)
	EVT_CLOSE(mxInfoWindow::OnClose)
END_EVENT_TABLE()

mxInfoWindow *info_win=NULL;
	
enum STEPS_INFO {STEP_READING_INFO,STEP_SHOWING_INFO,STEP_CREATING_DIRS,STEP_CREATING_FILES,STEP_DONE/*,STEP_ABORTING*/};

// devuelve true si el descompresor puede seguir, false si debe abortar
bool callback_info(wxString message, int progress) {
	if (progress) info_win->Progress(progress);
	if (message.Len()) info_win->Notify(message);
	if (info_win->ShouldStop()) {
		int ans = wxMessageBox(spanish?"¿Desea interrumpir la instalación?":"Abort instalation?",spanish?"Instalación de Complementos":"Complement installer",wxYES_NO);
		if (ans==wxYES) return false;
		else info_win->DontStop();
	}
	return true;
}
	
	
mxInfoWindow::mxInfoWindow(wxString _dest, wxString _file) : 
	wxFrame(NULL,wxID_ANY,spanish?"Instalación de complementos":"Complement installer",wxDefaultPosition,wxDefaultSize) 
{
	should_stop = false;
	
	if (!_dest.Len()) _dest=".";
	if (_dest.Last()!='\\' && _dest.Last()!='/') {
#ifdef __WIN32__
		_dest<<"\\";
#else
		_dest<<"/";
#endif
	}
	dest=_dest; file=_file; step=STEP_READING_INFO; info_win=this; should_stop=false;
	
	SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	
	wxStaticText *stext = new wxStaticText(this,wxID_ANY,spanish?"Información del complemento:":"Complement information:");
	sizer->Add(stext,wxSizerFlags().Proportion(0).Expand().Border(wxALL,5));
	
	text = new wxTextCtrl(this,wxID_ANY,(spanish?"Leyendo ":"Reading ")+file,wxDefaultPosition,wxDefaultSize,wxTE_MULTILINE|wxTE_READONLY);
	text->SetMinSize(wxSize(400,200));
	sizer->Add(text,wxSizerFlags().Proportion(1).Expand().Border(wxALL,5));
	
	show_details = new wxCheckBox(this,wxID_ANY,spanish?"Mostrar detalles durante la instalación":"Show details during installation");
	sizer->Add(show_details,wxSizerFlags().Proportion(0).Expand().Border(wxALL,5));
	
	gauge = new wxGauge(this,wxID_ANY,1);
	sizer->Add(gauge,wxSizerFlags().Proportion(0).Expand().Border(wxALL,5));
	gauge->Hide();
	
	wxBoxSizer *but_sizer = new wxBoxSizer(wxHORIZONTAL);
	but_ok = new wxButton(this,wxID_OK,spanish?"Instalar...":"Install...");
	but_cancel = new wxButton(this,wxID_CANCEL,spanish?"Cancelar":"Cancel");
	but_sizer->Add(but_cancel,wxSizerFlags().Border(wxALL,5));
	but_sizer->AddStretchSpacer();
	but_sizer->Add(but_ok,wxSizerFlags().Border(wxALL,5));
	sizer->Add(but_sizer,wxSizerFlags().Proportion(0).Expand());
	SetSizerAndFit(sizer);

	Show(); 
	but_ok->Enable(false);
	wxYield();

	wxFileSystem::AddHandler(new wxArchiveFSHandler);
	wxString desc; int fcount,dcount;
	if (!GetFilesAndDesc(callback_info,file,fcount,dcount,desc)) { wxMessageBox(spanish?"Ha ocurrido un error durante la instalación (GetFilesAndDesc).":"There was a error installing the complement (GetFilesAndDesc)"); Close(); return; }
	if (ShouldStop()) { Close(); return; }
	desc_split(desc,info);
	text->SetValue(spanish?info.desc_spanish:info.desc_english);
	gauge->SetRange(fcount*2+dcount);
	gauge->SetValue(0);
	but_ok->Enable(true);
	but_ok->SetFocus(); 
	step=STEP_SHOWING_INFO;
	if (info.reqver>VERSION) {
		wxMessageBox(spanish 
					 ? "El complemento está diseñado para un versión más actual de ZinjaI.\n"
					   "Se requiere actualizar ZinjaI para que el complemento funcione correctamente."
					 : "This complement is designed for a newer version of ZinjaI.\n"
					   "It is required to update ZinjaI for the complement to work properly.");
	}
}

void mxInfoWindow::OnButtonOk (wxCommandEvent & evt) {
	
	if (step==STEP_SHOWING_INFO) {
		if (info.closereq && wxNO==wxMessageBox(spanish?"Debe cerrar todas las instancias abiertas de ZinjaI antes de continuar. ¿Instalar ahora?.":"You must close all ZinjaI's instances before continuing. Install now?",spanish?"Advertencia":"Warning",wxYES_NO)) 
			return;
		details = show_details->GetValue();
		show_details->Hide(); gauge->Show(); Layout();
		but_ok->Enable(false);
		if (details) text->SetValue(spanish?"Instalando...\n":"Installing...\n");
		step=STEP_CREATING_DIRS;
		if (!CreateDirectories(callback_info,file,dest)) { wxMessageBox(spanish?"Ha ocurrido un error durante la instalación (CreateDirectories).":"There was a error installing the complement (CreateDirectories)"); Finish(); return; }
		if (ShouldStop()) { Finish(); return; }
		step=STEP_CREATING_FILES;
		if (!ExtractFiles(callback_info,file,dest)) { wxMessageBox(spanish?"Ha ocurrido un error durante la instalación (ExtractFiles).":"There was a error installing the complement (ExtractFiles)"); Finish(); return; }
		if (ShouldStop()) { Finish(); return; }
		// falta setear permisos a ejecutables
#ifndef __WIN32__
		if (info.bins.GetCount()) {
			Notify(spanish?"Corrigiendo permisos...":"Setting permissions...");
			SetBins(info.bins,dest);
		}
#endif
		Progress(1);
		Notify(spanish?"Instalación Finalizada":"Instalation completed.");
		if (info.resetreq)
			wxMessageBox(spanish?"Los cambios tendrán efecto la próxima vez que inicie ZinjaI.":"Changes will apply next time you start ZinjaI.");
		else 
			wxMessageBox(spanish?"La instalación ha finalizado correctamente.":"Installation successfully completed.");
		Finish();
	} else if (step==STEP_DONE) Close();
}

void mxInfoWindow::OnButtonCancel (wxCommandEvent & evt) {
	if (step==STEP_SHOWING_INFO) Close();
	else should_stop=true;
}

void mxInfoWindow::OnClose (wxCloseEvent & evt) {
	Destroy();
}

void mxInfoWindow::Notify(const wxString &message) {
	if (details) {
		text->AppendText(message+"\n");
		int i=text->GetValue().Len()-1;
		text->SetSelection(i,i);
	}
	wxYield();
}

void mxInfoWindow::Progress (int progress) {
	gauge->SetValue(info_win->gauge->GetValue()+progress);
}

void mxInfoWindow::Finish ( ) {
	but_cancel->Enable(false);
	but_ok->Enable(true);
	but_ok->SetLabel(spanish?"Cerrar":"Close");
	step=STEP_DONE;
	if (!details) Close();
}


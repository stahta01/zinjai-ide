#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/listbox.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include "mxBySourceCompilingOpts.h"
#include "mxUtils.h"
#include "mxSizers.h"
#include "ProjectManager.h"
#include "mxHelpWindow.h"

#define _default_obj_tpl "${TEMP_DIR}/${SRC_FNAME}.o"

BEGIN_EVENT_TABLE(mxBySourceCompilingOpts,wxDialog)
	EVT_BUTTON(mxID_BYSRCOPTS_APPLY_ARGS,mxBySourceCompilingOpts::OnButtonApplyArgs)
	EVT_BUTTON(mxID_BYSRCOPTS_APPLY_OBJ,mxBySourceCompilingOpts::OnButtonApplyObj)
	EVT_BUTTON(wxID_OK,mxBySourceCompilingOpts::OnButtonOk)
	EVT_BUTTON(wxID_CANCEL,mxBySourceCompilingOpts::OnButtonCancel)
	EVT_BUTTON(mxID_HELP_BUTTON,mxBySourceCompilingOpts::OnButtonHelp)
	EVT_LISTBOX(wxID_ANY,mxBySourceCompilingOpts::OnList)
	EVT_TEXT(wxID_FIND,mxBySourceCompilingOpts::OnFilter)
	EVT_COMBOBOX(wxID_OPEN,mxBySourceCompilingOpts::OnProfile)
END_EVENT_TABLE()

mxBySourceCompilingOpts::mxBySourceCompilingOpts(wxWindow *parent, project_file_item *item) 
	: mxDialog( parent ,LANG(BYSRCOPTS_CAPTION,"Opciones de compilación por fuente") )
{
	BoolFlagGuard fg(mask_list_selection_event);
	
	last_source = (item?item:project->files.sources[0])->GetRelativePath();
	config.Resize(project->configurations_count); active_config=0;
	for(int i=0;i<project->configurations_count;i++) {
		if (project->configurations[i]==project->active_configuration) active_config=i;
		config[i] = *(project->configurations[i]->by_src_compiling_options);
	}
	
	CreateSizer main_sizer(this), right_sizer(this), left_sizer(this);
	CreateHorizontalSizer top_sizer(this);
	
	left_sizer.BeginText( LANG(BYSRCOPTS_FILTER,"Filtrar") ).Short().Id(wxID_FIND).EndText(filter_sources);
	project->GetFileList(sources_list,FT_SOURCE,true);
	for(unsigned int i=0;i<sources_list.GetCount();i++) { 
		wxString tpl = project->FindFromRelativePath(sources_list[i])->GetBinNameTpl();
		objects.Add( tpl.IsEmpty() ? _default_obj_tpl : tpl );
	}
	list = new wxListBox(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,sources_list,wxLB_MULTIPLE);
	list->Select(sources_list.Index(last_source));
	left_sizer.Add(list,sizers->BA5_Exp1);
	
	right_sizer.BeginLine()
		.BeginText( LANG(BYSRCOPTS_OBJECT_FILE,"Archivo objeto") ).Expand().Value(_default_obj_tpl).EndText(obj_file)
		.Space(10)
		.BeginButton( LANG(BYSRCOPT_APPLY_TO_SELECTED_FILES,"Aplicar cambios") )
		.Id(mxID_BYSRCOPTS_APPLY_OBJ).EndButton()
		.EndLine();
	right_sizer.BeginLabel("").EndLabel();
	right_sizer.BeginSection( LANG(BYSRCOPTS_FROM_PROFILE,"Tomar desde el perfil") ) 
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_EXTRA_ARGS,"Parametros extra para la compilación")) .EndCheck(fp_extra)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_MACROS,"Macros a definir") ).EndCheck(fp_macros)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_EXTRA_PATHS,"Directorios adicionales para buscar cabeceras") ).EndCheck(fp_includes)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_STD,"Estandar") ).EndCheck(fp_std)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_WARNINGS,"Nivel de advertencias") ).EndCheck(fp_warnings)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_DEBUG,"Informacion de depuración") ).EndCheck(fp_debug)
		.BeginCheck( LANG(PROJECTCONFIG_COMPILING_OPTIM,"Nivel de optimización") ).EndCheck(fp_optimizations)
		.EndSection();
	
	right_sizer.BeginText( LANG(BYSRCOPTS_ADDITIONAL_ARGS,"Argumentos de compilación adicionales") ).MultiLine().EndText(additional_args);

	wxArrayString profiles_list;
	for(int i=0;i<project->configurations_count;i++) profiles_list.Add(project->configurations[i]->name);
	profiles_list.Add(LANG(BYSRCOPT_ALL_PROFILES,"<<todos los perfiles>>"));
	right_sizer.BeginLine()
			.BeginCombo( LANG(BYSRCOPT_PROFILE,"Aplicar para el perfil") )
				.Add(profiles_list).Select(project->active_configuration->name).Id(wxID_OPEN).EndCombo(profiles)
			.Space(10)
			.BeginButton( LANG(BYSRCOPT_APPLY_TO_SELECTED_FILES,"Aplicar cambios") )
				.Id(mxID_BYSRCOPTS_APPLY_ARGS).EndButton()
		.EndLine();
	
	top_sizer.Add(left_sizer,1).Add(right_sizer,2);
	main_sizer.Add(top_sizer,sizers->Exp1);
	main_sizer
		.BeginBottom().Help().Ok().Cancel().EndBottom(this)
		.SetAndFit();
	
	fg.Release(); wxCommandEvent e; OnList(e);
	Show(); SetFocus();
}

void mxBySourceCompilingOpts::OnButtonCancel (wxCommandEvent & evt) {
	Close();
}

void mxBySourceCompilingOpts::OnButtonHelp (wxCommandEvent & evt) {
	mxHelpWindow::ShowHelp("by_src_comp_args.html");
}

#define _auxByScr_ALL_OPTS "${EXT} ${DEF} ${INC} ${STD} ${WAR} ${DBG} ${OPT} "

void mxBySourceCompilingOpts::OnButtonApplyObj (wxCommandEvent & evt) {
	wxString value = obj_file->GetValue();
	for(unsigned int j=0;j<list->GetCount();j++) {
		if (!list->IsSelected(j)) continue;
		int p = sources_list.Index(list->GetString(j));
		objects[p] = value;
	}
}

void mxBySourceCompilingOpts::OnButtonApplyArgs (wxCommandEvent & evt) {
	int prof = profiles->GetSelection();
	wxString value;
	if (fp_extra->GetValue()) value<<"${EXT} ";
	if (fp_macros->GetValue()) value<<"${DEF} ";
	if (fp_includes->GetValue()) value<<"${INC} ";
	if (fp_std->GetValue()) value<<"${STD} ";
	if (fp_warnings->GetValue()) value<<"${WAR} ";
	if (fp_debug->GetValue()) value<<"${DBG} ";
	if (fp_optimizations->GetValue()) value<<"${OPT} ";
	if (value==_auxByScr_ALL_OPTS) value="${ALL} ";
	value<<additional_args->GetValue(); value.Trim(false).Trim(true);
	for(unsigned int j=0;j<list->GetCount();j++) {
		if (!list->IsSelected(j)) continue;
		if (prof==config.GetSize()) {
			for(int i=0;i<config.GetSize();i++)
				config[i][list->GetString(j)] = value;
		} else {
			config[prof][list->GetString(j)] = value;
		}
	}
}

void mxBySourceCompilingOpts::OnButtonOk (wxCommandEvent & evt) {
	for(int i=0;i<project->configurations_count;i++) {
		// sacar primero del mapa las que quedaron como es por defecto
		HashStringString::iterator it = config[i].begin();
		while (it != config[i].end())
			if (it->second=="${ALL}") { config[i].erase(it); it = config[i].begin(); } else ++it;
		*(project->configurations[i]->by_src_compiling_options) = config[i];
	}
	for(unsigned int i=0;i<sources_list.GetCount();i++) {
		project->FindFromRelativePath(sources_list[i])
			->SetBinNameTemplate( objects[i]==_default_obj_tpl ? "" : objects[i] );
	}
	Close();
}

void mxBySourceCompilingOpts::OnList (wxCommandEvent & evt) {
	if (mask_list_selection_event) return;
	wxString sel;
	for(unsigned int j=0;j<list->GetCount();j++) 
		if (list->IsSelected(j)) { 
			if (sel.IsEmpty()) sel = list->GetString(j);
			else return; 
		}
	Load(profiles->GetSelection(),sel);
	int psel = sources_list.Index(sel);
	if (psel!=wxNOT_FOUND) obj_file->SetValue( objects[psel] );
}

void mxBySourceCompilingOpts::OnFilter (wxCommandEvent & evt) {
	BoolFlagGuard fg(mask_list_selection_event);
	// guardar la seleccion previa
	wxArrayString sels;
	for(unsigned int j=0;j<list->GetCount();j++) 
		if (list->IsSelected(j)) sels.Add(list->GetString(j));
	// recargar la lista
	list->Clear();
	wxString filt = filter_sources->GetValue().MakeLower();
	for(unsigned int i=0;i<sources_list.GetCount();i++) { 
		if (sources_list[i].Lower().Contains(filt))
			list->Append(sources_list[i]);
	}
	// recrear la seleccion
	for(unsigned int j=0;j<list->GetCount();j++) 
		if (sels.Index(list->GetString(j))!=wxNOT_FOUND)
			list->Select(j);
}

void mxBySourceCompilingOpts::OnProfile (wxCommandEvent & evt) {
	if (profiles->GetSelection()!=config.GetSize()) Load(profiles->GetSelection());
}

void mxBySourceCompilingOpts::Load (int prof, wxString src) {
	if (src=="") src=last_source; else last_source=src;
	if (prof==config.GetSize()) prof=active_config;
	HashStringString::iterator it = config[prof].find(src);
	wxString value = it==config[prof].end()?"${ALL}":it->second;
	value.Replace("${ALL}",_auxByScr_ALL_OPTS);
	fp_extra->SetValue(value.Replace("${EXT}",""));
	fp_macros->SetValue(value.Replace("${DEF}",""));
	fp_includes->SetValue(value.Replace("${INC}",""));
	fp_std->SetValue(value.Replace("${STD}",""));
	fp_warnings->SetValue(value.Replace("${WAR}",""));
	fp_debug->SetValue(value.Replace("${DBG}",""));
	fp_optimizations->SetValue(value.Replace("${OPT}",""));
	additional_args->SetValue(value.Trim(false).Trim(true));
}


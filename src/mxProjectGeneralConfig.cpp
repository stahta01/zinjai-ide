#include <wx/dir.h>
#include <wx/choicdlg.h>
#include "mxProjectGeneralConfig.h"
#include "mxMainWindow.h"
#include "mxBitmapButton.h"
#include "mxSizers.h"
#include "ProjectManager.h"
#include "ids.h"
#include "ConfigManager.h"
#include "mxHelpWindow.h"
#include "CodeHelper.h"
#include "mxSource.h"
#include "mxMessageDialog.h"
#include "Language.h"
#include "Autocoder.h"
#include "mxMultipleChoiceEditor.h"
#include "mxBySourceCompilingOpts.h"
#include "mxInspectionsImprovingEditor.h"

BEGIN_EVENT_TABLE(mxProjectGeneralConfig, wxDialog)
	EVT_BUTTON(wxID_OK,mxProjectGeneralConfig::OnOkButton)
	EVT_BUTTON(wxID_CANCEL,mxProjectGeneralConfig::OnCancelButton)
	EVT_BUTTON(mxID_PROJECT_CONFIG_AUTOCOMP_INDEXES,mxProjectGeneralConfig::OnIndexesButton)
	EVT_BUTTON(mxID_RUN_CONFIG,mxProjectGeneralConfig::OnCompileConfigButton)
	EVT_BUTTON(mxID_PROJECT_CONFIG_CUSTOM_TOOLS,mxProjectGeneralConfig::OnCustomToolsConfig)
	EVT_BUTTON(mxID_TOOLS_DOXY_CONFIG,mxProjectGeneralConfig::OnDoxygenConfigButton)
	EVT_BUTTON(mxID_PROJECT_CONFIG_INHERITS_FROM,mxProjectGeneralConfig::OnInheritsFrom)
	EVT_BUTTON(mxID_DEBUG_MACROS,mxProjectGeneralConfig::OnDebugMacros)
	EVT_BUTTON(mxID_TOOLS_CPPCHECK_CONFIG,mxProjectGeneralConfig::OnCppCheckConfig)
	EVT_BUTTON(mxID_TOOLS_WXFB_CONFIG,mxProjectGeneralConfig::OnWxfbConfig)
	EVT_BUTTON(mxID_PROJECT_CONFIG_BYSRC,mxProjectGeneralConfig::OnBySrcCompilingOts)
	EVT_BUTTON(mxID_PROJECT_CONFIG_AUTOIMPROVE_TEMPLATES,mxProjectGeneralConfig::OnAutoimprovingInspections)
	EVT_BUTTON(mxID_TOOLS_DRAW_PROJECT,mxProjectGeneralConfig::OnDrawGraph)
	EVT_BUTTON(mxID_TOOLS_LIZARD_RUN,mxProjectGeneralConfig::OnRunLizard)
	EVT_BUTTON(mxID_TOOLS_PROJECT_STATISTICS,mxProjectGeneralConfig::OnStatistics)
	EVT_MENU(mxID_DEBUG_MACROS_OPEN,mxProjectGeneralConfig::OnDebugMacrosOpen)
	EVT_MENU(mxID_DEBUG_MACROS_EDIT,mxProjectGeneralConfig::OnDebugMacrosEdit)
	EVT_BUTTON(mxID_AUTOCODES_FILE,mxProjectGeneralConfig::OnAutocodes)
	EVT_MENU(mxID_AUTOCODES_OPEN,mxProjectGeneralConfig::OnAutocodesOpen)
	EVT_MENU(mxID_AUTOCODES_EDIT,mxProjectGeneralConfig::OnAutocodesEdit)
	EVT_BUTTON(mxID_HELP_BUTTON,mxProjectGeneralConfig::OnHelpButton)
//	EVT_CHECKBOX(mxID_PROJECT_CONFIG_CUSTOM_TABS,mxProjectGeneralConfig::OnCustomTabs)
END_EVENT_TABLE()

mxProjectGeneralConfig::mxProjectGeneralConfig() 
	: mxDialog(main_window, LANG(PROJECTGENERAL_CAPTION,"Configuracion de Proyecto") ) 
{
	CreateSizer(this)
		.BeginNotebook()
			.AddPage(this,&mxProjectGeneralConfig::CreateTabGeneral, LANG(PROJECTCONFIG_TAB_GENERAL,"General"))
			.AddPage(this,&mxProjectGeneralConfig::CreateTabAdvanced, LANG(PROJECTCONFIG_TAB_MORE,"Más"))
			.AddPage(this,&mxProjectGeneralConfig::CreateTabInfo, LANG(PROJECTCONFIG_TAB_INFO,"Info"))
		.EndNotebook()
		.BeginBottom().Help().Ok().Cancel().EndBottom(this)
		.SetAndFit();
	Show(); SetFocus();
}

wxPanel *mxProjectGeneralConfig::CreateTabGeneral(wxNotebook *notebook) {
	CreatePanelAndSizer sizer(notebook);
	
	
	sizer.BeginText(LANG(PROJECTGENERAL_NAME,"Nombre del proyecto")).Short().Value(project->project_name).EndText(project_name);
	
	sizer.BeginLine()
//		.BeginCheck(LANG(PROJECTGENERAL_CUSTOM_TABS,"Tabulado propio"))
//			.Value(project->custom_tabs!=0).Id(mxID_PROJECT_CONFIG_CUSTOM_TABS).EndCheck(custom_tab)
//			.Space(15)
		.BeginText(LANG(PROJECTGENERAL_TAB_WIDTH,"Ancho del tabulado"))
			.Value(project->tab_width)
			.Short().EndText(tab_width)
			.Space(15)
		.BeginCheck(LANG(PROJECTGENERAL_TAB_SPACE,"Espacios en lugar de tabs"))
			.Value(project->tab_use_spaces)
			.EndCheck(tab_use_spaces)
		.EndLine();
		
	sizer.BeginLine()
		.BeginLabel(LANG(PROJECTGENERAL_PREFERRED_EXTENSIONS,"Extensiones preferidas:")).EndLabel()
		.Space(15)
		.BeginText(LANG(PROJECTGENERAL_DEFAULT_EXTENSIONS_SOURCE,"Fuentes"))
			.Value(project->default_fext_source).Short().EndText(default_fext_source)
		.Space(15)
		.BeginText(LANG(PROJECTGENERAL_DEFAULT_EXTENSIONS_HEADERS,"Cabeceras"))
			.Value(project->default_fext_header).Short().EndText(default_fext_header)
		.EndLine();
	
	sizer.BeginText(LANG(PROJECTGENERAL_INHERITS_FROM,"Heredar archivos de"))
		.Value(project->inherits_from).Button(mxID_PROJECT_CONFIG_INHERITS_FROM).EndText(inherits_from);
	
	sizer.BeginText(LANG(PROJECTGENERAL_AUTOCOMP_EXTRA,"Indices de autocompletado adicionales"))
		.Value(project->autocomp_extra).Button(mxID_PROJECT_CONFIG_AUTOCOMP_INDEXES).EndText(project_autocomp);
	sizer.BeginText(LANG(PREFERENCES_WRITTING_AUTOCODES_FILE,"Archivo de definiciones de autocódigos"))
		.Value(project->autocodes_file).Button(mxID_AUTOCODES_FILE).EndText(project_autocodes);
	sizer.BeginText(LANG(PREFERENCES_DEBUG_GDB_MACROS_FILE,"Archivo de macros para gdb"))
		.Value(project->macros_file).Button(mxID_DEBUG_MACROS).EndText(project_debug_macros);
//	tab_width->Enable(custom_tab->GetValue());
//	tab_use_spaces->Enable(custom_tab->GetValue());
	sizer.BeginButton(LANG(PROJECTGENERAL_AUTOIMPROVE_TEMPLATES," Mejora de inspecciones según tipo ")).Id(mxID_PROJECT_CONFIG_AUTOIMPROVE_TEMPLATES).Expand().EndButton();

	sizer.SetAndFit();
	return sizer.GetPanel();
}

wxPanel *mxProjectGeneralConfig::CreateTabAdvanced(wxNotebook *notebook) {
	CreatePanelAndSizer sizer(notebook);
		
	sizer.BeginButton(LANG(PROJECTGENERAL_COMPILE_AND_RUN," Compilación y Ejecución (generales)... ")).Id(mxID_RUN_CONFIG).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_BYSRC_OPTIONS," Opciones de Compilación (por fuente)... ")).Id(mxID_PROJECT_CONFIG_BYSRC).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_DOXYGEN," Configuración Doxygen... ")).Id(mxID_TOOLS_DOXY_CONFIG).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_CPPCHECK," Configuración CppCheck... ")).Id(mxID_TOOLS_CPPCHECK_CONFIG).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_WXFB," Integración con wxFormBuilder... ")).Id(mxID_TOOLS_WXFB_CONFIG).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_CUSTOM_TOOLS," Herramientas Personalizadas... ")).Id(mxID_PROJECT_CONFIG_CUSTOM_TOOLS).Expand().EndButton();
	
	sizer.SetAndFit();
	return sizer.GetPanel();
}

wxPanel *mxProjectGeneralConfig::CreateTabInfo(wxNotebook *notebook) {
	CreatePanelAndSizer sizer(notebook);
	
	sizer.BeginText(LANG(PROJECTGENERAL_ZPR_FILE,"Archivo de proyecto:")).Value(project->filename).ReadOnly().EndText();
	sizer.BeginButton(LANG(PROJECTGENERAL_STATISTICS," Estadísticas... ")).Id(mxID_TOOLS_PROJECT_STATISTICS).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_GRAPH," Grafo de archivos... ")).Id(mxID_TOOLS_DRAW_PROJECT).Expand().EndButton();
	sizer.BeginButton(LANG(PROJECTGENERAL_LIZARD," Análisis de complejidad... ")).Id(mxID_TOOLS_LIZARD_RUN).Expand().EndButton();

	sizer.SetAndFit();
	return sizer.GetPanel();
}

void mxProjectGeneralConfig::OnOkButton(wxCommandEvent &evt) {
	if (!project) {Close(); return;}
	if (project->project_name!=project_name->GetValue()) {
		project->project_name=project_name->GetValue();
		main_window->SetOpenedFileName(project->project_name);
	}
	if (project->inherits_from!=inherits_from->GetValue()) {
		project->inherits_from=inherits_from->GetValue();
		project->ReloadFatherProjects();
	}
	
	int old_tab_width = project->tab_width; bool old_use_spaces = project->tab_use_spaces;
	long l;	if (tab_width->GetValue().ToLong(&l) && l>=0) project->tab_width=l;
	project->tab_use_spaces = tab_use_spaces->GetValue();
	if (old_tab_width!=project->tab_width || old_use_spaces!=project->tab_use_spaces) {
		wxAuiNotebook *ns=main_window->notebook_sources;
		for (unsigned int i=0;i<ns->GetPageCount();i++)
			((mxSource*)(ns->GetPage(i)))->LoadSourceConfig();
	}
	
//	project->use_wxfb = use_wxfb->GetValue();
	project->macros_file = project_debug_macros->GetValue();
	if (project->autocomp_extra != project_autocomp->GetValue()) {
		project->autocomp_extra = project_autocomp->GetValue();
		g_code_helper->ReloadIndexes(config->Help.autocomp_indexes+" "+project->autocomp_extra);
	}
//	main_window->menu.tools_wxfb_activate->Check(project->use_wxfb);
	if (project->autocodes_file != project_autocodes->GetValue()) {
		project->autocodes_file = project_autocodes->GetValue();
		Autocoder::GetInstance()->Reset(DIR_PLUS_FILE(project->path,project->autocodes_file));
	}
	project->default_fext_header = default_fext_header->GetValue();
	project->default_fext_source = default_fext_source->GetValue();
	Close();
}

void mxProjectGeneralConfig::OnCancelButton(wxCommandEvent &evt) {
	Close();
}

void mxProjectGeneralConfig::OnCompileConfigButton(wxCommandEvent &evt) {
	main_window->OnRunCompileConfig(evt);
}

void mxProjectGeneralConfig::OnDoxygenConfigButton(wxCommandEvent &evt) {
	main_window->OnToolsDoxyConfig(evt);
}

void mxProjectGeneralConfig::OnHelpButton(wxCommandEvent &event) {
	mxHelpWindow::ShowHelp("project_general_config.html");
}

void mxProjectGeneralConfig::OnIndexesButton(wxCommandEvent &evt) {
	wxArrayString autocomp_array_all;
	mxUT::GetFilesFromBothDirs(autocomp_array_all,"autocomp");
	mxMultipleChoiceEditor(this,LANG(PROJECTGENERAL_AUTOCOMPLETION,"Autocompletado"),LANG(PROJECTGENERAL_INDEXES,"Indices:"),project_autocomp,autocomp_array_all);
}

void mxProjectGeneralConfig::OnDebugMacrosEdit(wxCommandEvent &evt) {
	wxString file=project_debug_macros->GetValue();
	if (wxFileName(DIR_PLUS_FILE(project->path,file)).FileExists())
		main_window->OpenFileFromGui(DIR_PLUS_FILE(project->path,file),nullptr);
	else {
		mxMessageDialog(main_window,LANG(PROJECTGENERAL_FILE_NOT_EXISTS,"El archivo no existe"))
			.Title(LANG(PREFERENCES_DEBUG_GDB_MACROS_FILE,"Archivo de macros")).IconWarning().Run();
	}
}

void mxProjectGeneralConfig::OnDebugMacrosOpen(wxCommandEvent &evt) {
	wxFileDialog dlg(this,LANG(PREFERENCES_DEBUG_GDB_MACROS_FILE,"Archivo de macros para gdb"),"",DIR_PLUS_FILE(project->path,project_debug_macros->GetValue()));
	if (wxID_OK==dlg.ShowModal()) {
		wxFileName fn(dlg.GetPath());
		fn.MakeRelativeTo(project->path);
		project_debug_macros->SetValue(fn.GetFullPath());
	}
}

void mxProjectGeneralConfig::OnDebugMacros(wxCommandEvent &evt) {
	wxMenu menu;
	menu.Append(mxID_DEBUG_MACROS_OPEN,LANG(PREFERENCES_OPEN_FILE,"&Buscar archivo..."));
	menu.Append(mxID_DEBUG_MACROS_EDIT,LANG(PREFERENCES_EDIT_FILE,"&Editar archivo..."));
	PopupMenu(&menu);
}

void mxProjectGeneralConfig::OnAutocodesEdit(wxCommandEvent &evt) {
	wxString file=project_autocodes->GetValue();
	if (wxFileName(DIR_PLUS_FILE(project->path,file)).FileExists())
		main_window->OpenFileFromGui(DIR_PLUS_FILE(project->path,file),nullptr);
	else {
		mxMessageDialog(main_window,LANG(PROJECTGENERAL_FILE_NOT_EXISTS,"El archivo no existe"))
			.Title(LANG(PREFERENCES_WRITTING_AUTOCODES_FILE,"Archivo con definiciones de autocodigos"))
			.IconWarning().Run();
	}
}

void mxProjectGeneralConfig::OnAutocodesOpen(wxCommandEvent &evt) {
	wxFileDialog dlg(this,LANG(PREFERENCES_WRITTING_AUTOCODES_FILE,"Archivo con definiciones de autocodigos"),"",DIR_PLUS_FILE(project->path,project_autocodes->GetValue()));
	if (wxID_OK==dlg.ShowModal()) {
		wxFileName fn(dlg.GetPath());
		fn.MakeRelativeTo(project->path);
		project_autocodes->SetValue(fn.GetFullPath());
	}
}

void mxProjectGeneralConfig::OnAutocodes(wxCommandEvent &evt) {
	wxMenu menu;
	menu.Append(mxID_AUTOCODES_OPEN,LANG(PREFERENCES_OPEN_FILE,"&Buscar archivo..."));
	menu.Append(mxID_AUTOCODES_EDIT,LANG(PREFERENCES_EDIT_FILE,"&Editar archivo..."));
	PopupMenu(&menu);
}

void mxProjectGeneralConfig::OnCustomToolsConfig (wxCommandEvent & evt) {
	new mxCustomTools(true,-1);
}

//void mxProjectGeneralConfig::OnCustomTabs (wxCommandEvent & evt) {
//	evt.Skip();
//	tab_width->Enable(custom_tab->GetValue());
//	tab_use_spaces->Enable(custom_tab->GetValue());
//}

void mxProjectGeneralConfig::OnWxfbConfig (wxCommandEvent & evt) {
	main_window->OnToolsWxfbConfig(evt);
}

void mxProjectGeneralConfig::OnCppCheckConfig (wxCommandEvent & evt) {
	main_window->OnToolsCppCheckConfig(evt);
}

void mxProjectGeneralConfig::OnBySrcCompilingOts (wxCommandEvent & evt) {
	new mxBySourceCompilingOpts(main_window,nullptr);
}

void mxProjectGeneralConfig::OnDrawGraph (wxCommandEvent & evt) {
	main_window->OnToolsDrawProject(evt);
}

void mxProjectGeneralConfig::OnRunLizard (wxCommandEvent & evt) {
	main_window->OnToolsLizardRun(evt);
}

void mxProjectGeneralConfig::OnStatistics (wxCommandEvent & evt) {
	main_window->OnToolsProjectStatistics(evt);
}

void mxProjectGeneralConfig::OnAutoimprovingInspections (wxCommandEvent & evt) {
	mxInspectionsImprovingEditor(this,
		project->inspection_improving_template_from,project->inspection_improving_template_to);
}

void mxProjectGeneralConfig::OnInheritsFrom (wxCommandEvent & evt) {
	CommonPopup(inherits_from).CommaSplit(true).AddEditAsList().AddEditAsText().AddFilename().BasePath(project->path).Caption(LANG(PROJECTGENERAL_INHERITS_FROM,"Heredar archivos de")).Run(this);
}


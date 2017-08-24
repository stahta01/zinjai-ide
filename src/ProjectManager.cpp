/** 
* @file ProjectManager.cpp
* @brief Implementación de los métodos de la clase ProjectManager
**/

#include <iostream>
#include <algorithm>
#include <wx/textfile.h>
#include <wx/msgdlg.h>
#include <wx/treectrl.h>

#include "ProjectManager.h"
#include "DebugPatcher.h"
#include "mxUtils.h"
#include "BreakPointInfo.h"

#include "mxSource.h"
#include "mxMainWindow.h"
#include "mxStatusBar.h"
#include "mxMessageDialog.h"
#include "Parser.h"
#include "ids.h"
#include "DebugManager.h"
#include "version.h"
#include "mxArgumentsDialog.h"
#include "ConfigManager.h"
#include "Language.h"
#include "mxSingleton.h"
#include "mxOSD.h"
#include "parserData.h"
#include "mxCompiler.h"
#include "Autocoder.h"
#include "Toolchain.h"
#include "CodeHelper.h"
#include "execution_workaround.h"
#include "mxWxfbInheriter.h"
#include "MenusAndToolsConfig.h"
#include "mxExternCompilerOutput.h"
#include "asserts.h"
#include "mxAUI.h"
#include "IniFile.h"
#include "EnvVars.h"
using namespace std;

#ifdef __WIN32__
	// maldito seas "winbase.h" (ahi se hacen defines como los que estan aca abajo, entonces cualquiera que los incluya esta cambiando los nombres)
	#ifdef MoveFile
		#undef MoveFile
	#endif
	#ifdef DeleteFile
		#undef DeleteFile
	#endif
#endif


#define ICON_LINE(filename) (wxString("0 ICON \"")<<filename<<"\"")
#define MANIFEST_LINE(filename) (wxString("1 RT_MANIFEST \"")<<filename<<"\"")

ProjectManager *project = nullptr;
extern char path_sep;

wxString doxygen_configuration::get_tag_index() { 
	if (project && project->doxygen && project->doxygen->use_in_quickhelp)
		return DIR_PLUS_FILE_2(project->path,project->doxygen->destdir,"index-for-zinjai.tag");
	else return "";
}

static wxString fix_path_char(wxChar file_path_char, wxString value) {
#ifdef __WIN32__
#	define real_path_char '\\'
#else
#	define real_path_char '/'
#endif
	if (real_path_char!=file_path_char)
		for (unsigned int i=0;i<value.Len();i++)
			if (value[i]==file_path_char)
				value[i]=real_path_char;
	return value;
}

/// funcion auxiliar para leer datos de un proyecto desde el cual se heredan archivos
static bool ReadProjectFilesList(const wxString &zpr_full_path, ProjectManager::FilesList &out_list, wxArrayString &recursive_inheritances) {
	IniFileReader fil(zpr_full_path);
	if (!fil.IsOk()) {
		errors_manager->AddZinjaiError(false,LANG1(PROJECT_ERROR_OPENING_FATHER,"Error abriendo el proyecto \"<{1}>\" para heredar sus archivos.",zpr_full_path));
		return false;
	}
	wxString project_path = wxFileName(zpr_full_path).GetPath();
	wxChar file_path_char = _if_win32('\\','/');
	for ( wxString section = fil.GetNextSection(); !section.IsEmpty(); section = fil.GetNextSection() ) {
		
		if (section=="general") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="inherits_from") {
					wxArrayString aux_array;
					mxUT::Split(fix_path_char(file_path_char,p.AsString()),aux_array,true,false);
					for(unsigned int i=0;i<aux_array.GetCount();i++)
						recursive_inheritances.Add(DIR_PLUS_FILE(project_path,aux_array[i]));
				} else if (p.Key()=="path_char") file_path_char = p.AsChar();
			}
		} else if (section=="source" || section=="header" || section=="other" || section=="blacklist") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="path") {
					wxString filepath = fix_path_char(file_path_char,p.AsString());
					if      (section=="source")     out_list.sources.Add( new project_file_item(project_path,filepath,wxTreeItemId(),FT_SOURCE) );
					else if (section=="header")     out_list.headers.Add( new project_file_item(project_path,filepath,wxTreeItemId(),FT_HEADER) );
					else if (section=="other")      out_list.others.Add ( new project_file_item(project_path,filepath,wxTreeItemId(),FT_OTHER ) );
					else if (section=="blacklist")  out_list.others.Add ( new project_file_item(project_path,filepath,wxTreeItemId(),FT_BLACKLIST) );
				}
			}
		}
		
	}
	return true;
}

	
void ProjectManager::ReloadFatherProjects() {
	
	// cuando se llama desde el ctor esto no cambia nada, es para cuando se llama 
	// para un proyecto ya cargado luego de modificar la lista de zprs padres
	for (GlobalListIterator<project_file_item*> it(&files.all); it.IsValid(); it.Next()) 
		if (it->IsInherited()) it->SetUnknownFatherProject();
	
	// inicilizar la lista de proyectos padres
	wxArrayString project_inheritances; 
	mxUT::Split(inherits_from,project_inheritances,true,false);
	for(unsigned int i=0;i<project_inheritances.GetCount();i++) {
		project_inheritances[i] = DIR_PLUS_FILE(path,project_inheritances[i]);
	}
	// obtener las lista de archivos heredados
	for(unsigned int i=0;i<project_inheritances.GetCount();i++) {  
		FilesList flist; 
		wxString zpr_full_path = project_inheritances[i];
		wxString zpr_relative_path = mxUT::Relativize(zpr_full_path,path);
		if (ReadProjectFilesList(zpr_full_path,flist,project_inheritances)) {
			// warning: in *this al project_file_item paths are relative, 
			// but ReadProjectFilesList puts full paths in project_file_item::m_relative_path
			for (GlobalListIterator<project_file_item*> it(&flist.all); it.IsValid(); it.Next()) {
				project_file_item *item = FindFromFullPath(it->m_relative_path);
				if (!item) item = AddFile(it->GetCategory(),it->GetRelativePath(),false);
				else if (!item->IsInherited()) continue; // si ya estaba en el proyecto como propio, dejarlo así
				item->SetFatherProject(zpr_relative_path);
				main_window->project_tree.SetInherited(item->GetTreeItem(),true);
			}
		}
	}
	// limpiar los que estaban en este zpr como heredados pero desaparecieron de los padres
	SingleList<project_file_item*> to_del;
	for (GlobalListIterator<project_file_item*> it(&files.all); it.IsValid(); it.Next()) 
		if (it->FatherProjectIsUnknown()) to_del.Add(*it);
	for(int i=0;i<to_del.GetSize();i++) 
		DeleteFile(to_del[i],false);
	// y ordenar los arboles en la gui
	main_window->project_tree.treeCtrl->SortChildren(main_window->project_tree.sources);
	main_window->project_tree.treeCtrl->SortChildren(main_window->project_tree.others);
	main_window->project_tree.treeCtrl->SortChildren(main_window->project_tree.headers);
}

// abrir un proyecto existente
ProjectManager::ProjectManager(wxFileName name):custom_tools(MAX_PROJECT_CUSTOM_TOOLS) {
	
	errors_manager->Reset(true);
	
	loading=true;
	mxOSDGuard osd(main_window,wxString(LANG(OSD_LOADING_PROJECT_PRE,"Abriendo "))<<name.GetName()<<LANG(OSD_LOADING_PROJECT_POST,"..."));
	
	g_singleton->Stop();
	
	first_compile_step=nullptr;
	post_compile_action=nullptr;
	
	project=this;
	parser->CleanAll();
	cppcheck=nullptr;
	valgrind=nullptr;
	doxygen=nullptr;
	wxfb=nullptr;
	version_required=0;
	tab_width = config->Source.tabWidth;
	tab_use_spaces = config->Source.tabUseSpaces;
	
	bool project_template=false; // should be true only when creating a new project from a template
	
	int version_saved=0; // vesion del zinjai que guardo al proyecto (suele diferir de la requerida)
	int files_to_open=0, num_files_opened=0; // para la barra de progreso
	
	
	main_window->SetStatusText(wxString(LANG(PROJMNGR_OPENING,"Abriendo"))<<" \""+name.GetFullPath()+"\"...");
	main_window->notebook_sources->Freeze();
	
	mxInspectionGrid *inspections_grid = main_window->inspection_ctrl->Reset(); bool first_inspection_grid=true;
	
	name.MakeAbsolute();
	wxString conf_name="Debug";
	wxString current_source="";
	configurations_count=0;
	
	// inicializar el arbol de proyecto
	main_window->project_tree.treeCtrl->DeleteChildren(main_window->project_tree.sources);
	main_window->project_tree.treeCtrl->DeleteChildren(main_window->project_tree.headers);
	main_window->project_tree.treeCtrl->DeleteChildren(main_window->project_tree.others);
	// setear nombre y path
	project_name=name.GetName();
	name.MakeAbsolute();
	filename=name.GetFullName();
	path=name.GetPath(wxPATH_GET_VOLUME|wxPATH_GET_SEPARATOR);
	last_dir=path;
	default_fext_source="cpp"; default_fext_header="h";
	executable_name=DIR_PLUS_FILE(path,name.GetName()+_T(BINARY_EXTENSION));
	wxChar file_path_char = _if_win32('\\','/');
	
	IniFileReader fil(DIR_PLUS_FILE(path,filename));
	if (!fil.IsOk()) return;
	
	for ( wxString section = fil.GetNextSection(); !section.IsEmpty(); section = fil.GetNextSection() ) {
			
		if (section=="general") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="files_to_open") {
					files_to_open = p.AsInt()+1; // +1 por el zpr?
					main_window->SetStatusProgress((100*num_files_opened)/files_to_open);
				}
				else if (p.Key()=="project_template") project_template = p.AsBool();
				else if (p.Key()=="active_configuration") conf_name = p.AsString();
				else if (p.Key()=="current_source") current_source = p.AsString();
				else if (p.Key()=="path_char") file_path_char = p.AsChar();
				else if (p.Key()=="project_name") project_name = p.AsString();
				else if (p.Key()=="autocodes_file") autocodes_file = p.AsString();
				else if (p.Key()=="macros_file") macros_file = p.AsString();
				else if (p.Key()=="default_fext_source") default_fext_source = p.AsString();
				else if (p.Key()=="default_fext_header") default_fext_header = p.AsString();
				else if (p.Key()=="autocomp_extra") autocomp_extra = p.AsString();
				else if (p.Key()=="version_saved") version_saved = p.AsInt();
				else if (p.Key()=="version_required") version_required = p.AsInt();
				else if (p.Key()=="tab_width" || p.Key()=="custom_tabs") tab_width = p.AsInt(); 
				else if (p.Key()=="tab_use_spaces") tab_use_spaces = p.AsBool();
				else if (p.Key()=="explorer_path") last_dir = p.AsString();
				else if (p.Key()=="tab_use_spaces") tab_use_spaces = p.AsBool();
				else if (p.Key()=="inherits_from") inherits_from = p.AsString();
				else if (p.Key()=="inspection_improving_template") {
					inspection_improving_template_from.Add(p.AsString().BeforeFirst('|'));
					inspection_improving_template_to.Add(p.AsString().AfterFirst('|'));
				}
				// para compatibilidad hacia atrás con proyectos de zinjai viejos
				else if (p.Key()=="use_wxfb") GetWxfbConfiguration()->activate_integration = p.AsBool();
				else if (p.Key()=="auto_wxfb") GetWxfbConfiguration()->autoupdate_projects = p.AsBool();
			}
			if (tab_width<=0) {
				tab_width = config->Source.tabWidth;
				tab_use_spaces = config->Source.tabUseSpaces;
			}
			
		} else if (section=="lib_to_build") { // agregar una biblioteca a generar por el projecto
			project_library *lib_to_build = new project_library;
			active_configuration->libs_to_build.Add(lib_to_build);
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="path") lib_to_build->m_path = p.AsString();
				else if (p.Key()=="libname") lib_to_build->libname = p.AsString();
				else if (p.Key()=="sources") lib_to_build->sources = p.AsString();
				else if (p.Key()=="extra_link") lib_to_build->extra_link = p.AsString();
				else if (p.Key()=="is_static") lib_to_build->is_static = p.AsBool();
				else if (p.Key()=="default_lib") lib_to_build->default_lib = p.AsBool();
				else if (p.Key()=="do_link") lib_to_build->do_link = p.AsBool();
			}
			
		} else if (section=="extra_step") { // agregar un paso de compilación adicional a la configuración actual del proyecto
			compile_extra_step *extra_step = new compile_extra_step;
			active_configuration->extra_steps.Add(extra_step);
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="name") extra_step->name = p.AsString();
				else if (p.Key()=="output") extra_step->out = p.AsString();
				else if (p.Key()=="deps") extra_step->deps = p.AsString();
				else if (p.Key()=="command") extra_step->command = p.AsString();
				else if (p.Key()=="check_retval") extra_step->check_retval = p.AsBool();
				else if (p.Key()=="hide_win") extra_step->hide_window = p.AsBool();
				else if (p.Key()=="delete_on_clean") extra_step->delete_on_clean = p.AsBool();
				else if (p.Key()=="link_output") extra_step->link_output = p.AsBool();
				else if (p.Key()=="position") {
					if (p.AsString()=="before_sources")
						extra_step->pos=CES_BEFORE_SOURCES;
					else if (p.AsString()=="before_executable" || p.AsString()=="before_linking")
						extra_step->pos=CES_BEFORE_EXECUTABLE;
					else if (p.AsString()=="before_libs")
						extra_step->pos=CES_BEFORE_LIBS;
					else if (p.AsString()=="after_linking")
						extra_step->pos=CES_AFTER_LINKING;
				}
			}
			
		} else if (section=="config") {
			active_configuration=configurations[configurations_count++] = new project_configuration(name.GetName(),"");
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key().StartsWith("toolchain_argument_")) {
					long l=-1; p.Key().Mid(19).ToLong(&l);
					if (l>=0&&l<TOOLCHAIN_MAX_ARGS) 
						active_configuration->toolchain_arguments[l]=p.AsString();
				}
				else if (p.Key()=="name") active_configuration->name = p.AsString();
				else if (p.Key()=="toolchain") active_configuration->toolchain = p.AsString();
				else if (p.Key()=="working_folder") active_configuration->working_folder = p.AsString();
				else if (p.Key()=="always_ask_args") active_configuration->always_ask_args = p.AsBool();
				else if (p.Key()=="args") active_configuration->args = p.AsString();
				else if (p.Key()=="exec_method") active_configuration->exec_method = p.AsInt();
				else if (p.Key()=="exec_script") active_configuration->exec_script = p.AsString();
				else if (p.Key()=="env_vars") active_configuration->env_vars = p.AsString();
				else if (p.Key()=="wait_for_key") active_configuration->wait_for_key = p.AsInt();
				else if (p.Key()=="temp_folder") active_configuration->temp_folder = p.AsString();
				else if (p.Key()=="output_file") active_configuration->output_file = p.AsString();
				else if (p.Key()=="manifest_file") active_configuration->manifest_file = p.AsString();
				else if (p.Key()=="icon_file") active_configuration->icon_file = p.AsString();
				else if (p.Key()=="macros") active_configuration->macros = p.AsString();
				else if (p.Key()=="compiling_extra") active_configuration->compiling_extra = p.AsString();
				else if (p.Key()=="headers_dirs") active_configuration->headers_dirs = p.AsString();
				else if (p.Key()=="warnings_level") active_configuration->warnings_level = p.AsInt();
				else if (p.Key()=="warnings_as_errors") active_configuration->warnings_as_errors = p.AsBool();
				else if (p.Key()=="pedantic_errors") active_configuration->pedantic_errors = p.AsBool();
				else if (p.Key()=="std_c") active_configuration->std_c = p.AsString();
				else if (p.Key()=="std_cpp") active_configuration->std_cpp = p.AsString();
				else if (p.Key()=="debug_level") active_configuration->debug_level = p.AsInt();
				else if (p.Key()=="optimization_level") active_configuration->optimization_level = p.AsInt();
				else if (p.Key()=="enable_lto") active_configuration->enable_lto = p.AsBool();
				else if (p.Key()=="linking_extra") active_configuration->linking_extra = p.AsString();
				else if (p.Key()=="libraries_dirs") active_configuration->libraries_dirs = p.AsString();
				else if (p.Key()=="libraries") active_configuration->libraries = p.AsString();
				else if (p.Key()=="strip_executable") active_configuration->strip_executable = p.AsInt();
				else if (p.Key()=="console_program") active_configuration->console_program = p.AsBool();
				else if (p.Key()=="dont_generate_exe") active_configuration->dont_generate_exe = p.AsBool();
				else if (p.Key()=="by_src_comp_args") {
					wxString full = mxUT::Line2Text(p.AsString());
					(*(active_configuration->by_src_compiling_options))[full.BeforeFirst('\n')]=full.AfterFirst('\n');
				}
			}
			
		} else if ( section=="source" || section=="header" || section=="other" || section=="blacklist") {
			project_file_item *last_file = nullptr;
			BreakPointInfo *last_breakpoint = nullptr;
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="path") {
					wxString filepath = fix_path_char(file_path_char,p.AsString());
					if (section=="source")
						last_file = AddFile(FT_SOURCE,filepath,false);
					else if (section=="header")
						last_file = AddFile(FT_HEADER,filepath,false);
					else if (section=="other")
						last_file = AddFile(FT_OTHER,filepath,false);
					else if (section=="blacklist")
						last_file = AddFile(FT_BLACKLIST,filepath,false);
				} else if (p.Key()=="cursor") {
					if (last_file) last_file->GetSourceExtras().SetCurrentPos(p.AsString());
				} else if (p.Key()=="readonly") {
					if (last_file) last_file->SetReadOnly(p.AsBool());
				} else if (p.Key()=="inherited") {
					if (last_file && p.AsBool()) last_file->SetUnknownFatherProject();
				} else if (p.Key()=="hide_symbols") {
					if (last_file) last_file->SetSymbolsVisible(!p.AsBool());
				} else if (p.Key()=="marker") {
					if (last_file) last_file->GetSourceExtras().AddHighlightedLine(p.AsInt());
				} else if (p.Key()=="breakpoint") {
					if (last_file) last_breakpoint = new BreakPointInfo(last_file,p.AsInt());
				} else if (p.Key()=="breakpoint_ignore") {
					if (last_breakpoint) last_breakpoint->ignore_count = p.AsInt();
				} else if (p.Key()=="breakpoint_only_once"||p.Key()=="breakpoint_action") {
					if (last_breakpoint) last_breakpoint->action = p.AsInt();
				} else if (p.Key()=="enabled") {
					if (last_breakpoint) last_breakpoint->enabled = p.AsBool();
				} else if (p.Key()=="breakpoint_condition") {
					if (last_breakpoint) last_breakpoint->cond = p.AsString();
				} else if (p.Key()=="breakpoint_annotation") {
					if (last_breakpoint) last_breakpoint->annotation = p.AsMultiLineText();
				} else if (p.Key()=="open" && p.AsBool()) {
					if (files_to_open>0)
						main_window->SetStatusProgress((100*(++num_files_opened))/files_to_open);
					if (last_file) {
						/*mxSource *src = */main_window->OpenFile(last_file->GetFullPath(),false);
//						if (src && src!=EXTERNAL_SOURCE) src->MoveCursorTo(last_file->extras.GetCurrentPos(),true);
					}
				}
			}
			
		} else if (section=="wxfb") {
			GetWxfbConfiguration(true);
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="autoupdate_projects") wxfb->autoupdate_projects = p.AsBool();
				else if (p.Key()=="update_class_list") wxfb->update_class_list = p.AsBool();
				else if (p.Key()=="update_methods") wxfb->update_methods = p.AsBool();
				else if (p.Key()=="dont_show_base_classes_in_goto") wxfb->dont_show_base_classes_in_goto = p.AsBool();
				else if (p.Key()=="set_wxfb_sources_as_readonly") wxfb->set_wxfb_sources_as_readonly = p.AsBool();
			}
			
		} else if (section=="valgrind") {
			GetValgrindConfiguration();
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="arguments") valgrind->arguments = p.AsString();
				else if (p.Key()=="suppressions") valgrind->suppressions = p.AsString();
			}
			
		} else if (section=="doxygen") {
			GetDoxygenConfiguration();
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="name") doxygen->name = p.AsString();
				else if (p.Key()=="version") doxygen->version = p.AsString();
				else if (p.Key()=="destdir") doxygen->destdir = p.AsString();
				else if (p.Key()=="extra_files") doxygen->extra_files = p.AsString();
				else if (p.Key()=="extra_conf") doxygen->extra_conf = p.AsMultiLineText();
				else if (p.Key()=="base_path") doxygen->base_path = p.AsString();
				else if (p.Key()=="exclude_files") doxygen->exclude_files = p.AsString();
				else if (p.Key()=="lang") doxygen->lang = p.AsString();
				else if (p.Key()=="do_headers") doxygen->do_headers = p.AsBool();
				else if (p.Key()=="do_cpps") doxygen->do_cpps = p.AsBool();
				else if (p.Key()=="hideundocs") doxygen->hideundocs = p.AsBool();
				else if (p.Key()=="latex") doxygen->latex = p.AsBool();
				else if (p.Key()=="html") doxygen->html = p.AsBool();
				else if (p.Key()=="html_searchengine") doxygen->html_searchengine = p.AsBool();
				else if (p.Key()=="html_navtree") doxygen->html_navtree = p.AsBool();
				else if (p.Key()=="use_in_quickhelp") doxygen->use_in_quickhelp = p.AsBool();
				else if (p.Key()=="preprocess") doxygen->preprocess = p.AsBool();
				else if (p.Key()=="extra_static") doxygen->extra_static = p.AsBool();
				else if (p.Key()=="extra_private") doxygen->extra_private = p.AsBool();
			}
			
		} else if (section=="cppcheck") {
			GetCppCheckConfiguration(true);
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="copy_from_config") cppcheck->copy_from_config = p.AsBool();
				else if (p.Key()=="exclude_headers") cppcheck->exclude_headers = p.AsBool();
				else if (p.Key()=="config_d") cppcheck->config_d = p.AsString();
				else if (p.Key()=="config_u") cppcheck->config_u = p.AsString();
				else if (p.Key()=="style") cppcheck->style = p.AsString();
				else if (p.Key()=="platform") cppcheck->platform = p.AsString();
				else if (p.Key()=="standard") cppcheck->standard = p.AsString();
				else if (p.Key()=="suppress_file") cppcheck->suppress_file = p.AsString();
				else if (p.Key()=="suppress_ids") cppcheck->suppress_ids = p.AsString();
				else if (p.Key()=="additional_files") cppcheck->additional_files = p.AsString();
				else if (p.Key()=="exclude_list") cppcheck->exclude_list = p.AsString();
				else if (p.Key()=="inline_suppr") cppcheck->inline_suppr = p.AsBool();
			}
			
		} else if (section=="inspections") {
			if (first_inspection_grid) first_inspection_grid=false;
			else inspections_grid = main_window->inspection_ctrl->AddGrid(false);
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="expr") inspections_grid->ModifyExpression(-1,p.AsString(),true,true);
			}
			
		} else if (section=="custom_tools") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				custom_tools.ParseConfigLine(p.Key(),p.AsString());
			}
			
		}
	}
		
	// completar los archivos heredados
	inherits_from = fix_path_char(file_path_char,inherits_from);
	ReloadFatherProjects();
	
	modified=false;
	force_relink=false;

	
	last_dir = DIR_PLUS_FILE(path,fix_path_char(file_path_char,last_dir));
	if (wxFileName::DirExists(last_dir)) {
		wxFileName explorer_fname(last_dir);
		explorer_fname.Normalize(wxPATH_NORM_DOTS);
		main_window->SetExplorerPath(explorer_fname.GetFullPath());
	}
	
	// agregar los indices de autocompletado nuevos
	g_code_helper->AppendIndexes(autocomp_extra);
	
	// cargar las inspecciones en la tabla
	g_code_helper->AppendIndexes(autocomp_extra);
	
	if (configurations_count==0) { // si no tenia definida ninguna configuracion crear las dos predeterminadas
		// crear configuracion Debug
		configurations[0] = new project_configuration(name.GetName(),"Debug");
		configurations[0]->debug_level=2;
		configurations[0]->optimization_level=0;
		configurations[0]->strip_executable=DBSACTION_KEEP;
		configurations[0]->macros=configurations[0]->old_macros="_DEBUG";
		// crear configuracion Release
		configurations[1] = new project_configuration(name.GetName(),"Release");
		configurations[1]->debug_level=0;
		configurations[1]->optimization_level=2;
		configurations[1]->strip_executable=DBSACTION_STRIP;
		// Debug es la predeterminada
		active_configuration = configurations[0];
		configurations_count = 2;
	
	} else { // seleccionar la que corresponde si existe
		active_configuration=GetConfig(conf_name);
		if (active_configuration==nullptr) // si no tenia seleccionada ninguna, seleccionar la primera
			active_configuration = configurations[0];
		
		for (int i=0;i<configurations_count;i++) {
			configurations[i]->old_macros=configurations[i]->macros;
//			fix_path_char(configurations[i]->output_file);
//			fix_path_char(configurations[i]->temp_folder);
//			fix_path_char(configurations[i]->working_folder);
		}
#ifdef __WIN32__
		wxString cur1="win",cur2="w32";
#else
#ifdef __APPLE__
		wxString cur1="mac",cur2="apple";
#else
		wxString cur1="lnx",cur2="linux";
#endif
#endif
		if ( (
				active_configuration->name.Lower().Contains("lnx")||
				active_configuration->name.Lower().Contains("linux")||
				active_configuration->name.Lower().Contains("win")||
				active_configuration->name.Lower().Contains("w32")||
				active_configuration->name.Lower().Contains("mac")||
				active_configuration->name.Lower().Contains("apple")
			) && !(
				active_configuration->name.Lower().Contains(cur1)||
				active_configuration->name.Lower().Contains(cur2)
			) ) {
				
				project_configuration *suggested_configuration=nullptr;
				for (int i=0;i<configurations_count;i++) {
					if (configurations[i]->name.Lower().Contains(cur1)||configurations[i]->name.Lower().Contains(cur2)) {
						if (!suggested_configuration || 
							(!(suggested_configuration->name.Lower().Contains("debug")||suggested_configuration->name.Lower().Contains("dbg"))
							&&(configurations[i]->name.Lower().Contains("debug")||configurations[i]->name.Lower().Contains("dbg"))) )
								suggested_configuration=configurations[i];
					}
				}
				if (suggested_configuration) {
					bool do_change = project_template, do_select = false;
					if (!project_template) { 
						mxMessageDialog::mdAns ans = 
							mxMessageDialog(main_window,LANG2(PROJMNGR_CHANGE_PROFILE_OPENNING,""
															  "Parece que esta abriendo un proyecto que tiene seleccionado un perfil\n"
															  "de compilación y ejecución para otro sistema operativo: \"<{1}>\"\n"
															  "\n¿Desea cambiar el perfil activo por \"<{2}>\"?",
															  active_configuration->name,suggested_configuration->name))
								.Title(LANG(PROJMNGR_CHANGE_PROFILE_OPENNING_CAPTION,"Perfil de Compilación y Ejecución"))
								.Check1(LANG(PROJMNGR_CHANGE_PROFILE_OPENNING_CHECK,"Seleccionar otro perfil de la lista"),false)
								.ButtonsYesNo().IconWarning().Run();
						do_change = ans.yes;
						do_select = ans.check1;
					}
					if (do_change) {
						if (do_select) {
							wxArrayString choices;
							for (int i=0;i<configurations_count;i++)
								choices.Add(configurations[i]->name);
							wxString res=wxGetSingleChoice(
								LANG(PROJMNGR_CHANGE_PROFILE_OPENNING_CHOICE,"Seleccione el perfil a activar"),
								LANG(PROJMNGR_CHANGE_PROFILE_OPENNING_CAPTION,"Perfil de Compilación y Ejecución"),
								choices,main_window);
							if (res.Len())
								for (int i=0;i<configurations_count;i++)
									if (configurations[i]->name==res)
										active_configuration=configurations[i];
						} else {
							active_configuration=suggested_configuration;
						}
					}
				}
			}
	}
 
	SetActiveConfiguration(active_configuration);
	
	// configurar interface de la ventana principal para modo proyecto
	if (current_source!="") {
		main_window->OpenFile(DIR_PLUS_FILE(path,fix_path_char(file_path_char,current_source)),false);
	}
	
	main_window->notebook_sources->Thaw();
	main_window->notebook_sources->Fit();
	
	if (autocodes_file.Len()) Autocoder::GetInstance()->LoadFromFile(DIR_PLUS_FILE(path,autocodes_file));
	
	if (version_saved<20140410) { // arreglar cambios de significado strip_executable (paso de bool a int)
		for (int i=0;i<configurations_count;i++) {
			configurations[i]->strip_executable = configurations[i]->strip_executable!=0?DBSACTION_STRIP:DBSACTION_KEEP;
		}
	}
	
	if (version_saved<20130801) { // arreglar cambios de significado ("<default>" en el estandar c/cpp)
		for (int i=0;i<configurations_count;i++) {
			if (configurations[i]->std_c.StartsWith("<")) configurations[i]->std_c="";
			if (configurations[i]->std_cpp.StartsWith("<")) configurations[i]->std_cpp="";
		}
	}
	
	if (version_saved<20100518) { // arreglar cambios de significado
		for (int i=0;i<configurations_count;i++)
			if (configurations[i]->wait_for_key)
				configurations[i]->wait_for_key=2;
	}
	
	if (wxfb && version_saved<20140125) {
		WxfbSetFileProperties(true,wxfb->set_wxfb_sources_as_readonly,true,wxfb->dont_show_base_classes_in_goto);
	}
	
	if (wxfb && version_saved<20140125) {
		WxfbSetFileProperties(true,wxfb->set_wxfb_sources_as_readonly,true,wxfb->dont_show_base_classes_in_goto);
	}
	
	if (wxfb && version_saved<20141218) { // arreglar cambios de significado, se agrego EMETHOD_WRAPPER
		for (int i=0;i<configurations_count;i++)
			if (configurations[i]->exec_method>0)
				configurations[i]->exec_method++;
		for(int i=0;i<custom_tools.GetCount();i++) { 
			if (custom_tools[i].output_mode>=2)
				custom_tools[i].output_mode++;
		}
	}
	
	if (version_required>VERSION) {
		mxMessageDialog(main_window,LANG(PROJECT_REQUIRES_NEWER_ZINJAI,""
									"El proyecto que esta abriendo requiere una version superior de ZinjaI\n"
									"a la instalada actualmente para cargar todas las opciones de proyecto\n"
									"correctamente. El proyecto se abrira de todas formas, pero podria encontrar\n"
									"problemas posteriormente.\n"))
			.Title(LANG(GENERAL_WARNING,"Advertencia")).IconWarning().Run();
	}
	
	
	if (files_to_open>0) main_window->SetStatusProgress(-1);
	
	loading=false;

	main_window->PrepareGuiForProject(true);
	if (GetWxfbActivated()) {
		project->ActivateWxfb(true); // para que marque en el menu y verifique si esta instalado
	}
	
	g_navigation_history.Reset();
	
	main_window->SetStatusText(wxString(LANG(GENERAL_READY,"Listo")));
	
#ifdef __WIN32__
	if (!project_template && version_saved<20161116) { // changes mingw default calling convention
		mxMessageDialog::mdAns ret =
			mxMessageDialog(main_window,LANG(PROJECT_MINGW_CC_PROBLEM_DESC,""
				"El proyecto que está abriendo fue guardado con una versión de ZinjaI que utilizaba\n"
				"versiones ahora desactualizadas de las herramientas de compilación (mingw). Los\n"
				"archivos compilados con esas herramientas no serán compatibles con los que se\n"
				"compilen con la nueva versión disponible. Si utiliza el compilador por defecto\n"
				"deberá recompilar todo el proyecto. ¿Desea eliminar ahora los objetos compilados\n"
				"para forzar la recompilación del proyecto antes de la próxima ejecución?\n\n"
				"Nota: si su proyecto utiliza bibliotecas no provistas con ZinjaI, probablemente deba\n"
				"obtener además nuevas versiones de las mismas. En caso de no hacerlo, su programa\n"
				"podría finalizar anormalmente al intentar ejecutarlo."
				))
			.Title(LANG(GENERAL_WARNING,"Advertencia")).IconWarning().ButtonsYesNo()
			.Check1(LANG(PROJECT_MINGW_CC_PROBLEM_MORE,"Mostrar información (web)..."),false).Run();
		if (ret.check1) mxUT::OpenInBrowser("http://cucarachasracing.blogspot.com.ar/2013/07/mingw-y-las-calling-conventions.html");
		if (ret.yes) { wxCommandEvent evt; main_window->OnRunClean(evt); }
	}
#endif
	
	errors_manager->CompilationFinished();
	
}

// liberar memoria al destruir el proyecto
ProjectManager::~ProjectManager() {

	parser->CleanAll();
	Autocoder::GetInstance()->Reset("");
	
	// configurar interface para modo proyecto
	main_window->PrepareGuiForProject(false);

	// vaciar las listas
	for (GlobalListIterator<project_file_item*> it(&files.all); it.IsValid(); it.Next()) {
		// si el fuente queda abierto al cerrar el proyecto, pasarle la propiedad del SourceExtras
		mxSource *source = main_window->IsOpen(it->GetTreeItem());
		if (source) new SourceExtras(source,true);
	}
	
	// limipiar las configuraciones
	for (int i=0;i<configurations_count;i++)
		delete configurations[i];
	
	// restaurar los indices de autocompletado
	g_code_helper->ReloadIndexes(config->Help.autocomp_indexes);
	
	if (valgrind) delete valgrind;
	if (doxygen) delete doxygen;
	if (wxfb) delete wxfb;
	
	g_singleton->Start();
	
	project=nullptr;
	
	Toolchain::SelectToolchain(); // keep after project=nullptr
	
	g_navigation_history.Reset();
}

// devuelve verdadero si lo inserta, falso si ya estaba
project_file_item *ProjectManager::AddFile (eFileType where, wxFileName filename, bool sort_tree) {
	
	// convert filename to a relative path (relative to projects folder)
	if (filename.IsAbsolute()) filename.MakeRelativeTo(path);
	wxString relative_path = filename.GetFullPath();
	
	// check if the file already exists somewhere in the project, if not create it (also updates main_window->project_tree)
	project_file_item *fitem=nullptr;
	GlobalListIterator<project_file_item*> git(&files.all);
	while (git.IsValid()) { 
		if (git->GetRelativePath()==relative_path) {
			if (git->GetCategory()==where) return *git;
			fitem=*git; files.all.Remove(git);
			main_window->project_tree.treeCtrl->Delete(fitem->GetTreeItem());
			break;
		}
		git.Next();
	}
	if (!fitem) fitem = new project_file_item(path,relative_path,main_window->project_tree.AddFile(relative_path,where,sort_tree),where);
	
	// add it to the correct file list
	switch (where) {
		case FT_SOURCE:
			// add the file to default library if there is one
			for (int i=0;i<configurations_count;i++) {
				project_library *lib = project->GetDefaultLib(configurations[i]);
				if (lib) lib->sources<<" "<<mxUT::Quotize(relative_path);
			}
			files.sources.Add(fitem);
			break;
		case FT_HEADER:
			files.headers.Add(fitem);
			break;
		case FT_BLACKLIST:
			files.blacklist.Add(fitem);
			break;
		default:
			files.others.Add(fitem);
			if (wxfb && fitem->GetRelativePath().Right(4).Lower()==".fbp") WxfbGetFiles();
			break;
	};
	modified=true;
	return fitem;
}

bool ProjectManager::Save (bool as_template) {
	
	if (!as_template && version_required>VERSION) {
		mxMessageDialog::mdAns res 
			= mxMessageDialog(main_window,"El proyecto que esta por guardar fue creado con una version de ZinjaI superior\n"
										  "a la que esta utilizando. Si graba el proyecto ahora se convertira a su su\n"
										  "version. Algunas opciones de configuración del mismo podrian perderse en la\n"
										  "conversion. Desea guardarlo de todas formas?")
				.Title("Version del Proyecto").IconWarning().ButtonsYesNo().Run();
		if (res.no) return false;
//		if (res&mxMD_CHECKED) {		
//			wxFileDialog dlg (this, "Guardar Proyecto",source->sin_titulo?wxString(wxFileName::GetHomeDir()):wxFileName(source->source_filename).GetPath(),source->sin_titulo?wxString(wxEmptyString):wxFileName(source->source_filename).GetFullName(), "Any file (*)|*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
//			dlg.SetDirectory(source->sin_titulo?wxString(project?project->last_dir:config->Files.last_dir):wxFileName(source->source_filename).GetPath());
//			dlg.SetWildcard("Todos los archivos|*|Proyectos ZinjaI|"WILDCARD_PROJECT);
//			if (dlg.ShowModal() != wxID_OK) return;
//			wxFileName fn(dlg.GetPath());
//			path=fn.
//		}
	}
	
	version_required = GetRequiredVersion();
	
	
	// abrir el archivo (crear o pisar)
	wxTextFile fil(DIR_PLUS_FILE(path,filename));
	if (fil.Exists())
		fil.Open();
	else
		fil.Create();
	fil.Clear();
	
	mxSource *source;
	
#ifdef __WIN32__
	fil.AddLine(wxString("# generated by ZinjaI-w32-")<<VERSION);
#else
	fil.AddLine(wxString("# generated by ZinjaI-lnx-")<<VERSION);
#endif
	
	// guardar cosas varias
	fil.AddLine("[general]");
	if (as_template) CFG_BOOL_WRITE_DN("project_template",true);
	if (main_window->notebook_sources->GetPageCount()>2)
		CFG_GENERIC_WRITE_DN("files_to_open",main_window->notebook_sources->GetPageCount());
	CFG_GENERIC_WRITE_DN("project_name",project_name);
	CFG_GENERIC_WRITE_DN("autocodes_file",autocodes_file);
	CFG_GENERIC_WRITE_DN("macros_file",macros_file);
	CFG_GENERIC_WRITE_DN("default_fext_source",default_fext_source);
	CFG_GENERIC_WRITE_DN("default_fext_header",default_fext_header);
	CFG_GENERIC_WRITE_DN("autocomp_extra",autocomp_extra);
	CFG_GENERIC_WRITE_DN("active_configuration",active_configuration->name);
	CFG_GENERIC_WRITE_DN("version_saved",VERSION);
	CFG_GENERIC_WRITE_DN("version_required",version_required);
	CFG_GENERIC_WRITE_DN("tab_width",tab_width);
	CFG_BOOL_WRITE_DN("tab_use_spaces",tab_use_spaces);
	CFG_GENERIC_WRITE_DN("explorer_path",mxUT::Relativize(main_window->explorer_tree.path,path));
	for(unsigned int i=0;i<inspection_improving_template_from.GetCount();i++)
		CFG_GENERIC_WRITE_DN("inspection_improving_template",inspection_improving_template_from[i]+"|"+inspection_improving_template_to[i]);
	
	CFG_GENERIC_WRITE_DN("inherits_from",inherits_from);
	
	if (main_window->notebook_sources->GetPageCount()>0) {
		mxSource *source=(mxSource*)(main_window->notebook_sources->GetPage(main_window->notebook_sources->GetSelection()));
		if (!source->sin_titulo) {
			wxFileName fn(source->source_filename.GetFullPath());
			fn.MakeRelativeTo(path);
			CFG_GENERIC_WRITE_DN("current_source",fn.GetFullPath());
		}
	}
#ifdef __WIN32__
	fil.AddLine("path_char=\\");
#else
	fil.AddLine("path_char=/");
#endif
	// agregar la lista de fuentes pertenecientes al proyecto
	wxString file_sections[]={"[source]","[header]","[other]","[blacklist]"};
	LocalList<project_file_item*> *file_lists[] = { &files.sources, &files.headers, &files.others, &files.blacklist };
	for(int i=0;i<4;i++) { 
		LocalListIterator<project_file_item*> item(file_lists[i]);
		while (item.IsValid()) {
			fil.AddLine(file_sections[i]);
			CFG_GENERIC_WRITE_DN("path",item->m_relative_path);
			if (item->IsReadOnly()) CFG_GENERIC_WRITE_DN("readonly",1);
			if (item->IsInherited()) CFG_GENERIC_WRITE_DN("inherited",1);
			if (!item->AreSymbolsVisible()) CFG_GENERIC_WRITE_DN("hide_symbols",1);
			source=main_window->IsOpen(item->GetFullPath());
			if (source) source->UpdateExtras();
			CFG_GENERIC_WRITE_DN("cursor",item->GetSourceExtras().GetCurrentPos());
			const LocalList<BreakPointInfo*> &breakpoints=item->GetSourceExtras().GetBreakpoints();
			for(int j=0;j<breakpoints.GetSize();j++) {
				if (breakpoints[j]->line_number>=0) { 
					CFG_GENERIC_WRITE_DN("breakpoint",breakpoints[j]->line_number);
					if (!breakpoints[j]->enabled) CFG_BOOL_WRITE_DN("breakpoint_enabled",false);
					if (breakpoints[j]->action) CFG_GENERIC_WRITE_DN("breakpoint_action",true);
					if (breakpoints[j]->ignore_count) CFG_GENERIC_WRITE_DN("breakpoint_ignore",breakpoints[j]->ignore_count);
					if (breakpoints[j]->annotation) CFG_GENERIC_WRITE_DN("breakpoint_annotation",mxUT::Text2Line(breakpoints[j]->annotation));
					if (breakpoints[j]->cond.Len()) CFG_GENERIC_WRITE_DN("breakpoint_condition",breakpoints[j]->cond);
	//				breakpoint->enabled=true; // que hace esto aca?
				}
			}
			const SingleList<int> &highlighted_lines=item->GetSourceExtras().GetHighlightedLines();
			for(int j=0;j<highlighted_lines.GetSize();j++)
				CFG_GENERIC_WRITE_DN("marker",highlighted_lines[j]);
			if (source)
				fil.AddLine("open=true");
			item.Next();
		}
	}
	
	for (int i=0;i<configurations_count;i++) {
		fil.AddLine("[config]");
		CFG_GENERIC_WRITE_DN("name",configurations[i]->name);
		CFG_GENERIC_WRITE_DN("toolchain",configurations[i]->toolchain);
		for(int j=0;j<TOOLCHAIN_MAX_ARGS;j++)
			fil.AddLine(wxString("toolchain_argument_")<<j<<"="<<configurations[i]->toolchain_arguments[j]);
		CFG_GENERIC_WRITE_DN("working_folder",configurations[i]->working_folder);
		CFG_BOOL_WRITE_DN("always_ask_args",configurations[i]->always_ask_args);
		CFG_GENERIC_WRITE_DN("args",configurations[i]->args);
		CFG_GENERIC_WRITE_DN("exec_method",configurations[i]->exec_method);
		CFG_GENERIC_WRITE_DN("exec_script",configurations[i]->exec_script);
		CFG_GENERIC_WRITE_DN("env_vars",configurations[i]->env_vars);
		CFG_GENERIC_WRITE_DN("wait_for_key",configurations[i]->wait_for_key);
		CFG_GENERIC_WRITE_DN("temp_folder",configurations[i]->temp_folder);
		CFG_GENERIC_WRITE_DN("output_file",configurations[i]->output_file);
		CFG_GENERIC_WRITE_DN("icon_file",configurations[i]->icon_file);
		CFG_GENERIC_WRITE_DN("manifest_file",configurations[i]->manifest_file);
		CFG_GENERIC_WRITE_DN("compiling_extra",configurations[i]->compiling_extra);
		CFG_GENERIC_WRITE_DN("macros",configurations[i]->macros);
		CFG_GENERIC_WRITE_DN("warnings_level",configurations[i]->warnings_level);
		CFG_BOOL_WRITE_DN("warnings_as_errors",configurations[i]->warnings_as_errors);
		CFG_BOOL_WRITE_DN("pedantic_errors",configurations[i]->pedantic_errors);
		CFG_GENERIC_WRITE_DN("std_c",configurations[i]->std_c);
		CFG_GENERIC_WRITE_DN("std_cpp",configurations[i]->std_cpp);
		CFG_GENERIC_WRITE_DN("debug_level",configurations[i]->debug_level);
		CFG_GENERIC_WRITE_DN("optimization_level",configurations[i]->optimization_level);
		CFG_BOOL_WRITE_DN("enable_lto",configurations[i]->enable_lto);
		CFG_GENERIC_WRITE_DN("headers_dirs",configurations[i]->headers_dirs);
		CFG_GENERIC_WRITE_DN("linking_extra",configurations[i]->linking_extra);
		CFG_GENERIC_WRITE_DN("libraries_dirs",configurations[i]->libraries_dirs);
		CFG_GENERIC_WRITE_DN("libraries",configurations[i]->libraries);
		CFG_GENERIC_WRITE_DN("strip_executable",configurations[i]->strip_executable);
		CFG_BOOL_WRITE_DN("console_program",configurations[i]->console_program);
		CFG_BOOL_WRITE_DN("dont_generate_exe",configurations[i]->dont_generate_exe);
		for(HashStringString::iterator it = configurations[i]->by_src_compiling_options->begin();
			it!=configurations[i]->by_src_compiling_options->end(); ++it) 
		{
			CFG_GENERIC_WRITE_DN("by_src_comp_args",mxUT::Text2Line(it->first+"\n"+it->second));
		}
		for(JavaVectorIterator<project_library> lib(configurations[i]->libs_to_build); lib.IsValid(); lib.Next()) { 
			fil.AddLine("[lib_to_build]");
			CFG_GENERIC_WRITE_DN("libname",lib->libname);
			CFG_GENERIC_WRITE_DN("path",lib->m_path);
			CFG_GENERIC_WRITE_DN("sources",lib->sources);
			CFG_GENERIC_WRITE_DN("extra_link",lib->extra_link);
			CFG_BOOL_WRITE_DN("is_static",lib->is_static);
			CFG_BOOL_WRITE_DN("default_lib",lib->default_lib);
			CFG_BOOL_WRITE_DN("do_link",lib->do_link);
		};
		for(JavaVectorIterator<compile_extra_step> step(configurations[i]->extra_steps); step.IsValid(); step.Next()) {
			fil.AddLine("[extra_step]");
			CFG_GENERIC_WRITE_DN("name",step->name);
			CFG_GENERIC_WRITE_DN("deps",step->deps);
			CFG_GENERIC_WRITE_DN("output",step->out);
			CFG_GENERIC_WRITE_DN("command",step->command);
			wxString spos="unknown";
			switch (step->pos) {
			case CES_BEFORE_SOURCES:
				spos="before_sources";
				break;
			case CES_BEFORE_LIBS:
				spos="before_libs";
				break;
			case CES_BEFORE_EXECUTABLE:
				spos="before_executable";
				break;
			case CES_AFTER_LINKING:
				spos="after_linking";
				break;
			}
			CFG_GENERIC_WRITE_DN("position",spos);
			CFG_BOOL_WRITE_DN("check_retval",step->check_retval);
			CFG_BOOL_WRITE_DN("hide_win",step->hide_window);
			CFG_BOOL_WRITE_DN("delete_on_clean",step->delete_on_clean);
			CFG_BOOL_WRITE_DN("link_output",step->link_output);
		}
	}

	// configuracion wxfb
	if (wxfb && wxfb->activate_integration) {
		fil.AddLine("[wxfb]");
		CFG_BOOL_WRITE_DN("autoupdate_projects",wxfb->autoupdate_projects);
		CFG_BOOL_WRITE_DN("update_class_list",wxfb->update_class_list);
		CFG_BOOL_WRITE_DN("update_methods",wxfb->update_methods);
		CFG_BOOL_WRITE_DN("dont_show_base_classes_in_goto",wxfb->dont_show_base_classes_in_goto);
		CFG_BOOL_WRITE_DN("set_wxfb_sources_as_readonly",wxfb->set_wxfb_sources_as_readonly);
	}
	
	// configuracion Valgrind
	if (valgrind && !valgrind->IsEmpty()) {
		fil.AddLine("[valgrind]");
		CFG_GENERIC_WRITE_DN("arguments",valgrind->arguments);
		CFG_GENERIC_WRITE_DN("suppressions",valgrind->suppressions);
	}
	
	// configuracion Doxygen
	if (doxygen && doxygen->save) {
		fil.AddLine("[doxygen]");
		CFG_GENERIC_WRITE_DN("name",doxygen->name);
		CFG_GENERIC_WRITE_DN("version",doxygen->version);
		CFG_GENERIC_WRITE_DN("destdir",doxygen->destdir);
		CFG_GENERIC_WRITE_DN("lang",doxygen->lang);
		CFG_GENERIC_WRITE_DN("extra_files",doxygen->extra_files);
		CFG_GENERIC_WRITE_DN("base_path",doxygen->base_path);
		CFG_TEXT_WRITE_DN("extra_conf",doxygen->extra_conf);
		CFG_GENERIC_WRITE_DN("exclude_files",doxygen->exclude_files);
		CFG_BOOL_WRITE_DN("do_headers",doxygen->do_headers);
		CFG_BOOL_WRITE_DN("do_cpps",doxygen->do_cpps);
		CFG_BOOL_WRITE_DN("hideundocs",doxygen->hideundocs);
		CFG_BOOL_WRITE_DN("html",doxygen->html);
		CFG_BOOL_WRITE_DN("use_in_quickhelp",doxygen->use_in_quickhelp);
		CFG_BOOL_WRITE_DN("preprocess",doxygen->preprocess);
		CFG_BOOL_WRITE_DN("extra_private",doxygen->extra_private);
		CFG_BOOL_WRITE_DN("extra_static",doxygen->extra_static);
		CFG_BOOL_WRITE_DN("html_searchengine",doxygen->html_searchengine);
		CFG_BOOL_WRITE_DN("html_navtree",doxygen->html_navtree);
		CFG_BOOL_WRITE_DN("latex",doxygen->latex);
	}
	
	if (cppcheck && cppcheck->save_in_project) {
		fil.AddLine("[cppcheck]");
		CFG_BOOL_WRITE_DN("copy_from_config",cppcheck->copy_from_config);
		CFG_BOOL_WRITE_DN("exclude_headers",cppcheck->exclude_headers);
		CFG_GENERIC_WRITE_DN("config_d",cppcheck->config_d);
		CFG_GENERIC_WRITE_DN("config_u",cppcheck->config_u);
		CFG_GENERIC_WRITE_DN("style",cppcheck->style);
		CFG_GENERIC_WRITE_DN("platform",cppcheck->platform);
		CFG_GENERIC_WRITE_DN("standard",cppcheck->standard);
		CFG_GENERIC_WRITE_DN("suppress_file",cppcheck->suppress_file);
		CFG_GENERIC_WRITE_DN("suppress_ids",cppcheck->suppress_ids);
		CFG_GENERIC_WRITE_DN("inline_suppr",cppcheck->inline_suppr);
		CFG_GENERIC_WRITE_DN("additional_files",cppcheck->additional_files);
		CFG_GENERIC_WRITE_DN("exclude_list",cppcheck->exclude_list);
	}
	
	// guardar inspecciones actuales y tablas guardadas
	for(int i=0;i<main_window->inspection_ctrl->GetTabsCount();i++) {
		if (main_window->inspection_ctrl->PageIsInspectionsGrid(i)) {
			wxArrayString expressions;
			main_window->inspection_ctrl->GetInspectionGrid(i)->GetInspectionsList(expressions);
			if (expressions.IsEmpty()) continue;
			fil.AddLine("[inspections]");
			for(unsigned int j=0;j<expressions.GetCount();j++) 
				CFG_GENERIC_WRITE_DN("expr",expressions[j]);
		}
	}
	
	// guardar herramientas personalizadas
	fil.AddLine("[custom_tools]");
	custom_tools.WriteConfig(fil);
	
	// sellar, escribir, cerrar y terminar
	fil.AddLine("[end]");
	fil.Write();
	fil.Close();
	modified=false;
	return true;
	
}

wxString ProjectManager::GetFileName() {
	return filename.Mid(0,filename.Len()-4);
}

project_file_item *ProjectManager::FilesList::FindFromItem(const wxTreeItemId &tree_item) {
	for( GlobalListIterator<project_file_item*> it(&all); it.IsValid(); it.Next() ) {
		if (it->GetTreeItem()==tree_item) return *it;
	}
	return nullptr;
}

project_file_item *ProjectManager::FindFromName(wxString name) {
	for( GlobalListIterator<project_file_item*> it(&files.all); it.IsValid(); it.Next()) {
		if (wxFileName(it->GetRelativePath()).GetFullName()==name || it->GetFullPath()==name)
			return *it;
	}
	return nullptr;
}

/**
* @return puntero al project_file_item del archivo si lo encuenctra, nullptr si no lo encuentra
**/
project_file_item *ProjectManager::FindFromFullPath(wxString file) {
	file = mxUT::NormalizePath(file);
	GlobalListIterator<project_file_item*> it(&files.all);
	while (it.IsValid()) {
		if (it->GetFullPath()==file)
			return *it;
		it.Next();
	}
	return nullptr;
}

wxString ProjectManager::GetNameFromItem(wxTreeItemId &tree_item, bool relative) {
	project_file_item *item=files.FindFromItem(tree_item);
	if (item)
		return relative ? item->GetRelativePath() : item->GetFullPath();
	else
		return "";
}


bool ProjectManager::RenameFile(wxTreeItemId &tree_item, wxString new_name) {
	project_file_item *item = files.FindFromItem(tree_item);
	if (item) {
		wxFileName fname = new_name;
		fname.MakeRelativeTo(path);
		new_name = fname.GetFullPath();
		wxString src = item->GetFullPath();
		wxString dst = DIR_PLUS_FILE(path,new_name);
		if ( !FindFromFullPath(new_name) && !wxFileName::DirExists(dst) && 
			(!wxFileName::FileExists(dst) || 
				mxMessageDialog(main_window,LANG(PROJMNGR_CONFIRM_REPLACE,""
												 "Ya existe un archivo con ese nombre. Desea Reemplazarlo?"))
					.Title(LANG(GENERAL_WARNING,"Advertencia")).ButtonsYesNo().IconWarning().Run().yes) )
		{ 
			parser->RenameFile(item->GetFullPath(),DIR_PLUS_FILE(path,new_name));
			item->Rename(path,new_name);
			modified=true;
			wxRenameFile(src,dst,true);
			main_window->project_tree.RenameFile(item->GetTreeItem(),item->GetRelativePath());
			mxSource *source;
			for (int i=0,j=main_window->notebook_sources->GetPageCount();i<j;i++)
				if (((mxSource*)(main_window->notebook_sources->GetPage(i)))->source_filename==src) {
					source = ((mxSource*)(main_window->notebook_sources->GetPage(i)));
					source->SaveSource(dst);
					source->SetPageText(wxFileName(dst).GetFullName());
					parser->ParseSource(source,true);
					return true;
				} 
			return true;
		}
	}
	return false;
}

void ProjectManager::FilesList::ChangeCategory(project_file_item *item, eFileType new_category) {
	all.FindAndRemove(item);
	// elegir la lista adecuada y agregarlo al arbol de proyecto
	switch (new_category) {
	case FT_SOURCE:    sources.Add(item);   break;
	case FT_HEADER:    headers.Add(item);   break;
	case FT_BLACKLIST: blacklist.Add(item); break;
	default:           others.Add(item);    break;
	};
	item->SetCategory(new_category);
}

void ProjectManager::SetFileAsOwn(project_file_item *fitem) {
	EXPECT_OR(fitem!=nullptr,return);
	fitem->m_inherited_from.Clear();
	main_window->project_tree.SetInherited(fitem->GetTreeItem(),false);
}

void ProjectManager::MoveFile(wxTreeItemId &tree_item, eFileType where) {
	// buscar el archivo
	project_file_item *item = files.FindFromItem(tree_item);
	EXPECT_OR(item!=nullptr,return); // 
	if (where==FT_NULL) mxUT::GetFileType(item->GetRelativePath(),false);
	if (item->GetCategory()==where) return; // nothing to do
	modified=true;
	// cambiar de lista en el FileList
	files.ChangeCategory(item,where); 
	// eliminar del arbol	
	wxTreeItemId new_tree_item = main_window->project_tree.MoveFile(item->GetTreeItem(),where,true);
	item->SetTreeItem( new_tree_item );
	// if it was moved to source or hedear, parse it (could came from others), else remove its data from parser
	if (where==FT_SOURCE || where==FT_HEADER) {
		wxString name = item->GetFullPath();
		mxSource *source;
		if ((source=main_window->IsOpen(name)) && source->GetModify()) {
			parser->ParseSource(source);
		} else {
			parser->ParseFile(name);
		}
		if (!item->AreSymbolsVisible()) parser->SetHideSymbols(name,true);
	} else {
		wxString name = item->GetFullPath();
		parser->RemoveFile(name);
	}
}

void ProjectManager::DeleteFile(project_file_item *item, bool also_delete_from_disk) {
	// cerrar si está abierto
	mxSource *src = main_window->IsOpen(item->GetTreeItem());
	if (src) {
		if (also_delete_from_disk) main_window->CloseSource(src);
		else src->SetNotInTheProjectAnymore();
	}
	// eliminar el archivo del disco
	wxString fullpath = item->GetFullPath();
	if (also_delete_from_disk) wxRemoveFile(fullpath);
	// eliminar sus simbolos del parser
	parser->RemoveFile(fullpath);
	// quitar el archivo del arbol de proyecto
	main_window->project_tree.DeleteFile(item->GetTreeItem());
	// eliminar el item de la lista
	files.all.FindAndRemove(item);
	if (wxfb && wxfb->projects.Contains(fullpath)) WxfbGetFiles();
	delete item;
}

bool ProjectManager::DeleteFile(wxTreeItemId tree_item) {
	bool also=false;
	modified=true;
	while (true) {
		project_file_item *item = files.FindFromItem(tree_item);
		if (!item) return also;
		mxSource *src = main_window->IsOpen(tree_item);
		if (src) new SourceExtras(src); // "tranfers ownership" of extras to the mxSource
		mxMessageDialog::mdAns ans = also
			? mxMessageDialog(main_window,LANG1(PROJMNGR_CONFIRM_DETACH_ALSO,""
												"¿Desea quitar tambien el archivo \"<{1}>\" del proyecto?",item->GetRelativePath()))
				.Check1(LANG(PROJMNGR_DELETE_FROM_DISK,"Eliminar el archivo del disco"),false)
				.Title(item->GetRelativePath()).ButtonsYesNo().IconQuestion().Run()
			: mxMessageDialog(main_window,LANG(PROJMNGR_CONFIRM_DETACH_FILE,""
											   "¿Desea quitar el archivo del proyecto?"))
				.Check1(LANG(PROJMNGR_DELETE_FROM_DISK,"Eliminar el archivo del disco"),false)
				.Title(item->GetRelativePath()).ButtonsYesNo().IconQuestion().Run();
		if (ans.cancel || ans.no) return false;
		wxString comp = mxUT::GetComplementaryFile(item->GetFullPath());
		DeleteFile(item,ans.check1);
		if (comp.Len()==0 || !FindFromFullPath(comp)) return true;
		tree_item = FindFromFullPath(comp)->GetTreeItem(); also=true;
	}
}

bool ProjectManager::DependsOnMacro(project_file_item *item, wxArrayString &macros) {
	wxTextFile fil(item->GetFullPath());
	if (fil.Exists()) {
		fil.Open();
		unsigned int i,l=macros.GetCount();
		for ( wxString str = fil.GetFirstLine(); !fil.Eof(); str = fil.GetNextLine()) {
			for (i=0;i<l;i++) {
				int j=str.Find(macros[i]), n=macros[i].Len();
				if ( j!=wxNOT_FOUND // si estaba...
					// ...y antes no habia letra/numero/guion_bajo...
					&& ( j==0 || ( !(str[j-1]>='a'&&str[j-1]<='z')&&!(str[j-1]>='A'&&str[j-1]<='Z')&&!(str[j-1]>='0'&&str[j-1]<='9')&&str[j-1]!='_' ) ) 
					// ...y despues no habia letra/numero/guion_bajo...
					&& ( j+n==int(str.Len()) || ( !(str[j+n]>='a'&&str[j+n]<='z')&&!(str[j+n]>='A'&&str[j+n]<='Z')&&!(str[j+n]>='0'&&str[j+n]<='9')&&str[j+n]!='_' ) ) 
					)
						return true;
			}
		}
		fil.Close();
	}
	return false;
}


/**
* Si only_one es nullptr se analiza todo el proyecto (fuentes, bibliotecas, pasos 
* adicionales, ejecutable, etc). Si no es nullptr se analiza solo lo relacionado 
* a ese archivo en particular, para recompilar un solo objeto manualmente desde
* el menú contextual del arbol de proyecto.
**/
bool ProjectManager::PrepareForBuilding(project_file_item *only_one) {
	
	mxUT::ClearIncludesCache();
	
	// borrar los pasos que hayan quedado incompletos de compilaciones anteriores
	// para preparar la nueva lista, se pone un nodo ficticio para facilitar la 
	// "insersion", que siempre sera al final, se evita preguntar si es el primero
	while (first_compile_step) {
		compile_step *s = first_compile_step->next;
		delete first_compile_step;
		first_compile_step=s;
	}
	warnings.Clear();

	compile_was_ok=true; // inicialmente no hay errores, se va a activar si un paso falla
	
	// si se encarga otro....
	if (current_toolchain.IsExtern()) {
		current_step=0;
		steps_count=1;
		SaveAll(false);
		first_compile_step = new compile_step(CNS_EXTERN,nullptr);
		return true;
	}
	
	bool retval=false, relink_exe=false;
	config_analized=false;
	
	GetTempFolder(true); // crear el directorio para los objetos si no existe
	
	// prepara la info de las bibliotecas
	AssociateLibsAndSources(active_configuration);
	
	compile_step *step=first_compile_step=new compile_step(CNS_VOID,nullptr);
	current_step=steps_count=0;

	wxString full_path;
	wxDateTime bin_date, youngest_bin(wxDateTime::Now());
	youngest_bin.SetYear(1900);
	
	wxString extra_step_for_link;
		
	// guardar todos los fuentes abiertos
	SaveAll(false);
	if (!only_one) {
		if (GetWxfbActivated()) WxfbGenerate();
		
		// agregar los items extra previos a los fuentes
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()) {
			if (!extra_step->pos==CES_BEFORE_SOURCES) continue;
			if (ShouldDoExtraStep(extra_step)) {
				steps_count++;
				step = step->next = new compile_step(CNS_EXTRA,extra_step);
				if (extra_step->out.Len()) {
					project_file_item *fitem = project->FindFromFullPath(DIR_PLUS_FILE(path,extra_step->out));
					if (fitem) fitem->ForceRecompilation();
					if (extra_step->link_output) force_relink=true;
				}
				retval=true;
			}
		}
	}
	
	// marcar los headers que cambiaron por culpa de las macros
	wxArrayString dif_mac;
	if (!only_one && active_configuration->old_macros!=active_configuration->macros) {
		wxArrayString omac,nmac; 
		mxUT::Split(active_configuration->old_macros,omac,true,true);
		mxUT::Split(active_configuration->macros,nmac,true,true);
		for (unsigned int i=0;i<omac.GetCount();i++) {
			int j=nmac.Index(omac[i]);
			if (j==wxNOT_FOUND) dif_mac.Add(omac[i]);
			else nmac.RemoveAt(j);
		}
		for (unsigned int i=0;i<nmac.GetCount();i++)
			dif_mac.Add(nmac[i]);
		for (unsigned int i=0;i<dif_mac.GetCount();i++)
			if (dif_mac[i].Contains('='))
				dif_mac[i]=dif_mac[i].BeforeFirst('=');
		for ( LocalListIterator<project_file_item*> litem(&files.headers); litem.IsValid(); litem.Next() ) {
			if (!litem->ShouldBeRecompiled() && DependsOnMacro(*litem,dif_mac))
				litem->ForceRecompilation();
		}
	}
	
	// ver si cambiaron 
	wxArrayString header_dirs_array;
	mxUT::Split(active_configuration->headers_dirs,header_dirs_array,true,false);
	for (unsigned int i=0;i<header_dirs_array.GetCount();i++) 
		header_dirs_array[i]=DIR_PLUS_FILE(path,header_dirs_array[i]);
	
	compile_step *prev_to_sources=step;
	wxDateTime now=wxDateTime::Now();
	for( LocalListIterator<project_file_item*> item(&files.sources); item.IsValid(); item.Next() ) {
		bool flag=false;
		if ( ( only_one && only_one==*item ) || item->ShouldBeRecompiled() ) {
			flag=true;
		} else if (!only_one && (!active_configuration->dont_generate_exe || item->IsInALibrary())) {
			full_path = item->GetFullPath();
			wxFileName bin_name ( item->GetBinName(temp_folder) );
			wxDateTime src_date=wxFileName(full_path).GetModificationTime();
			// nota: se usa getseconds porque comparar con < anda mal en windows (al menos en xp, funciona como <=)
			if ((now-src_date).GetSeconds().ToLong()<-3) { // si el fuente es del futuro, touch
				wxFileName(full_path).Touch();
				mxSource *src = main_window->IsOpen(item->GetTreeItem());
				if (src) src->Reload();
				warnings.Add(LANG1(PROJMNGR_FUTURE_SOURCE,"El fuente <{1}> tenia fecha de modificación en el futuro. Se reemplazó por la fecha actual.",full_path));
				mxUT::AreIncludesUpdated(bin_date,full_path,header_dirs_array,true);
				flag=true;
			} else 
			if (bin_name.FileExists()) {
				bin_date = bin_name.GetModificationTime();
				if (bin_date>youngest_bin) // guardar el objeto mas nuevo para comparar con el ejecutable
					youngest_bin=bin_date;
				if (src_date>bin_date) // si el objeto esta desactualizado respecto al fuente 
					flag=true;
				else if (bin_date.IsLaterThan(now)) { // si el objeto es del futuro (por las dudas)
					warnings.Add(LANG1(PROJMNGR_FUTURE_OBJECT,"El objeto <{1}> tenia fecha de modificación en el futuro. Se recompilara el fuente.",full_path));
					flag=true;
				} else { // ver si el objeto esta desactualizado respecto a los includes del fuente
					flag=mxUT::AreIncludesUpdated(bin_date,full_path,header_dirs_array,true);
					if (!flag && dif_mac.GetCount()) flag=DependsOnMacro(*item,dif_mac); // si cambiaron las macros y el fuente las usa
				}
			} else { // si el objeto no existe, hay que compilar
				flag=true;
			}
		} else if (active_configuration->dont_generate_exe && !item->IsInALibrary()) {
			warnings.Add(LANG1(PROJMNGR_NO_LIB_FOR_THAT_SOURCE,"El fuente \"<{1}>\" no esta asociado a ninguna biblioteca.",item->GetRelativePath()));
		}
		if (flag) {
			step = step->next = new compile_step(CNS_SOURCE,*item);
			if (item->IsInALibrary()) {
//				item->ForceRecompilation();
				item->GetLibrary()->need_relink=true;
			} else
				relink_exe=true;
			steps_count++;
		}
	}
	
	if (prev_to_sources->next) prev_to_sources->next->start_parallel=true;
	step = step->next = new compile_step(CNS_BARRIER,nullptr);
	if (!only_one) {
		// agregar los items extra previos al enlazado de bibliotecas
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()) {
			if (extra_step->pos!=CES_BEFORE_LIBS) continue;
			if (ShouldDoExtraStep(extra_step)) {
				steps_count++; relink_exe=true;
				step = step->next = new compile_step(CNS_EXTRA,extra_step);
			}
		}
		
		// agregar el enlazado de bibliotecas
		for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build); lib.IsValid(); lib.Next()) {
			wxString lib_dir = lib->GetPath(this);
			if (lib_dir.Len() && !wxFileName::DirExists(DIR_PLUS_FILE(path,lib_dir)))
				wxFileName::Mkdir(DIR_PLUS_FILE(path,lib_dir),0777,wxPATH_MKDIR_FULL);
			if (lib->need_relink || !wxFileName(DIR_PLUS_FILE(path,lib->filename)).FileExists()) {
				AnalizeConfig(path,true,current_toolchain.mingw_dir,false); // esto no fuerza el "reanalizado", pero se puede llegar hasta aca sin haberlo hecho antes??
				if (lib->objects_list.Len()) {
					step = step->next = new compile_step(CNS_LINK,new linking_info(
						lib->is_static?current_toolchain.static_lib_linker:current_toolchain.dynamic_lib_linker,
						DIR_PLUS_FILE(path,lib->filename),lib->objects_list,lib->parsed_extra,&(lib->need_relink)));
					steps_count++;
					lib->need_relink=true; // para que sepa el loop que sigue (el del strip_executable)
					if (lib->do_link) relink_exe=true; else retval=true;
				} else {
					lib->need_relink=false; // para que sepa el loop que sigue (el del strip_executable)
					warnings.Add(LANG1(PROJMNGR_LIB_WITHOUT_SOURCES,"La biblioteca \"<{1}>\" no tiene ningun fuente asociado.",lib->libname));
				}
			}
		}
		
		// si hay que mover la info de depuracion afuera de los binarios, agregar esos pasos
		if (active_configuration->strip_executable==DBSACTION_COPY) {
			step = step->next = new compile_step(CNS_BARRIER,nullptr);
			for(int k=0;k<3;k++) {
				for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next()) { 
					if (lib->need_relink && !lib->is_static) {
						step = step->next = new compile_step(CNS_DEBUGSYM,new stripping_info(DIR_PLUS_FILE(path,lib->filename),"",k));
						if (k==0) steps_count++;
					}
				}
			}
		}
		
		// agregar los items extra posteriores al enlazado de blibliotecas
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()) {
			if (extra_step->pos!=CES_BEFORE_EXECUTABLE) continue;
			if (ShouldDoExtraStep(extra_step)) {
				steps_count++; relink_exe=true;
				step = step->next = new compile_step(CNS_EXTRA,extra_step);
			}
		}
		

#ifdef __WIN32__
		bool rc_redo=false;
		wxArrayString rc_text;
		if (active_configuration->icon_file.Len()) { // ver si hay que compilar el recurso del icono
			wxFileName ficon_in(DIR_PLUS_FILE(path,active_configuration->icon_file));
			if (ficon_in.FileExists()) {
				wxFileName ficon_out(DIR_PLUS_FILE(temp_folder,"zpr_resource.o"));
				if (ficon_in.FileExists() && (!ficon_out.FileExists() || ficon_in.GetModificationTime()>ficon_out.GetModificationTime() ) )
					rc_redo=true;
				wxString icon_name=active_configuration->icon_file;
				icon_name.Replace("\\","\\\\",true);
				rc_text.Add(ICON_LINE(icon_name));
			} else {
				warnings.Add(LANG(PROJMNGR_ICON_NOT_FOUND,"No se ha encontrado el icono del ejecutable."));
			}
		}
		if (active_configuration->manifest_file.Len()) { // ver si hay que compilar el recurso del manifest
			wxFileName ficon_in(DIR_PLUS_FILE(path,active_configuration->manifest_file));
			if (ficon_in.FileExists()) {
				wxFileName ficon_out(DIR_PLUS_FILE(temp_folder,"zpr_resource.o"));
				if (ficon_in.FileExists() && (!ficon_out.FileExists() || ficon_in.GetModificationTime()>ficon_out.GetModificationTime() ) )
					rc_redo=true;
				wxString icon_name=active_configuration->manifest_file;
				icon_name.Replace("\\","\\\\",true);
				rc_text.Insert("#include \"winuser.h\"",0);
				rc_text.Add(MANIFEST_LINE(icon_name));
			} else {
				warnings.Add(LANG(PROJMNGR_MANIFEST_NOT_FOUND,"No se ha encontrado el archivo manifest.xml."));
			}
		}
		wxFileName rc_file(DIR_PLUS_FILE(temp_folder,"zpr_resource.rc"));
		if (rc_text.GetCount()) { 
			// ver si cambio o falta el archivo .rc
			if (!rc_file.FileExists())
				rc_redo=true;
			else if (!rc_redo) {
				wxTextFile fil(rc_file.GetFullPath());
				fil.Open();
				wxString cont; 
				rc_redo=rc_text.GetCount()!=fil.GetLineCount();
				if (!rc_redo) {
					int i=0;
					for ( wxString str = fil.GetFirstLine(); !fil.Eof(); str = fil.GetNextLine() )
						if (rc_text[i++]!=str) rc_redo=true;
				}
				fil.Close();
			}
			if (rc_redo) {
				wxTextFile fil(rc_file.GetFullPath());
				if (fil.Exists())
					fil.Open();
				else
					fil.Create();
				fil.Clear();
				for (unsigned int i=0;i<rc_text.GetCount();i++)
					fil.AddLine(rc_text[i]);
				fil.Write();
				fil.Close();
				
				steps_count++; relink_exe=true;
				step = step->next = new compile_step(CNS_ICON,new wxString(DIR_PLUS_FILE(path,active_configuration->icon_file)));
			}
		} 
#endif
	
		// si ningun objeto esta desactualizado, ver si hay que relinkar
		if (!active_configuration->dont_generate_exe) {
			wxFileName exe_file(GetExePath());
			if (!wxFileName::DirExists(exe_file.GetPath()) ) // si el directorio del exe no existe, crearlo
				wxFileName::Mkdir(exe_file.GetPath(),0777,wxPATH_MKDIR_FULL);
			if (!relink_exe) { // si no hay que actualizar ningun objeto, preguntar por el exe
				if (force_relink || !exe_file.FileExists() || exe_file.GetModificationTime()<youngest_bin)
					relink_exe=true;
			} else {
				relink_exe=true;
			}

			if (relink_exe) {
				AnalizeConfig(path,true,current_toolchain.mingw_dir,false);
				wxString output_bin_file=executable_name;
				if (debug->IsDebugging()) debug->GetPatcher()->AlterOutputFileName(output_bin_file);
				step = step->next = new compile_step(CNS_LINK,
													new linking_info(current_toolchain.linker+" -o",
													output_bin_file,objects_list,linking_options,&force_relink));
				steps_count++;
				if (active_configuration->strip_executable==DBSACTION_COPY) {
					int ini=0;
#ifdef __WIN32__
					// en windows no podemos modificar archivos que se esten usando, y ademas compilamos 
					// en uno alternativo para parchearlo en gdb, asi que solo lo stripeamos y linkamos 
					// para que quede como el original, pero no reescribimos el archivo de la info de depuracion
					if (debug->IsDebugging()) ini=1;
#endif
					for(int k=ini;k<3;k++) step = step->next = new compile_step(CNS_DEBUGSYM,new stripping_info(output_bin_file,"",k));
					steps_count++;
				}
			}
		} else if (relink_exe) {
			AnalizeConfig(path,true,current_toolchain.mingw_dir,false);
		}
	} else  { // si only_one
		// configurar si hay que recompilar un solo fuente
		AnalizeConfig(path,true,current_toolchain.mingw_dir,false);
		relink_exe=true;
	}
	
	if (!only_one) {
		// agregar los items extra posteriores al reenlazado
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()) {
			if (extra_step->pos!=CES_AFTER_LINKING) continue;
			if (ShouldDoExtraStep(extra_step)) {
				steps_count++;
				step = step->next = new compile_step(CNS_EXTRA,extra_step);
			}
		}
		// /*reestablecer la bandera force_recompile de los headers*/ y guardar lista de macros
		active_configuration->old_macros = active_configuration->macros;
//		for( LocalListIterator<project_file_item*> item(&files.headers); item.IsValid(); item.Next() ) { 
//			item->ForceRecompilation(false);
//		}
	}
	
	// calcular la cantidad de pasos y el progreso en cada uno
	if (steps_count)
		progress_step=100.0/(steps_count);
	actual_progress = -progress_step/2;
	
	// eliminar el primer nodo ficticio de la lista de pasos
	step = first_compile_step->next;
	delete first_compile_step;
	first_compile_step = step;
	
	// resetear los flags de force_recompile
	for( GlobalListIterator<project_file_item*> gitem(&files.all); gitem.IsValid(); gitem.Next() ) {
		gitem->ForceRecompilation(false);
	}
	
	retval|=relink_exe;
	
	return retval;
}

/** 
* Guarda todos los archivos del proyecto que esten abiertos y eventualmente tambien
* el archivo de configuración de proyecto. Si se indica guardar tambien este archivo,
* pero el proyecto no ha cambiado, no lo reescribe.
* @param save_project determina si también se debe guardar el archivo de configuración de proyecto
**/
void ProjectManager::SaveAll(bool save_project) {
	// guardar los fuentes
	GlobalListIterator<project_file_item*> item(&files.all);
	while (item.IsValid()) {
		mxSource *source=main_window->IsOpen(item->GetFullPath());
		if (source && source->GetModify()) {
			source->SaveSource();
			if (item->ShouldBeParsed()) parser->ParseSource(source,true);
		}
		item.Next();
	}
	// guardar la configuracion del proyecto
	if (save_project && modified) Save();
}


long int ProjectManager::CompileFile(compile_and_run_struct_single *compile_and_run, project_file_item *item) {
	compile_and_run->step_label=item->GetRelativePath();
	compile_and_run->compiling=true;
//	item->force_recompile=false;
	
	// preparar la linea de comando 
	wxString fname = item->GetFullPath();
	wxString command = wxString(item->IsCppOrJustC()?current_toolchain.cpp_compiler:current_toolchain.c_compiler)+
#ifndef __WIN32__
		(item->IsInALibrary()?" -fPIC ":" ")+
#endif
		item->compiling_options+" "+mxUT::Quotize(fname)+" -c -o "+mxUT::Quotize(item->GetBinName(temp_folder));
	
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	
	// ejecutar
	compile_and_run->output_type=MXC_GCC;
	compile_and_run->m_cem_state = errors_manager->InitCompiling(command);
	return mxUT::Execute(path,command, wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,compile_and_run->process);	
}


long int ProjectManager::CompileFile(compile_and_run_struct_single *compile_and_run, wxFileName filename) {
	AnalizeConfig(path,true,current_toolchain.mingw_dir);
	// crear el directorio para los objetos si no existe
	if (!wxFileName::DirExists(temp_folder))
		wxFileName::Mkdir(temp_folder,0777,wxPATH_MKDIR_FULL);
	
	GlobalListIterator<project_file_item*> item(&files.all);
	while (item.IsValid()) {
		if (wxFileName(item->GetFullPath())==filename)
			return CompileFile(compile_and_run,*item);
		item.Next();
	}
	return 0;

}

/** 
* Compila un fuente o enlaza el ejecutable si ya estan todos los objetos listos.
* El paso que ejecuta es el primero de la lista (first_compile_step), y además 
* lo saca de esta lista. Puede fallar si ya no quedan pasos o si hay otros pasos
* en paralelos que deben finalizar antes de continuar (esto es cuando el próximo
* paso es de tipo CNS_BARRIER). Se encarga además de lanzar otros pasos en paralelo 
* si es el primero de un conjunto paralelizable (lo cual se indica en la propiedad 
* start_parallel del paso). Devuelve el pid del proceso que lanzo para 
* compilar/enlazar o 0 en caso contrario. Coloca el nombre corto del paso que 
* está ejecutando en caption para que se muestre en la interfaz.
**/
long int ProjectManager::CompileNext(compile_and_run_struct_single *compile_and_run, wxString &caption) {

	if (first_compile_step && first_compile_step->type==CNS_BARRIER) {
		if (compiler->NumCompilers()>1) {
			return 0;
		} else {
			compile_step *step = first_compile_step;
			first_compile_step = step->next;
			delete step;
			return CompileNext(compile_and_run,caption);
		}
	}

	compile_and_run->pid=0;
	
	main_window->SetStatusProgress( int(actual_progress+=progress_step) );
	if (first_compile_step && (first_compile_step->type!=CNS_DEBUGSYM || ((stripping_info*)first_compile_step->what)->current_step==0)) ++current_step;
	main_window->SetStatusText(wxString()<<LANG(PROJMNGR_COMPILING,"Compilando")<<"... ( "<<LANG2(PROJMNGR_STEP_FROM,"paso <{1}> de <{2}>",wxString()<<(current_step),wxString()<<steps_count)<<" - "<<int(actual_progress)<<"% )");
	
	if (first_compile_step) {
		compile_step *step = first_compile_step;
		switch (step->type) {
		case CNS_EXTERN: // for custom extern toolchains
			caption="";
			compile_and_run->pid = CompileWithExternToolchain(compile_and_run);
			break;
		case CNS_ICON:
			caption=wxString(LANG(PROJMNGR_COMPILING,"Compilando"))<<" \""<<((project_file_item*)step->what)->GetRelativePath()<<"\"...";
			compile_and_run->pid = CompileIcon(compile_and_run,*((wxString*)step->what));
			delete ((wxString*)step->what);
			break;
		case CNS_SOURCE:
			compile_and_run->pid = CompileFile(compile_and_run,(project_file_item*)step->what);
			// ver si hay que lanzar otra en paralelo
			if (step->start_parallel) {
				int nc=1;
				while (step->next && step->next->type==CNS_SOURCE && nc<config->Init.max_jobs) {
					first_compile_step = step->next; delete step; step=first_compile_step; current_step++;
					compile_and_run_struct_single *compile_and_run_2=new compile_and_run_struct_single(compile_and_run);
					compile_and_run_2->pid=CompileFile(compile_and_run_2,(project_file_item*)step->what); nc++;
					if (compile_and_run_2->pid<=0) {
						delete compile_and_run_2; break;
					}
				}
			}
			caption=compiler->GetCompilingStatusText();
			break;
		case CNS_EXTRA:
			caption=wxString(LANG(PROJMNGR_RUNNING,"Ejecutando"))<<" \""<<((compile_extra_step*)step->what)->name<<"\"...";
			compile_and_run->pid = CompileExtra(compile_and_run,(compile_extra_step*)step->what);
			break;
		case CNS_LINK:
			if (!compile_was_ok) return 0;
			caption=wxString(LANG(PROJMNGR_LINKING,"Enlazando \""))<<((linking_info*)step->what)->output_file<<"\"...";
			compile_and_run->pid = Link(compile_and_run,(linking_info*)step->what);
			delete ((linking_info*)step->what);
			break;
		case CNS_DEBUGSYM:
			if (!compile_was_ok) return 0;
			caption=LANG2(PROJMNGR_STRIPING,"Extrayendo información de depuración de \"<{1}>\" - paso <{2}> de 3...",((stripping_info*)step->what)->filename,(wxString()<<((stripping_info*)step->what)->current_step+1));
			compile_and_run->pid = Strip(compile_and_run,(stripping_info*)step->what);
			delete ((stripping_info*)step->what);
			break;
		default:
			;
		}
		first_compile_step = step->next;
		delete step;
	}
	if (!compile_and_run->pid) fms_move(compile_and_run->on_end,post_compile_action);
	return compile_and_run->pid;
}

long int ProjectManager::Link(compile_and_run_struct_single *compile_and_run, linking_info *info) {
	(*info->flag_relink)=false;
	wxString command = info->command;
	command<<" "+mxUT::Quotize(info->output_file)+" ";
	command<<info->objects_list+" "+info->extra_args;
	compile_and_run->process=new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	compile_and_run->output_type=MXC_SIMIL_GCC;
	compile_and_run->linking=true; // para que cuando termine sepa que enlazo, para verificar llamar a compiler->CheckForExecutablePermision
	compile_and_run->m_cem_state = errors_manager->InitLinking(command);
	return mxUT::Execute(path, command, wxEXEC_ASYNC,compile_and_run->process);
}

/**
* @param paso sirve para saber que paso del proceso de extración se tiene que ejecutar, porque si bien zinjai lo presenta como una sola cosa, en realidad lleva 3 pasos (extraer del ejecutable, stripearle, y añadir la referencia)
**/
long int ProjectManager::Strip(compile_and_run_struct_single *compile_and_run, stripping_info *info) {
	wxString command;
	if (info->current_step==0) command<<"objcopy --only-keep-debug "<<mxUT::Quotize(info->fullpath)<<" "<<mxUT::Quotize(info->fullpath+".dbg");
	else if (info->current_step==1) command<<"strip "<<mxUT::Quotize(info->fullpath);
	else if (info->current_step==2) command<<"objcopy --add-gnu-debuglink="<<mxUT::Quotize(info->fullpath+".dbg")<<" "<<mxUT::Quotize(info->fullpath);
	compile_and_run->compiling=compile_and_run->linking=false;
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	compile_and_run->output_type=MXC_EXTRA;
	compile_and_run->step_label=wxString("Extrayendo símbolos de depuración de ")<<info->filename<<" - paso "<<info->current_step+1<<" de 3";
	compile_and_run->m_cem_state = errors_manager->InitOtherStep(command);
	return mxUT::Execute(path, command, wxEXEC_ASYNC,compile_and_run->process);
}

long int ProjectManager::Run() {
	// ver que no sea un proyecto sin ejecutable
	if (active_configuration->dont_generate_exe) {
		mxMessageDialog(main_window,LANG(PROJMNGR_RUNNING_NO_EXE,""
										 "Este proyecto no puede ejecutarse porque está configurado\n"
										 "para generar solo bibliotecas."))
			.Title(LANG(GENERAL_WARNING,"Aviso")).IconWarning().Run();
		return 0;
	}
	
	compile_and_run_struct_single *compile_and_run=new compile_and_run_struct_single("ProjectManager::Run");
	compile_and_run->linking=compile_and_run->compiling=false;
	
	// agregar los argumentos de ejecucion
	if (active_configuration->always_ask_args) {
		int res = mxArgumentsDialog(main_window,path,active_configuration->args,active_configuration->working_folder).ShowModal();
		if (res==0) { delete compile_and_run; return 0; }
		active_configuration->working_folder = mxArgumentsDialog::last_workdir;
		active_configuration->args = (res&AD_EMPTY) ? "" : mxArgumentsDialog::last_arguments;
		if (res&AD_REMEMBER) active_configuration->always_ask_args=false;
	} 
	
	wxString exe_pref;
#ifndef __WIN32__
	// agregar el prefijo para valgrind
	if (compile_and_run->valgrind_cmd.Len()) exe_pref = compile_and_run->valgrind_cmd+" ";
	else if (active_configuration->exec_method==EMETHOD_WRAPPER) exe_pref = active_configuration->exec_script+" ";
#endif
	// armar la linea de comando para ejecutar
	executable_name=GetExePath();
	
	wxString working_path = active_configuration->working_folder.Len()?DIR_PLUS_FILE(path,active_configuration->working_folder):path;
	if (working_path.Last()==path_sep) working_path.RemoveLast();
	
	wxString command=exe_pref<<mxUT::Quotize(wxFileName(executable_name).GetFullPath());
	if (active_configuration->exec_method==EMETHOD_SCRIPT) 
#ifdef __WIN32__
		command=mxUT::Quotize(DIR_PLUS_FILE(path,active_configuration->exec_script));
#else
		command="/bin/sh "+mxUT::Quotize(DIR_PLUS_FILE(path,active_configuration->exec_script));
#endif
	
	if (active_configuration->args.Len()) command<<' '<<active_configuration->args;	
	
	if (active_configuration->exec_method==EMETHOD_INIT) {
#ifdef __WIN32__
		command=DIR_PLUS_FILE(path,active_configuration->exec_script)<<" & "<<command;
#else
		command=wxString()<<"/bin/sh -c "<<mxUT::SingleQuotes(wxString()
			<<". "<<DIR_PLUS_FILE(path,active_configuration->exec_script)<<"; "<<command);
#endif
	}
	
	if (active_configuration->console_program) { // si es de consola...
		wxString terminal_cmd(config->Files.terminal_command);
		terminal_cmd.Replace("${TITLE}",LANG(GENERA_CONSOLE_CAPTION,"ZinjaI - Consola de Ejecucion")); // NO USAR ACENTOS, PUEDE ROMER EL X!!!! (me daba un segfault en la libICE al poner el ó en EjeuciÓn)
		if (terminal_cmd.Len()!=0) { // corregir espacios al final del comando de la terminal
			if (terminal_cmd==" " ) terminal_cmd="";
			else if (terminal_cmd[terminal_cmd.Len()-1]!=' ') terminal_cmd<<" ";
		}
		// invocar al runner
		terminal_cmd<<mxUT::GetRunnerBaseCommand(active_configuration->wait_for_key);
		terminal_cmd<<mxUT::Quotize(working_path)<<" ";
		// corregir el comando final
		command=terminal_cmd+command;
	}
	
	// lanzar la ejecucion
#ifdef __WIN32__
	wxString ldlp_vname="PATH";
	wxString ldlp_sep=";";
#else
	wxString ldlp_vname="LD_LIBRARY_PATH";
	wxString ldlp_sep=":";
#endif
	
	EnvVars::SetMode(EnvVars::RUNNING);
	
	int pid;
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	if (active_configuration->console_program)
		pid = wxExecute(command, wxEXEC_NOHIDE|wxEXEC_MAKE_GROUP_LEADER, compile_and_run->process);
	else
		pid = mxUT::Execute(working_path,command,wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER,compile_and_run->process);
	compile_and_run->pid=pid;
		
	EnvVars::Reset();
	
	main_window->StartExecutionStuff(compile_and_run,LANG(GENERAL_RUNNING_DOTS,"Ejecutando..."));
	
	return pid;
}


project_configuration *ProjectManager::GetConfig(wxString name) {
	for (int i=0;i<configurations_count;i++)
		if (configurations[i]->name==name)
			return configurations[i];
	return nullptr;
}

wxString ProjectManager::GetCustomStepCommand(const compile_extra_step *step) {
	wxString deps, output;
	return GetCustomStepCommand(step,current_toolchain.mingw_dir,deps,output);
}

wxString ProjectManager::GetCustomStepCommand(const compile_extra_step *step, wxString mingw_dir, wxString &deps, wxString &output) {
	wxString command = step->command;
	output = step->out;
	deps = step->deps;
	mxUT::ParameterReplace(deps,"${TEMP_DIR}",project->temp_folder);
	mxUT::ParameterReplace(deps,"${PROJECT_PATH}",project->path);
	mxUT::ParameterReplace(deps,"${PROJECT_BIN}",project->executable_name);
	mxUT::ParameterReplace(output,"${TEMP_DIR}",project->temp_folder);
	mxUT::ParameterReplace(output,"${PROJECT_PATH}",project->path);
	mxUT::ParameterReplace(output,"${PROJECT_BIN}",project->executable_name);
	mxUT::ParameterReplace(command,"${TEMP_DIR}",project->temp_folder);
	mxUT::ParameterReplace(command,"${PROJECT_PATH}",project->path);
	mxUT::ParameterReplace(command,"${PROJECT_BIN}",project->executable_name);
//#ifdef __WIN32__
	mxUT::ParameterReplace(output,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(deps,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(command,"${MINGW_DIR}",mingw_dir);
//#endif
	mxUT::ParameterReplace(command,"${DEPS}",deps);
	mxUT::ParameterReplace(command,"${OUTPUT}",output);
	mxUT::ParameterReplace(command,"${ZINJAI_DIR}",config->zinjai_dir);
	return command;
}

static wxString get_percent(int cur, int tot) {
	wxString s; s<<(cur*100)/tot;
	if (s.Len()==1) s=wxString("  ")+s;
	else if (s.Len()==2) s=wxString(" ")+s;
	return wxString("\"[")+s+"%]\"";
}

void ProjectManager::ExportMakefile(wxString make_file, bool exec_comas, wxString mingw_dir, MakefileTypeEnum mktype, bool cmake_style) {
#warning TODO: considerar el nuevo significado de strip_executable
#warning TODO: considerar las opciones de compilacion por fuente
	
	// calcular cuantos pasos hay en cada etapa para saber que porcentajes de progreso mostrar en cada comando (para el cmake-style)
	int steps_extras = active_configuration->extra_steps.GetSize(), 
		steps_objs = files.sources.GetSize(), 
		steps_libs = active_configuration->libs_to_build.GetSize();
	int steps_total = steps_extras+steps_objs+steps_libs+(active_configuration->dont_generate_exe?0:1);
	
	wxString old_temp_folder = active_configuration->temp_folder;
	if (mktype!=MKTYPE_FULL) active_configuration->temp_folder="${OBJS_DIR}";
	
	AnalizeConfig("",exec_comas,mingw_dir);
	if (executable_name.Contains(" "))
		executable_name=wxString("\"")+executable_name+"\"";
	
	wxString tab("\t");
	
	// abrir el archivo (crear o pisar)
	wxTextFile fil(make_file);
	if (fil.Exists())
		fil.Open();
	else
		fil.Create();
	fil.Clear();
	
	// definir las variables
	if (mktype==MKTYPE_CONFIG) fil.AddLine(wxString("OBJS_DIR=")+old_temp_folder);
	if (mktype!=MKTYPE_OBJS) {
		bool have_c_source=false;
		LocalListIterator<project_file_item*> item(&files.sources);
		while(!have_c_source && item.IsValid()) {
			have_c_source = !item->IsCppOrJustC();
			item.Next();
		}
		if (mingw_dir=="${MINGW_DIR}")
			fil.AddLine(wxString("MINGW_DIR=")+current_toolchain.mingw_dir);
		if (have_c_source) fil.AddLine(wxString("GCC=")+current_toolchain.c_compiler);
		fil.AddLine(wxString("GPP=")+current_toolchain.cpp_compiler);
		if (have_c_source) fil.AddLine(wxString("CFLAGS=")+c_compiling_options);
		fil.AddLine(wxString("CXXFLAGS=")+cpp_compiling_options);
		fil.AddLine(wxString("LDFLAGS=")+linking_options);
	}
	
	wxString libs_deps;
	for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next()) { 
		if (lib->objects_list.Len()) libs_deps<<" "<<lib->objects_list;
	}
	
	if (mktype!=MKTYPE_CONFIG) fil.AddLine(wxString("OBJS=")+exe_deps);
	if (libs_deps.Len()&&mktype!=MKTYPE_CONFIG) fil.AddLine(wxString("LIBS_OBJS=")+libs_deps);
	fil.AddLine("");
	
	if (mktype!=MKTYPE_OBJS) {
		// agregar las secciones all, clean, y la del ejecutable
		fil.AddLine(wxString("all: ")+temp_folder_short+" "+executable_name);
		if (cmake_style) fil.AddLine(tab+"@echo [100%] Built target "+executable_name);
		fil.AddLine("");
		
		if (mktype==MKTYPE_CONFIG) { 
			fil.AddLine("include Makefile.common");
			fil.AddLine("");
		}

		
		wxString extra_pre, extra_post;
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()){
			if (extra_step->out.Len() && extra_step->delete_on_clean) {
				if (extra_step->pos!=CES_AFTER_LINKING) {
					extra_pre<<" ";
					extra_pre<<extra_step->out;
				} else {
					extra_post<<" ";
					extra_post<<extra_step->out;
				}
			}
		}
		mxUT::ParameterReplace(extra_post,"${TEMP_DIR}",temp_folder_short);
		mxUT::ParameterReplace(extra_pre,"${TEMP_DIR}",temp_folder_short);
		mxUT::ParameterReplace(extra_post,"${PROJECT_PATH}",project->path);
		mxUT::ParameterReplace(extra_pre,"${PROJECT_PATH}",project->path);
		mxUT::ParameterReplace(extra_post,"${PROJECT_BIN}",executable_name);
		mxUT::ParameterReplace(extra_pre,"${PROJECT_BIN}",executable_name);
		
		fil.AddLine("clean:");
	#ifdef __WIN32__
		fil.AddLine(wxString("\tdel ${OBJS} ")+executable_name+extra_pre+extra_post+(libs_deps.Len()?" ${LIBS_OBJS}":""));
	#else
		fil.AddLine(wxString("\trm -rf ${OBJS} ")+executable_name+extra_pre+extra_post+(libs_deps.Len()?" ${LIBS_OBJS}":""));
	#endif
		fil.AddLine("");

//		compile_extra_step *estep=active_configuration->extra_steps;
//		wxString extra_deps;
//		while (estep) {
//			if (estep->out.Len()) {
//				extra_deps<<" ";
//				extra_deps<<estep->out;
//			}
//			estep=estep->next;
//		}
//		mxUT::ParameterReplace(extra_deps,"${TEMP_DIR}",temp_folder_short);
		
		fil.AddLine(executable_name+": ${OBJS}"/*+extra_deps*/+extra_pre);
		if (cmake_style) fil.AddLine(tab+"@echo "+get_percent(steps_objs+steps_extras,steps_total)+" Linking executable "+executable_name);
		fil.AddLine(tab+(cmake_style?"@":"")+"${GPP} ${OBJS} ${LDFLAGS} -o $@");
		fil.AddLine("");

		if (temp_folder_short.Len()!=0) {
			fil.AddLine(temp_folder_short+":");
			fil.AddLine("\tmkdir "+temp_folder_short);
			fil.AddLine("");
		}
	
		// agregar las secciones de los pasos extra 
		/// @todo: falta ver como forzar la ejecucion y el orden
		
		for(JavaVectorIterator<compile_extra_step> extra_step(active_configuration->extra_steps); extra_step.IsValid(); extra_step.Next()) {
			if (extra_step->out.Len()) {
				wxString output,deps,command;
				command = GetCustomStepCommand(extra_step,mingw_dir,deps,output);
				fil.AddLine(output+": "+deps);
				fil.AddLine("\t"+command);
				fil.AddLine("");
			}
		}
	
		int steps_current = steps_objs;
		// agregar las bibliotecas
		for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next()) {
			wxString libdep = mxUT::Quotize(lib->filename);
			wxString objs;
			libdep<<":";
			for(LocalListIterator<project_file_item*> item(&files.sources); item.IsValid();item.Next()) {
				if (item->GetLibrary()==lib)
					objs<<" "<<mxUT::Quotize(item->GetBinName(temp_folder_short));
			}
			fil.AddLine(libdep+objs);
			
			if (cmake_style) fil.AddLine(tab+"@echo "+get_percent(steps_current++,steps_total)+" Linking library "+libdep);
			fil.AddLine(tab+(cmake_style?"@":"")+(lib->is_static?current_toolchain.static_lib_linker:current_toolchain.dynamic_lib_linker)+" "+mxUT::Quotize(lib->filename)+objs+" "+lib->extra_link);
			fil.AddLine("");
		}
	}
	
	if (mktype!=MKTYPE_CONFIG) {
		// agregar las secciones de los objetos
		wxArrayString header_dirs_array;
		mxUT::Split(active_configuration->headers_dirs,header_dirs_array,true,false);
		for (unsigned int i=0;i<header_dirs_array.GetCount();i++) 
			header_dirs_array[i]=DIR_PLUS_FILE(path,header_dirs_array[i]);
		
		int steps_current=0;
		for( LocalListIterator<project_file_item*> item(&files.sources); item.IsValid(); item.Next() ) {
			wxString bin_full_path = mxUT::Quotize(item->GetBinName(temp_folder_short));
			fil.AddLine(bin_full_path+": "+mxUT::FindObjectDeps(item->GetFullPath(),path,header_dirs_array));
			bool cpp = item->IsCppOrJustC();
			
			if (cmake_style) fil.AddLine(tab+"@echo "+get_percent(steps_current++,steps_total)+" Building "+(cpp?"C++":"C")+" object "+bin_full_path);
			fil.AddLine(tab+(cmake_style?"@":"")+(cpp?"${GPP}":"${GCC}")+(cpp?" ${CXXFLAGS} ":" ${CFLAGS} ")+
	#ifndef __WIN32__
				(item->IsInALibrary()?"-fPIC ":"")+
	#endif
				"-c "+item->GetRelativePath()+" -o $@");
			fil.AddLine("");
		}
	}
	
	// guardar y cerrar
	fil.Write();
	fil.Close();
	
	active_configuration->temp_folder=old_temp_folder;
	
	config_analized = false; // avoid reusing this temporal and incomplete configuration
	
}

void ProjectManager::Clean() {
	
	if (current_toolchain.IsExtern()) {
		compile_and_run_struct_single *compile_and_run=new compile_and_run_struct_single("clean");
		compile_and_run->killed=true; // para que no lo procese el evento main_window->ProcessKilled
		compile_and_run->pid=CompileWithExternToolchain(compile_and_run,false);
		return;
	}
	
	// preparar las rutas y nombres adecuados
	AnalizeConfig(path,true,current_toolchain.mingw_dir);
	// borrar los objetos
	LocalListIterator<project_file_item*> item(&files.sources);
	while(item.IsValid()) {
		wxString bin_name = item->GetBinName(temp_folder);
		wxString gcov_file = DIR_PLUS_FILE(temp_folder,wxFileName(item->GetRelativePath()).GetName()+".gcno");
		if (wxFileName::FileExists(bin_name)) wxRemoveFile(bin_name);
		if (wxFileName::FileExists(gcov_file)) wxRemoveFile(gcov_file);
		item.Next();
	}
	// borrar las salidas de los pasos adicionales
	for(JavaVectorIterator<compile_extra_step> step(active_configuration->extra_steps); step.IsValid(); step.Next()) {
		if (step->delete_on_clean&&step->out.Len()){
			wxString out=step->out;
			mxUT::ParameterReplace(out,"${TEMP_DIR}",temp_folder);
			mxUT::ParameterReplace(out,"${PROJECT_BIN}",executable_name);
			mxUT::ParameterReplace(out,"${PROJECT_PATH}",path);
#ifdef __WIN32__
			mxUT::ParameterReplace(out,"${MINGW_DIR}",current_toolchain.mingw_dir);
#endif
			wxString file=DIR_PLUS_FILE(path,out);
			if (wxFileName::FileExists(file)) wxRemoveFile(file);
		}
	}
	// borrar temporales del icono
	if (wxFileName::FileExists(DIR_PLUS_FILE(temp_folder,"zpr_resource.rc")))
		wxRemoveFile(DIR_PLUS_FILE(temp_folder,"zpr_resource.rc"));
	if (wxFileName::FileExists(DIR_PLUS_FILE(temp_folder,"zpr_resource.o")))
		wxRemoveFile(DIR_PLUS_FILE(temp_folder,"zpr_resource.o"));

	// borrar las bibliotecas
	for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next()) { 
		wxString file=DIR_PLUS_FILE(path,lib->filename);
		if (wxFileName::FileExists(file)) wxRemoveFile(file);
		if (wxFileName::FileExists(file+".dbg")) wxRemoveFile(file+".dbg");
	}
	
	// borrar el ejecutable
	if (wxFileName::FileExists(executable_name)) wxRemoveFile(executable_name);
	if (wxFileName::FileExists(executable_name+".dbg")) wxRemoveFile(executable_name+".dbg");
}


/**
* Cuando se invoca al compilador se requiere una lista de argumentos que dependen
* directa o indirectamente de varios campos del cuadro de Opciones de Compilación 
* y Ejecución del Proyecto. No es cómodo convertir estos campos en argumentos
* para gcc cada vez que se los necesita, y además tampoco es eficiente, ya que por
* ejemplo puede requerir ejecutar comandos externos como pkg-config, y se los
* necesita al menos una vez por cada objeto a generar en el proyecto. Esta función
* se encarga de hacerlo una vez antes de compilar y guardar los resultados en los
* atributos de esta clase marcados como "temporales".
* @param path        es el directorio de base a utilizar para todas las rutas
*                    relativas. En la salida de esta función se utiliza este path
*                    para convertirlas en absolutas. Usualmente será la carpeta
*                    del proyecto, pero a la hora de generar el makefile se deja 
*                    en blanco, para que se mantengan relativas.
* @param exec_comas  indica si se deben reemplazar las cadenas entre acentos
*                    en los campos de argumentos adicionales o si se dejan 
*                    como estan (al compilar en zinjai se ejecutan, al generar
*                    el makefile suele ser convieniente dejarlos para que 
*                    los ejecute make).
* @param mingw_dir   indica con que valor se deben reemplazar las ocurrencias de
*                    la variable ${MINGW_DIR} en las opciones. Es el valor real
*                    al compilar en zinjai, pero conviene queda como variable
*                    al generar el makefile.
* @param force       hay un bool config_analized que indica que la configuración
*                    ya está actualizada, cuyo caso no hace nada. Este bool sirve
*                    para omitir esa comprobación y hacer el proceso obligatoriamente.
**/
void ProjectManager::AnalizeConfig(wxString path, bool exec_comas, wxString mingw_dir, bool force) { // parse active_configuration to generate the parts of the command line
	
	if (!force && config_analized) return;
	
	Toolchain::SelectToolchain();
	
	GetTempFolderEx(path,false);
	wxString compiling_options=" ";
	compiling_options<<current_toolchain.cpp_compiling_options<<" "<<current_toolchain.GetExtraCompilingArguments(true)<<" ";
	
	// debug_level
	wxString co_debug;
	if (active_configuration->debug_level==1) 
		co_debug<<config->Debug.format<<" -g1";
	else if (active_configuration->debug_level==2) 
		co_debug<<config->Debug.format<<" -g2";
	else if (active_configuration->debug_level==3) 
		co_debug<<config->Debug.format<<" -g3";
	compiling_options<<co_debug<<" ";
	
	// warnings_level
	wxString co_warnings;
	if (active_configuration->warnings_level==0) 
		co_warnings<<"-w";
	else if (active_configuration->warnings_level==2) 
		co_warnings<<"-Wall";
	else if (active_configuration->warnings_level==3) 
		co_warnings<<"-Wall -Wextra";
	if (active_configuration->warnings_as_errors)
		co_warnings<<" -Werror";
	compiling_options<<co_warnings<<" ";
	
	// optimization_level
	wxString co_optim;
	if (active_configuration->optimization_level==0) 
		co_optim<<"-O0";
	else if (active_configuration->optimization_level==1) 
		co_optim<<"-O1";
	else if (active_configuration->optimization_level==2) 
		co_optim<<"-O2";
	else if (active_configuration->optimization_level==3) 
		co_optim<<"-O3";
	else if (active_configuration->optimization_level==4) 
		co_optim<<"-Os";
	else if (active_configuration->optimization_level==5) 
		co_optim<<current_toolchain.FixArgument(true,"-Og");
	else if (active_configuration->optimization_level==6) 
		co_optim<<"-Ofast";
	if (active_configuration->enable_lto) co_optim<<" -flto";
	compiling_options<<co_optim<<" ";
	
	
	// headers_dirs
	wxString co_includes;
	co_includes<<mxUT::Split(active_configuration->headers_dirs,"-I");
	mxUT::ParameterReplace(co_includes,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(co_includes,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(co_includes,"${ZINJAI_DIR}",config->zinjai_dir);
	compiling_options<<co_includes<<" ";
	
	// parametros variables
	wxString co_extra = active_configuration->compiling_extra;
	mxUT::ParameterReplace(co_extra,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(co_extra,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(co_extra,"${ZINJAI_DIR}",config->zinjai_dir);
	// reemplazar subcomandos y agregar extras
	if (exec_comas) co_extra = mxUT::ExecComas(path, co_extra);
	
	compiling_options<<co_extra<<" ";
	
	
	// macros predefinidas
	wxString co_defines;
	if (active_configuration->macros.Len())
		co_defines<<mxUT::Split(active_configuration->macros,"-D");
	compiling_options<<co_defines;
		
	wxString co_std_c, co_std_cpp;
	// ansi_compliance
	if (active_configuration->pedantic_errors) { co_std_c<<"-pedantic-errors "; co_std_cpp<<"-pedantic-errors "; }
	if (active_configuration->std_c.Len()) co_std_c<<"-std="<<active_configuration->std_c<<" ";
	if (active_configuration->std_cpp.Len()) co_std_cpp<<current_toolchain.FixArgument(true,wxString("-std=")+active_configuration->std_cpp);
	
	c_compiling_options=compiling_options+co_std_c;
	cpp_compiling_options=compiling_options+co_std_cpp;
	
	linking_options=" ";
	linking_options<<current_toolchain.cpp_linker_options<<" ";
	if (active_configuration->enable_lto) {
		linking_options<<co_optim<<" ";
	}
	// mwindows
#ifdef __WIN32__
	if (!active_configuration->console_program)
		linking_options<<"-mwindows ";
#endif
	// strip
	if (active_configuration->strip_executable==DBSACTION_STRIP)
		linking_options<<"-s ";
	// directorios para bibliotecas
	linking_options<<mxUT::Split(active_configuration->libraries_dirs,"-L");
	// bibliotecas
	linking_options<<mxUT::Split(active_configuration->libraries,"-l");
	// reemplazar variables
	wxString linking_extra = active_configuration->linking_extra;
	mxUT::ParameterReplace(linking_extra,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(linking_extra,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(linking_options,"${MINGW_DIR}",mingw_dir);
	mxUT::ParameterReplace(linking_options,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(linking_options,"${ZINJAI_DIR}",config->zinjai_dir);
	// reemplazar subcomandos y agregar extras
	if (exec_comas)
		linking_options<<" "<<mxUT::ExecComas(path,linking_extra);
	else
		linking_options<<" "<<linking_extra;

	/*executable_name = */GetExePath(false,false,path); // GetExePath ya asigna en executable_name
	
	// bibliotecas
	for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next())
		lib->objects_list.Clear();
	
	AssociateLibsAndSources(active_configuration);
	
	objects_list="";
#ifdef __WIN32__
	if (
		( active_configuration->icon_file.Len() && wxFileName::FileExists(DIR_PLUS_FILE(path,active_configuration->icon_file)) )
		||
		( active_configuration->manifest_file.Len() && wxFileName::FileExists(DIR_PLUS_FILE(path,active_configuration->manifest_file)) ) 
		)
			objects_list<<mxUT::Quotize(DIR_PLUS_FILE(temp_folder,"zpr_resource.o"))<<" ";
#endif
	
	wxString extra_step_objs;
	for(JavaVectorIterator<compile_extra_step> step(active_configuration->extra_steps); step.IsValid(); step.Next()) {
		if (step->pos!=CES_AFTER_LINKING && step->out.Len() && step->link_output) extra_step_objs<<step->out<<" ";
	}
	if (extra_step_objs.Len()) {
		mxUT::ParameterReplace(extra_step_objs,"${TEMP_DIR}",temp_folder);
		mxUT::ParameterReplace(extra_step_objs,"${TEMP_DIR}",temp_folder);
		mxUT::ParameterReplace(extra_step_objs,"${PROJECT_PATH}",project->path);
		mxUT::ParameterReplace(extra_step_objs,"${PROJECT_PATH}",project->path);
		mxUT::ParameterReplace(extra_step_objs,"${PROJECT_BIN}",executable_name);
		mxUT::ParameterReplace(extra_step_objs,"${PROJECT_BIN}",executable_name);
		objects_list<<" "<<extra_step_objs;
	}
	
	LocalListIterator<project_file_item*> item(&files.sources);
	while(item.IsValid()) {
		wxString *olist = item->IsInALibrary()?&(item->GetLibrary()->objects_list):&objects_list;
		(*olist)<<mxUT::Quotize(item->GetBinName(temp_folder))<<" ";
		
		HashStringString::iterator it = active_configuration->by_src_compiling_options->find(item->GetRelativePath());
		if (it==active_configuration->by_src_compiling_options->end()) {
			item->compiling_options = item->IsCppOrJustC()?cpp_compiling_options:c_compiling_options;
		} else {
			wxString custom_options = it->second;
			mxUT::ParameterReplace(custom_options,"${MINGW_DIR}",mingw_dir,false);
			mxUT::ParameterReplace(custom_options,"${ZINJAI_DIR}",config->zinjai_dir,false);
			mxUT::ParameterReplace(custom_options,"${TEMP_DIR}",temp_folder_short,false);
			mxUT::ParameterReplace(custom_options,"${DBG}",co_debug,false);
			mxUT::ParameterReplace(custom_options,"${DEF}",co_defines,false);
			mxUT::ParameterReplace(custom_options,"${EXT}",co_extra,false);
			mxUT::ParameterReplace(custom_options,"${INC}",co_includes,false);
			mxUT::ParameterReplace(custom_options,"${OPT}",co_optim,false);
			mxUT::ParameterReplace(custom_options,"${STD}",item->IsCppOrJustC()?co_std_cpp:co_std_c,false);
			mxUT::ParameterReplace(custom_options,"${WAR}",co_warnings,false);
			mxUT::ParameterReplace(custom_options,"${ALL}",item->IsCppOrJustC()?cpp_compiling_options:c_compiling_options,false);
			item->compiling_options = custom_options;
		}
		
		item.Next();
	}
	
	exe_deps = objects_list;

	for(JavaVectorIterator<project_library> lib(active_configuration->libs_to_build);lib.IsValid();lib.Next()) {
		if (lib->objects_list.Len()) {
			lib->objects_list.RemoveLast();
			lib->parsed_extra = mxUT::ExecComas(path,lib->extra_link);
			mxUT::ParameterReplace(lib->parsed_extra,"${MINGW_DIR}",mingw_dir);
			mxUT::ParameterReplace(lib->parsed_extra,"${TEMP_DIR}",temp_folder_short);
			wxFileName bin_name = DIR_PLUS_FILE(path,lib->filename);
			wxString libfile = lib->is_static ? bin_name.GetFullPath():(wxString("-l")<<lib->libname);
			if (lib->do_link) objects_list<<mxUT::Quotize(libfile)<<" ";
			if (!lib->is_static) {
				bin_name.MakeRelativeTo(path);
				if (bin_name.GetPath().Len())
					linking_options<<" -L"<<mxUT::Quotize(bin_name.GetPath());
				else
					linking_options<<" -L./";
			}
			exe_deps<<mxUT::Quotize(lib->filename)<<" ";
		}
	}
	objects_list.RemoveLast();

	EnvVars::SetMode(EnvVars::COMPILING);
}


/**
* @brief Agrega los archivos del proyecto al arreglo array.
*
* El arreglo no se vacia antes de comenzar.
**/
int ProjectManager::GetFileList(wxArrayString &array, eFileType cuales, bool relative_paths) {
	int count=0;
	
	if (cuales==FT_NULL) {
		for( GlobalListIterator<project_file_item*> item(&files.all); item.IsValid(); item.Next()) {
			if (item->GetCategory()==FT_BLACKLIST) continue;
			array.Add( relative_paths ? item->GetRelativePath() : item->GetFullPath() );
			count++;
		}
	} else {
		LocalList<project_file_item*> &list = cuales==FT_SOURCE ? files.sources : (cuales==FT_HEADER ? files.headers : files.others);
		for( LocalListIterator<project_file_item*> item(&list); item.IsValid(); item.Next() ) {
			array.Add( relative_paths ? item->GetRelativePath(): item->GetFullPath() );
			count++;
		}		
	}
	return count;
}

bool ProjectManager::Debug() {
	if (active_configuration->dont_generate_exe) {
		mxMessageDialog(main_window,LANG(PROJMNGR_RUNNING_NO_EXE,""
										 "Este proyecto no puede ejecutarse porque está configurado\n"
										 "para generar solo bibliotecas."))
			.Title(LANG(GENERAL_WARNING,"Aviso")).IconWarning().Run();
		return false;
	}
	wxString command ( GetExePath(true) );
	wxString args = active_configuration->args;
	if (active_configuration->always_ask_args) {
		int res = mxArgumentsDialog(main_window,path,active_configuration->args,active_configuration->working_folder).ShowModal();
		if (res==0) return false;
		if (res&AD_ARGS) {
			active_configuration->args = mxArgumentsDialog::last_arguments;
			active_configuration->working_folder = mxArgumentsDialog::last_workdir;
			args=active_configuration->args;
		}
		if (res&AD_REMEMBER) {
			active_configuration->always_ask_args=false;
			if (res&AD_EMPTY) active_configuration->args="";
		}
	}
	wxString working_path = (active_configuration->working_folder=="")?path:DIR_PLUS_FILE(path,active_configuration->working_folder);
	if (working_path.Last()==path_sep) working_path.RemoveLast();
	
	EnvVars::SetMode(EnvVars::DEBUGGING);
	bool ret = debug->Start(working_path,command,args,active_configuration->console_program,active_configuration->wait_for_key);
	EnvVars::Reset();
	return ret;
}

/**
* Una vez inicializado gdb, esta función invoca a través de DebugManager los 
* comandos  necesarios para cargar todos los breakpoints definidos en todos los 
* archivos del proyecto. Setea además las propiedades de cada uno. Se debe 
* llamar antes de comenzar la ejecución.
**/
void ProjectManager::SetBreakpoints() {
	GlobalListIterator<BreakPointInfo*> bpi=BreakPointInfo::GetGlobalIterator();
	while (bpi.IsValid()) {
		bpi->UpdateLineNumber();
		debug->SetBreakPoint(*bpi);
		bpi.Next();
	}
}

bool ProjectManager::GenerateDoxyfile(wxString fname) {
	
	GetDoxygenConfiguration();

	wxTextFile fil(DIR_PLUS_FILE(path,fname));
	if (fil.Exists())
		fil.Open();
	else
		fil.Create();
	fil.Clear();
	
	fil.AddLine(wxString("PROJECT_NAME = ")<<doxygen->name);
	fil.AddLine(wxString("PROJECT_NUMBER = ")<<doxygen->version);
	fil.AddLine(wxString("OUTPUT_DIRECTORY = ")<<doxygen->destdir);
	if (doxygen->use_in_quickhelp)
		fil.AddLine(wxString("GENERATE_TAGFILE = ")<<DIR_PLUS_FILE(doxygen->destdir,"index-for-zinjai.tag"));
	fil.AddLine("INPUT_ENCODING = ISO-8859-15");
	fil.AddLine(wxString("OUTPUT_LANGUAGE = ")<<doxygen->lang);
	if (doxygen->base_path.Len()) fil.AddLine(wxString("STRIP_FROM_PATH = ")<<doxygen->base_path);
		
	if (doxygen->hideundocs) {
		fil.AddLine("EXTRACT_ALL = NO");
		fil.AddLine("HIDE_UNDOC_MEMBERS = YES");
		fil.AddLine("HIDE_UNDOC_CLASSES = YES");
	} else {
		fil.AddLine("EXTRACT_ALL = YES");
		fil.AddLine("HIDE_UNDOC_MEMBERS = NO");
		fil.AddLine("HIDE_UNDOC_CLASSES = NO");
		
	}
	
	wxArrayString input;
	
	if (doxygen->do_headers) {
		for( LocalListIterator<project_file_item*> item(&files.headers); item.IsValid(); item.Next() ) {
			input.Add(item->GetRelativePath());
		}
	}
	if (doxygen->do_cpps) {
		for( LocalListIterator<project_file_item*> item(&files.sources); item.IsValid(); item.Next() ) {
			input.Add(item->GetRelativePath());
		}
	}
	
	wxArrayString array;
	mxUT::Split(doxygen->extra_files,array,true,false);
	for (unsigned int i=0;i<array.GetCount();i++)
		input.Add(array[i]);
	
	array.Clear();
	mxUT::Split(doxygen->exclude_files,array,true,false);
	for (unsigned int i=0;i<array.GetCount();i++) {
		for (unsigned int j=0;i<input.GetCount();j++) {
#ifdef __WIN32__
			if (array[i].CmpNoCase(input[j])==0) 
#else
			if (array[i]==input[j]) 
#endif
			{
				input.RemoveAt(j);
				j--;
			}
		}
	}
	
	if (input.GetCount()) {
		fil.AddLine(wxString("INPUT = ")<<input[0]);
		for (unsigned int i=1;i<input.GetCount();i++)
			fil.AddLine(wxString("INPUT += \"")<<input[i]<<"\"");
	}
	
	array.Clear();
	
	fil.AddLine(wxString("GENERATE_LATEX = ")<<(doxygen->latex?"YES":"NO"));
	fil.AddLine(wxString("GENERATE_HTML = ")<<(doxygen->html?"YES":"NO"));
	fil.AddLine(wxString("GENERATE_TREEVIEW = ")<<(doxygen->html_navtree?"YES":"NO"));
	fil.AddLine(wxString("SEARCHENGINE = ")<<(doxygen->html_searchengine?"YES":"NO"));
	fil.AddLine("SEARCH_INCLUDES = YES");
	mxUT::Split(active_configuration->headers_dirs,array,true,false);
	if (array.GetCount()) {
		wxString defs("INCLUDE_PATH = ");
		for (unsigned int i=0;i<array.GetCount();i++)
			defs<<"\""<<array[i]<<"\" ";
		fil.AddLine(defs);
	}
	fil.AddLine(wxString("ENABLE_PREPROCESSING = ")<<(doxygen->preprocess?"YES":"NO"));
	fil.AddLine(wxString("EXTRACT_PRIVATE = ")<<(doxygen->extra_private?"YES":"NO"));
	fil.AddLine(wxString("EXTRACT_STATIC = ")<<(doxygen->extra_static?"YES":"NO"));
	if (doxygen->preprocess) {
		array.Clear();
		mxUT::Split(active_configuration->macros,array,true,false);
		if (array.GetCount()) {
			wxString defs("PREDEFINED = ");
			for (unsigned int i=0;i<array.GetCount();i++)
				defs<<"\""<<array[i]<<"\" ";
			fil.AddLine(defs);
		}
	}
	fil.AddLine("HTML_FILE_EXTENSION = .html");
	
	fil.AddLine(doxygen->extra_conf);
	
	fil.Write();
	fil.Close();
	return true;
}

wxString ProjectManager::GetExePath(bool short_path, bool refresh_temp_folder, wxString path) {
	executable_name=active_configuration->output_file; 
	executable_name.Replace("${TEMP_DIR}",refresh_temp_folder?GetTempFolder():temp_folder);
	if (path=="${PROJECT_DIR}") path = this->path;
	if (!path.IsEmpty()) executable_name = DIR_PLUS_FILE(path,executable_name);
	return executable_name = ( short_path
							 ? wxFileName(executable_name).GetShortPath()
							 : wxFileName(executable_name).GetFullPath() );
}

wxString ProjectManager::GetPath() {
	return path;
}

void ProjectManager::MoveFirst(wxTreeItemId &tree_item) {
	for(int i=0;i<files.sources.GetSize();i++) { 
		if (files.sources[i]->GetTreeItem()==tree_item) {
			files.sources.Swap(0,i);
			break;
		}
	}
}


wxString ProjectManager::WxfbGetSourceFile(wxString fbp_file) {
	wxString str;
	wxTextFile tf(fbp_file);
	tf.Open();
	int found=0;
	bool generates_code=true;
	wxString out_name,out_path;
	for (str = tf.GetFirstLine(); !tf.Eof(); str = tf.GetNextLine() ) {
		if (str.Contains("<property name=\"code_generation\">")) {
			generates_code = !str.Contains("XRC");
			found++;
		} else if (str.Contains("property name=\"path\"")) {
			out_path=str.AfterFirst('>').BeforeLast('<');
			found++;
		} else if (str.Contains("property name=\"file\"")) {
			out_name=str.AfterFirst('>').BeforeLast('<');
			found++;
		}
		if (found==3) break;
	}
	tf.Close();
	if (!generates_code) return "";
	wxFileName fn(fbp_file);
	wxString in_name = "noname";
	wxString in_path = fn.GetPathWithSep();
	if (out_name.Len()) in_name = out_name;
	if (out_path.Len()) in_name = DIR_PLUS_FILE(out_path,in_name);
	return DIR_PLUS_FILE(fn.GetPath(),in_name);
}

void ProjectManager::WxfbGetFiles() {
	wxfb->projects.Clear();
	wxfb->sources.Clear();
	LocalListIterator<project_file_item*> item(&files.others);
	while(item.IsValid()) { // por cada archivo .fbp (deberian estar en "otros")
		if (item->GetRelativePath().Right(4).Lower()==".fbp") {
			wxFileName fbp_file=item->GetFullPath();
			fbp_file.Normalize();
			wxfb->projects.Add(fbp_file.GetFullPath());
			wxFileName source_file=WxfbGetSourceFile(fbp_file.GetFullPath());
			source_file.Normalize();
			wxfb->sources.Add(source_file.GetFullPath());
		}
		item.Next();
	}
}

/**
* Ask wxformbuilder to regenerate one or all fbp projects (i.e. to create .cpp and .h or .xrc files)
*
* It is called automatically when ZinjaI gets the focus (for all wxfb projects, cual=nullptr), or 
* manually from tools menu (for a single wxfb project, specified in argument cual)
**/
bool ProjectManager::WxfbGenerate(bool show_osd, project_file_item *cual) {
	if (!config->CheckWxfbPresent()) {
		wxfb->autoupdate_projects_temp_disabled = true;
		return false;
	}
	boolFlagGuard wxfb_working_guard(wxfb->working);
	wxString old_compiler_tree_text = main_window->compiler_tree.treeCtrl->GetItemText(main_window->compiler_tree.state);
	mxOSDGuard osd;
	main_window->SetCompilingStatus(LANG(PROJMNGR_REGENERATING_WXFB,"Regenerando proyecto wxFormBuilder..."));
	
	bool something_changed=false;
	if (cual) {
		wxString fbp_file=cual->GetFullPath();
		wxString fbase = WxfbGetSourceFile(fbp_file);
		something_changed=WxfbGenerate(fbp_file,fbase,true,show_osd?&osd:nullptr);
	} else {
		for(int i=0;i<wxfb->projects.GetSize();i++) { 
			if (WxfbGenerate(wxfb->projects[i],wxfb->sources[i],false,show_osd?&osd:nullptr)) something_changed=true;
			if (!wxfb->autoupdate_projects) break;
		}
	}
	
	/// @todo: reemplazar estas lineas por SetCompilingStatus, pero ver antes en que contexto se llega aca para saber que puede haber habido en status
	main_window->compiler_tree.treeCtrl->SetItemText(main_window->compiler_tree.state,old_compiler_tree_text);
	main_window->SetStatusText(wxString(LANG(GENERAL_READY,"Listo")));
	return something_changed;
}

void ProjectManager::WxfbSetFileProperties(bool change_read_only, bool read_only_value, bool change_hide_symbols, bool hide_symbols_value) {
	for(int i=0;i<wxfb->sources.GetSize();i++) {
		wxString base = wxfb->sources[i];
		char exts[][5]={".cpp",".h",".xrc"};
		for(int j=0;j<3;j++) { 
			project_file_item *item = FindFromFullPath(base+exts[i]);
			if (!item) continue;
			if (change_read_only) SetFileReadOnly(item,read_only_value);
			if (change_hide_symbols) SetFileHideSymbols(item,hide_symbols_value);
		}
	}
}

bool ProjectManager::WxfbGenerate(wxString fbp_file, wxString fbase, bool force_regen, mxOSDGuard *osd) {
	// ver si hay que regenerar (comparando con fflag)
	wxString fflag = fbp_file.Mid(0,fbp_file.Len()-4)+".flg";
	wxDateTime dp = wxFileName(fbp_file).GetModificationTime();
	bool regen = force_regen || (!wxFileName::FileExists(fflag) || dp>wxFileName(fflag).GetModificationTime());
	if (!regen) return false;
	
	if (osd) osd->Create(main_window,LANG(PROJMNGR_REGENERATING_WXFB,"Regenerando proyecto wxFormBuilder..."));
	
	wxString command=mxUT::Quotize(config->Files.wxfb_command)+" -g "+mxUT::Quotize(fbp_file);
	_IF_DEBUGMODE("command: "<<command);
	int ret = mxExecute(command, wxEXEC_NODISABLE|wxEXEC_SYNC);
	_IF_DEBUGMODE("exit value: "<<ret);

	if (fbase.Len()) {
		if (ret) {
			if (osd) osd->Hide();
			wxString message = LANG(PROJMNGR_REGENERATING_ERROR_1,""
				 "No se pudieron actualizar correctamente los proyectos wxFormBuilder\n"
				 "(probablemente la ruta al ejecutable de wxFormBuilder no esté\n"
				 "correctamente definida. Verifique esta propiedad en la pestaña \"Rutas 2\"\n"
				 "del cuadro de \"Preferencias\").");
			if (wxfb->autoupdate_projects && !wxfb->autoupdate_projects_temp_disabled) {
				wxfb->autoupdate_projects_temp_disabled = true;
				message += "\n\n";
				message += LANG(PROJMNGR_REGENERATING_ERROR_2,""
								"La actualización automática de estos proyectos\nse deshabilitará temporalmente.");
			}
//			bool just_installed =	
				config->CheckComplaintAndInstall(main_window, "", LANG(GENERAL_WARNING,"Advertencia"), 
												 message, "wxformbuilder", "http://wxformbuilder.org","wxformbuilder");
//			mxMessageDialog(main_window,message).Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			return false;
		}
		
		if (wxFileName::FileExists(fbase+".cpp")) {
			mxSource *src = main_window->FindSource(fbase+".cpp");
			if (src) src->Reload();
			if (project->FindFromFullPath(fbase+".cpp")) parser->ParseFile(fbase+".cpp");
		}
		if (wxFileName::FileExists(fbase+".h")) {
			mxSource *src = main_window->FindSource(fbase+".h");
			if (src) src->Reload();
			if (project->FindFromFullPath(fbase+".h")) parser->ParseFile(fbase+".h");
		}
		
	} else {
		wxString fxrc=fbase+".xrc";
		mxSource *src=main_window->FindSource(fxrc);
		if (src) src->Reload();
	}
	
	wxTextFile fil(fflag);
	if (!fil.Exists()) fil.Create();
	fil.Open();
	if (!fil.IsOpened()) {
		if (osd) osd->Hide();
		wxString message = LANG(PROJMNGR_REGENERATING_ERROR_4,""
								"No se pudieron actualizar correctamente los proyectos wxFormBuilder\n"
								"(probablemente no se puede escribir en la carpeta de proyecto).");
		if (wxfb->autoupdate_projects && !wxfb->autoupdate_projects_temp_disabled) {
			wxfb->autoupdate_projects_temp_disabled = true;
			message += "\n\n";
			message += LANG(PROJMNGR_REGENERATING_ERROR_2,""
							"La actualización automática de estos proyectos\nse deshabilitará temporalmente.");
		}
		mxMessageDialog(main_window,message).Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
		return true;
	}
	fil.Clear();
	fil.AddLine("Este archivo se utiliza para determinar la fecha y hora de la última compilación del proyecto wxFormBuilder homónimo.");
	fil.AddLine("This is a dummy file to be used as timestamp for the generation of a wxFormBuilder project.");
	fil.Write();
	fil.Close();

	return true;
}

compile_extra_step *ProjectManager::InsertExtraSteps(project_configuration *conf, wxString name, wxString cmd, int pos) {
	modified=true;
	if (!conf) conf=active_configuration;
	compile_extra_step *step = new compile_extra_step;
	step->command=cmd;
	step->name=name;
	step->pos=pos;
	conf->extra_steps.Add(step);
	return step;
}

void ProjectManager::MoveExtraSteps(project_configuration *conf, compile_extra_step *step, bool up) {
	if (!conf) conf = active_configuration;
	int idx = conf->extra_steps.FindPtr(step);
	if (up) {
		if (idx==0 || conf->extra_steps[idx-1]->pos!=step->pos) {
			EXPECT_OR( step->pos!=CES_BEFORE_SOURCES, return );
			if (step->pos==CES_BEFORE_EXECUTABLE) {
//				if (conf->libs_to_build.GetSize())
					step->pos=CES_BEFORE_LIBS;
//				else
//					step->pos=CES_BEFORE_SOURCES;
			} else if (step->pos==CES_BEFORE_LIBS) {
				step->pos=CES_BEFORE_SOURCES;
			} else if (step->pos==CES_AFTER_LINKING)
				step->pos=CES_BEFORE_EXECUTABLE;
		} else if (step->prev) {
			EXPECT_OR( idx>0, return );
			conf->extra_steps.Swap(idx,idx-1);
		}
	} else {
		if (idx==conf->extra_steps.GetSize()-1 || conf->extra_steps[idx+1]->pos!=step->pos) {
			EXPECT_OR( step->pos!=CES_AFTER_LINKING, return );
			if (step->pos==CES_BEFORE_EXECUTABLE)
				step->pos=CES_AFTER_LINKING;
			else if (step->pos==CES_BEFORE_SOURCES) {
//				if (conf->libs_to_build.GetSize())
					step->pos=CES_BEFORE_LIBS;
//				else
//					step->pos=CES_BEFORE_EXECUTABLE;
			} else if (step->pos==CES_BEFORE_LIBS) {
//				if (conf->libs_to_build.GetSize())
					step->pos=CES_BEFORE_EXECUTABLE;
//				else
//					step->pos=CES_AFTER_LINKING;
			}
		} else if (step->next) {
			EXPECT_OR( idx+1<conf->extra_steps.GetSize(), return );
			conf->extra_steps.Swap(idx,idx+1);
		}
	}
	modified=true;
}

bool ProjectManager::DeleteExtraStep(project_configuration *conf, compile_extra_step *step) {
	if (!conf) conf = active_configuration;
	if (!step) return false;
	int idx = conf->extra_steps.FindPtr(step);
	if (idx==conf->extra_steps.NotFound()) return false;
	modified = true;
	conf->extra_steps.Remove(idx);
	return true;
}

/** 
* Busca información correspondiente a un paso de compilación personalizado. La busqueda se hace
* por nombre.
* @param conf configuración en la cual buscar; si recibe nullptr utiliza active_configuration
* @param name nombre del paso a buscar, case sensitive
* @return puntero al paso, si es que existe, sino nullptr
**/

compile_extra_step *ProjectManager::GetExtraStep(project_configuration *conf, wxString name) {
	if (!conf) conf=active_configuration;
	for(JavaVectorIterator<compile_extra_step> step(conf->extra_steps);step.IsValid();step.Next()) {
		if (step->name==name) return step;
	}
	return nullptr;
}

long int ProjectManager::CompileExtra(compile_and_run_struct_single *compile_and_run, compile_extra_step *step) {
	wxString command = GetCustomStepCommand(step);
	compile_and_run->compiling=true; compile_and_run->linking=false;
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	compile_and_run->output_type=MXC_EXTRA;
	compile_and_run->step_label=step->name;
	compile_and_run->m_cem_state = errors_manager->InitOtherStep(command);
	return mxUT::Execute(path,command, wxEXEC_ASYNC|(step->hide_window?0:wxEXEC_NOHIDE),compile_and_run->process);
}

long int ProjectManager::CompileWithExternToolchain(compile_and_run_struct_single *compile_and_run, bool build) {
	
	wxString command = build?current_toolchain.build_command:current_toolchain.clean_command;
	for(int i=0;i<TOOLCHAIN_MAX_ARGS;i++) 
		command.Replace(wxString("${ARG")<<i+1<<"}",current_toolchain.arguments[i][1],true);
	
#ifdef __WIN32__
	mxUT::ParameterReplace(command,"${MINGW_DIR}",current_toolchain.mingw_dir);
#endif
	mxUT::ParameterReplace(command,"${PROJECT_PATH}",project->path);
	mxUT::ParameterReplace(command,"${PROJECT_BIN}",project->executable_name);
	mxUT::ParameterReplace(command,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(command,"${NUM_PROCS}",wxString()<<config->Init.max_jobs);
	
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	
	// ejecutar
	compile_and_run->output_type=MXC_EXTERN;
	compile_and_run->compiling=true;
	compile_and_run->m_cem_state = errors_manager->InitExtern(command);
	main_window->extern_compiler_output->AddLine("> ",command);
	return mxUT::Execute(path,command, wxEXEC_ASYNC/*|(step->hide_window?0:wxEXEC_NOHIDE)*/,compile_and_run->process);
}


/** 
* Determina si se debe realizar o no un paso extra de compilación de acuerdo a
* sus dependencias. Si el paso no tiene definido el archivo de salida, se realiza siempre.
* Si tiene definido el archivo de salida, pero no las dependencias, se realiza solo si
* el archivo de salida no existe. Si está completamente definido, se realiza si alguno
* de los archivos de la lista de dependencias tiene fecha de modificación mayor a la del
* archivo de salida. Se la invoca desde PrepareForBuilding.
* @param step puntero a la estructura que contiene la información del paso extra por el que se pregunta
* @retval true si es necesario ejecutar este paso
* @retval false si es no necesario ejecutar este paso
**/
bool ProjectManager::ShouldDoExtraStep(compile_extra_step *step) {
	if (step->out.Len()==0) return true;
	wxString str_out(step->out);
	mxUT::ParameterReplace(str_out,"${TEMP_DIR}",temp_folder_short);
	mxUT::ParameterReplace(str_out,"${PROJECT_BIN}",executable_name);
	mxUT::ParameterReplace(str_out,"${PROJECT_PATH}",path);
	mxUT::ParameterReplace(str_out,"${MINGW_DIR}",current_toolchain.mingw_dir);
	str_out=DIR_PLUS_FILE(path,str_out);
	wxFileName fn_out(str_out);
	if (!fn_out.FileExists()) return true;
	wxDateTime dt_out = fn_out.GetModificationTime();
	wxArrayString array;
	mxUT::Split(step->deps,array,true,false);
	for (unsigned int i=0;i<array.GetCount();i++) {
		wxString str_dep(array[i]);
		mxUT::ParameterReplace(str_dep,"${TEMP_DIR}",temp_folder_short);
		mxUT::ParameterReplace(str_dep,"${PROJECT_BIN}",executable_name);
		mxUT::ParameterReplace(str_dep,"${PROJECT_PATH}",path);
		mxUT::ParameterReplace(str_dep,"${MINGW_DIR}",current_toolchain.mingw_dir);
		str_dep = DIR_PLUS_FILE(path,str_dep);
		wxFileName fn_dep = wxFileName(str_dep);
		if (fn_dep.FileExists() && fn_dep.GetModificationTime()>dt_out)
			return true;
	}
	return false;
}

/**
* Cambia algunos parametros del proyecto para corregir nombres del template con
* el nombre de proyecto ingresado por el usuario. Se llama desde el asistente de
* nuevo proyecto, luego de crearlo y abrirlo.
*
* @param name nombre del proyecto (nombre del archivo sin ruta ni extensión)
**/
void ProjectManager::FixTemplateData(wxString name) {
	project_name=name;
	for (int i=0;i<configurations_count;i++) {
		mxUT::ParameterReplace(configurations[i]->output_file,"${FILENAME}",name);
		for(JavaVectorIterator<project_library> lib(configurations[i]->libs_to_build);lib.IsValid();lib.Next()) { 
			mxUT::ParameterReplace(lib->libname,"${FILENAME}",name);
		}
	}
	main_window->SetOpenedFileName(project_name=name);
}

void ProjectManager::ActivateWxfb(bool do_activate) {
	GetWxfbConfiguration()->activate_integration=do_activate;
	_menu_item(mxID_TOOLS_WXFB_REGEN)->Enable(do_activate);
	_menu_item(mxID_TOOLS_WXFB_INHERIT_CLASS)->Enable(do_activate);
	_menu_item(mxID_TOOLS_WXFB_UPDATE_INHERIT)->Enable(do_activate);
	menu_data->UpdateToolbar(MenusAndToolsConfig::tbPROJECT,true);
	if (do_activate) {
		WxfbGetFiles();
		if (!config->CheckWxfbPresent()) {
			GetWxfbConfiguration()->autoupdate_projects_temp_disabled = true;
		}
	}
	main_window->m_aui->Update(); // para que se de cuenta de el cambio en la barra de herramientas
}

long int ProjectManager::CompileIcon(compile_and_run_struct_single *compile_and_run, wxString icon_name) {
	// preparar la linea de comando 
	wxString command = "windres ";
	command<<" -i \""<<wxFileName(DIR_PLUS_FILE(temp_folder,"zpr_resource.rc")).GetShortPath()<<"\"";
	command<<" -o \""<<wxFileName(DIR_PLUS_FILE(temp_folder,"zpr_resource.o")).GetShortPath()<<"\"";
	compile_and_run->process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	// ejecutar
	compile_and_run->output_type=MXC_SIMIL_GCC;
	compile_and_run->m_cem_state = errors_manager->InitOtherStep(command);
	return mxUT::Execute(path,command, wxEXEC_ASYNC,compile_and_run->process);
}

int ProjectManager::GetRequiredVersion() {
	
	bool have_macros=false,have_icon=false,have_temp_dir=false,builds_libs=false,have_extra_vars=false,
		 have_manifest=false,have_std=false,use_og=false,use_exec_script=false,have_custom_tools=false,
		 have_env_vars=false,have_breakpoint_annotation=false,env_vars_autoref=false,exe_use_temp=false,
		 copy_debug_symbols=false, use_ofast=false, exec_wrapper=false, by_src_args=false, use_lto_or_werror=false,
		 libs_dont_link=false;
	
	// breakpoint options
	GlobalListIterator<BreakPointInfo*> bpi=BreakPointInfo::GetGlobalIterator();
	while (bpi.IsValid()) {
		if (bpi->annotation.Len()) have_breakpoint_annotation=true;
		bpi.Next();
	}
	
	// compiling and running options
	for (int i=0;i<configurations_count;i++) {
		if (configurations[i]->enable_lto || configurations[i]->warnings_as_errors || configurations[i]->warnings_level==3) use_lto_or_werror=true;
		if (configurations[i]->exec_method==EMETHOD_WRAPPER) exec_wrapper=true;
		if (configurations[i]->strip_executable==DBSACTION_COPY) copy_debug_symbols=true;
		if (configurations[i]->exec_method!=0) use_exec_script=true;
		if (configurations[i]->output_file.Contains("${TEMP_DIR}")) exe_use_temp=true;
		if (configurations[i]->optimization_level==5) use_og=true;
		else if (configurations[i]->optimization_level==6) use_ofast=true;
		if (!configurations[i]->std_c.Len()) have_std=true;
		if (!configurations[i]->std_cpp.Len()) have_std=true;
		if (configurations[i]->pedantic_errors) have_std=true;
		if (configurations[i]->compiling_extra.Contains("${TEMP_DIR}")) have_temp_dir=true;
		if (configurations[i]->linking_extra.Contains("${TEMP_DIR}")) have_temp_dir=true;
		if (configurations[i]->macros.Len()) have_macros=true;
		if (configurations[i]->manifest_file.Len()) have_manifest=true;
		if (configurations[i]->icon_file.Len()) have_icon=true;
		if (configurations[i]->libs_to_build.GetSize()) builds_libs=true;
		if (configurations[i]->env_vars.Len()) {
			have_env_vars=true;
			if (configurations[i]->env_vars.Contains("${")) env_vars_autoref=true;
		}
		for(JavaVectorIterator<project_library> lib(configurations[i]->libs_to_build);lib.IsValid();lib.Next()) { 
			if (!lib->do_link) libs_dont_link=true;
		}
		for(JavaVectorIterator<compile_extra_step> step(configurations[i]->extra_steps); step.IsValid(); step.Next()) {
			if (step->deps.Contains("${TEMP_DIR}")) have_temp_dir=true;
			if (step->out.Contains("${TEMP_DIR}")) have_temp_dir=true;
			wxString sum=step->out+step->deps+step->command;
			if (sum.Contains("${PROJECT_PATH}")) have_extra_vars=true;
			if (sum.Contains("${PROJECT_BIN}")) have_extra_vars=true;
			if (sum.Contains("${MINGW_DIR}")) have_extra_vars=true;
			if (sum.Contains("${DEPS}")) have_extra_vars=true;
			if (sum.Contains("${OUTPUT}")) have_extra_vars=true;
			if (!step->delete_on_clean) have_extra_vars=true;
		}
		
		if (!configurations[i]->by_src_compiling_options->empty()) by_src_args=true;
	}
	// project's custom tools
	for (int i=0;i<MAX_PROJECT_CUSTOM_TOOLS;i++) if (custom_tools[i].command.Len()) have_custom_tools=true;
	
	version_required=0;
	if (!inherits_from.IsEmpty()) version_required=20170726;
	else if (libs_dont_link) version_required=20160711;
	else if (use_lto_or_werror) version_required=20160225;
	else if (inspection_improving_template_to.GetCount()) version_required=20150227;
	else if (by_src_args) version_required=20150220;
	else if (exec_wrapper) version_required=20141218;
	else if (use_ofast) version_required=20140507;
	else if (copy_debug_symbols) version_required=20140410;
	else if (env_vars_autoref || exe_use_temp) version_required=20140318;
	else if (have_breakpoint_annotation) version_required=20140213;
	else if (wxfb && wxfb->activate_integration) version_required=20131219;
	else if (have_env_vars) version_required=20131122;
	else if (have_custom_tools) version_required=20131115;
	else if (use_exec_script) version_required=20130817;
	else if (use_og || have_std) version_required=20130729;
	else if (autocodes_file.Len()) version_required=20110814;
	else if (have_manifest) version_required=20110610;
	else if (have_extra_vars) version_required=20110401;
	else if (builds_libs) version_required=20100503;
	else if (doxygen && doxygen->base_path.Len()) version_required=20091127;
	else if (have_temp_dir) version_required=20091119;
	else if (have_icon) version_required=20091117;
	else if (macros_file.Len()) version_required=20091026;
	else if (have_macros) version_required=20090903;
	return version_required;
}

void ProjectManager::UpdateSymbols() {
	for(int i=0;i<2;i++) {
		LocalListIterator<project_file_item*> item(i==0?&files.sources:&files.headers);
		while(item.IsValid()) {
			wxString name = item->GetFullPath();
			mxSource *source=main_window->IsOpen(name);
			if (source && source->GetModify()) {
				parser->ParseSource(source);
			} else {
				parser->ParseIfUpdated(name);
			}
			item.Next();
		}
	}
}


project_library *ProjectManager::AppendLibToBuild(project_configuration *conf) {
	project_library *new_lib = new project_library();
	conf->libs_to_build.Add(new_lib);
	return new_lib;
}

/** @brief Elimina una biblioteca a construir de una configuración **/
bool ProjectManager::DeleteLibToBuild(project_configuration *conf, project_library *lib_to_del) {
	int pos = conf->libs_to_build.FindPtr(lib_to_del);
	if (pos!=conf->libs_to_build.NotFound()) {
		conf->libs_to_build.Remove(pos);
		return true;
	} else {
		return false;
	}
}

/** @brief Elimina una biblioteca a construir de una configuración **/
project_library *ProjectManager::GetLibToBuild(project_configuration *conf, wxString libname) {
	for(JavaVectorIterator<project_library> lib(conf->libs_to_build); lib.IsValid(); lib.Next())
		if (lib->libname==libname) return lib;
	return nullptr;
}

void ProjectManager::SaveLibsAndSourcesAssociation(project_configuration *conf) {
	for(JavaVectorIterator<project_library> lib(conf->libs_to_build); lib.IsValid(); lib.Next()) { 
		lib->sources.Clear();
	}
	for (LocalListIterator<project_file_item*> fi(&files.sources); fi.IsValid(); fi.Next()) {
		if (fi->IsInALibrary()) {
			if (fi->GetLibrary()->sources.Len()) fi->GetLibrary()->sources<<" ";
			fi->GetLibrary()->sources << mxUT::Quotize( fi->GetRelativePath() );
		}
	}
}

// parte de la PrepareForBuilding
void ProjectManager::AssociateLibsAndSources(project_configuration *conf) {
	if (!conf) conf=active_configuration;
	for (LocalListIterator<project_file_item*> fi(&files.sources); fi.IsValid(); fi.Next()) { fi->UnsetLibrary(); }
	wxArrayString srcs;
	for(JavaVectorIterator<project_library> lib(conf->libs_to_build); lib.IsValid(); lib.Next()) {
		srcs.Clear();
		mxUT::Split(lib->sources,srcs);
		for (unsigned int i=0;i<srcs.GetCount();i++) {
			for( LocalListIterator<project_file_item*> fi(&files.sources); fi.IsValid(); fi.Next() ) {
				if (!fi->IsInALibrary() && fi->GetRelativePath()==srcs[i]) {
					fi->SetLibrary(lib);
					break;
				}
			}
		}
		// armar tambien el nombre del archivo
		lib->filename = DIR_PLUS_FILE(lib->GetPath(this),wxString("lib")<<lib->libname);
#ifdef __WIN32__
		if (lib->is_static)
			lib->filename<<".a";
		else
			lib->filename<<".dll";
#else
		if (lib->is_static)
			lib->filename<<".a";
		else
			lib->filename<<".so";
#endif
	}
}

struct draw_graph_item {
	wxString name;
	wxFileName fullpath;
	unsigned long sz,sy,lc;
	bool operator<(const draw_graph_item &gi) const {
		return lc<gi.lc;
	}
};

void ProjectManager::DrawGraph() {
	
	unsigned long Ml=0, ml=0, mm=0; // maxima, minima, mediana
	mxOSDGuard osd(main_window,LANG(OSD_GENERATING_GRAPH,"Generando grafo..."));
	
	wxArrayString header_dirs_array;
	mxUT::Split(active_configuration->headers_dirs,header_dirs_array,true,false);

	wxString graph_file=DIR_PLUS_FILE(config->temp_dir,"graph.dot");
	wxTextFile fil(graph_file);
	
	int c=files.sources.GetSize()+files.headers.GetSize();
	
	draw_graph_item *dgi = new draw_graph_item[c];
	c=0;
	wxString tab("\t");
	
	if (fil.Exists())
		fil.Open();
	else
		fil.Create();
	fil.Clear();
	fil.AddLine("digraph h {");
	fil.AddLine("\tsplines=true;");
	
	for(int i=0;i<2;i++) { 
		LocalListIterator<project_file_item*> fi(i==0?&files.headers:&files.sources);
		while(fi.IsValid()) {
			dgi[c].name = fi->GetRelativePath();
			wxString fullname = fi->GetFullPath();
			dgi[c].fullpath = fullname;
			dgi[c].sz=wxFileName::GetSize(fullname).ToULong();
			if (dgi[c].sz==wxInvalidSize) dgi[c].sz=0;
			wxTextFile tf(fullname);
			if (tf.Open()) {
				dgi[c].lc=tf.GetLineCount();
				tf.Close();
				if (c) {
					if (dgi[c].lc>Ml) Ml=dgi[c].lc;
					if (dgi[c].lc<ml) ml=dgi[c].lc;
				} else
					ml=Ml=dgi[c].lc;
			} else dgi[c].lc=0;
			dgi[c].sy=0;
			pd_file *pd = parser->GetFile(fullname);
			pd_ref *ref;
			if (pd) {
				ref=pd->first_class;
				ML_ITERATE(ref) dgi[c].sy++;
				ref=pd->first_func_dec;
				ML_ITERATE(ref) dgi[c].sy++;
				ref=pd->first_func_def;
				ML_ITERATE(ref) if (PD_UNREF(pd_func,ref)->file_dec!=pd) dgi[c].sy++;
				ref=pd->first_attrib;
				ML_ITERATE(ref) dgi[c].sy++;
				ref=pd->first_macro;
				ML_ITERATE(ref) dgi[c].sy++;
			}
			c++;
			fi.Next();
		}
	}
	
	sort(dgi,dgi+c);
	mm=dgi[c/2].lc;
	if (Ml==mm || mm==ml) mm=(ml+Ml)/2;
	
	for (int i=0;i<c;i++) {
		wxString line("\t");
		unsigned long x,lc=dgi[i].lc;
		wxColour col;
		if (ml==Ml) {
			col=wxColour(128,255,128);
		} else if (lc<mm) {
			x=(dgi[i].lc-ml)*100/(mm-ml);
			col=wxColour(156,155+x,255-x);
		} else {
			x=(dgi[i].lc-mm)*100/(Ml-mm);
			col=wxColour(155+x,255-x,156);
		}
		line<<"\""<<dgi[i].name<<"\""
			<<"[shape=note,style=filled,fillcolor=\""
			<<col.GetAsString(wxC2S_HTML_SYNTAX)<<"\",label=\""
			<<dgi[i].name
			<<"  \\l"<<dgi[i].sz<<LANG(PROJMNGR_PROJECT_GRAPH_BYTES," bytes")
			<<"\\r"<<dgi[i].lc<<LANG(PROJMNGR_PROJECT_GRAPH_LINES," linea(s)")
			<<"\\r"<<dgi[i].sy<<LANG(PROJMNGR_PROJECT_GRAPH_SYMBOLS," simbolo(s)")
			<<"\\r\"];";
		fil.AddLine(line);
	}
	
	wxArrayString deps;
	for (int i=0;i<c;i++) {
		mxUT::FindIncludes(deps,dgi[i].fullpath,path,header_dirs_array);
		for (unsigned int j=0;j<deps.GetCount();j++) {
			wxString header=DIR_PLUS_FILE(path,deps[j]);
			for (int k=0;k<c;k++) {
				if (dgi[k].fullpath==header) {
					fil.AddLine(tab+"\""+dgi[i].name+"\"->\""+dgi[k].name+"\";");
					break;
				}
			}
		}
	}
	
	fil.AddLine("}");
	fil.Write();
	fil.Close();
	
	delete [] dgi;
	osd.Hide(); // ProcessGraph will show its own one
	mxUT::ProcessGraph(graph_file,true,"",LANG(PROJMNGR_PROJECT_GRAPH_TITLE,"Grafo de Proyecto"));

}



/// Funcion auxiliar para ProjectManager::WxfbNewClass y ProjectManager::WxfbUpdateClass
void static GetFatherMethods(wxString base_header, wxString class_name, wxArrayString &methods) {
	// encontrar los metodos a heredar
	wxTextFile btf(base_header);
	btf.Open();
	wxString str, class_tag = wxString("class ")+class_name;
	bool on_the_class=false;
	for ( str = btf.GetFirstLine(); !btf.Eof(); str = btf.GetNextLine() ) {
		if (str.Contains("class ") && mxUT::LeftTrim(str.Mid(0,2))!="//") {
			if (str.Contains(class_tag)) {
				on_the_class=true;
			} else if (on_the_class) 
				break;
		} else if (on_the_class) {
			if (str.Contains("virtual ") && mxUT::LeftTrim(str.Mid(0,2))!="//")
				methods.Add(mxUT::LeftTrim(str.BeforeFirst('{')).Mid(8));
		}
	}
	btf.Close();
}

bool ProjectManager::WxfbUpdateClass(wxString wxfb_class, wxString user_class) {
	pd_class *pdc_son = parser->GetClass(user_class);
	wxString cfile = mxUT::GetComplementaryFile(pdc_son->file->name,FT_HEADER);
	if (cfile.Len()==0) { // si tampoco hay "public:", no hay caso
		mxMessageDialog(main_window,"No se pudo determinar donde definir los nuevos metodos.\nNo se encontró el archivo fuente complementario.")
			.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
		return false;
	}
	wxTextFile fil(pdc_son->file->name);
	fil.Open();
	bool add_visibility=false;
	int lines=fil.GetLineCount(),curpos=pdc_son->line,inspos=-1, pubpos=-1; // linea en la cual agregar los metodos
	wxString str;
	wxString tabs_pro, tabs_pub;
	while (curpos<lines) {
		str = fil.GetLine(curpos);
		int i=0; while (str[i]==' '||str[i]=='\t') i++;
		if (str.Len()-i>=10 && str.Mid(i,10)=="protected:") {
			inspos=curpos;
			tabs_pro=str.Mid(0,i);
			break;
		} else
			if (str.Len()-i>=7 && str.Mid(i,7)=="public:") {
				tabs_pub=str.Mid(0,i);
				pubpos=curpos;
			} else
				if (str.Len()-i>6 && str.Mid(i,6)=="class ") break;
		curpos++;
	}
	if (inspos==-1) { // si no hay "protected:", se agrega antes de "public:"
		if (pubpos==-1) { // si tampoco hay "public:", no hay caso
			mxMessageDialog(main_window,"No se pudo determinar donde declarar los nuevos metodos.\nNo se encontraron la etiquetas public/protected en la clase.")
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			return false;
		}
		inspos = pubpos;
		tabs_pro = tabs_pub;
		add_visibility=true;
	} else inspos++; // linea siguiente a "protected:"
	if (add_visibility) {
		fil.InsertLine(tabs_pro+"protected:",inspos++);
		fil.InsertLine(tabs_pro,inspos);
	}
	
	tabs_pro+="\t";
	
	pd_class *pdc_father = parser->GetClass(wxfb_class);
	wxArrayString methods;
	GetFatherMethods(pdc_father->file->name,wxfb_class,methods);
	
	bool modified=false;
	
	wxTextFile fil2(cfile);
	fil2.Open();
	if (fil2.GetLastLine().Len())
		fil2.AddLine("");
	
	for (unsigned int i=0;i<methods.GetCount();i++) {
		wxString mname = methods[i].BeforeFirst('(');
		while (mname.Last()==' ') mname.RemoveLast();
		mname = mname.AfterLast(' ');
		pd_func *pdm_son = pdc_son->first_method;
		bool found=false;
		ML_ITERATE(pdm_son)	if (pdm_son->name==mname) { found=true; break; }
		if (!found) {
			modified=true;
			fil.InsertLine(tabs_pro+methods[i]+";",inspos++);
			fil2.AddLine(methods[i].BeforeFirst(' ')+" "+user_class+"::"+methods[i].AfterFirst(' ')+" {");
			fil2.AddLine("\tevent.Skip();");
			fil2.AddLine("}");
			fil2.AddLine("");
		}
	}
	
	if (modified) {
		fil.Write();
		fil.Close();
		
		fil2.Write();
		fil2.Close();
		
		mxSource *src_h=main_window->IsOpen(pdc_son->file->name);
		mxSource *src_c=main_window->IsOpen(cfile);
		if (src_h) {
			src_h->Reload();
			parser->ParseSource(src_h,true);
		} else {
			parser->ParseFile(pdc_son->file->name);
		}
		if (src_c) {
			src_c->Reload();
			parser->ParseSource(src_c,true);
		} else {
			parser->ParseFile(cfile);
		}
	} else {
		fil.Close();
		fil2.Close();
	}
	
	return true;
}

ProjectManager::WxfbAutoCheckData::WxfbAutoCheckData() {
	for (int i=0; i<project->wxfb->sources.GetSize();i++) {
		pd_file *pdf=parser->GetFile(project->wxfb->sources[i]+".h");
		if (pdf) {
			pd_ref *cls_ref = pdf->first_class;
			ML_ITERATE(cls_ref)
				wxfb_classes.Add(PD_UNREF(pd_class,cls_ref)->name);
		}
	}
	pd_inherit *item=parser->first_inherit;
	while (item->next) {
		item=item->next;
		if (wxfb_classes.Contains(item->father)) {
			user_fathers.Add(item->father);
			user_classes.Add(item->son);
		}
	}
}

/**
* Parte 1, se llama al recibir el foco en la ventana principal. Primero mira si hay cambios
* en los proyectos cd wxfb comparando los archivos de proyecto con los archivos flags que 
* genera ZinjaI, y si los hay hace regererar el código, manda a parsear los nuevos fuentes
* y programa el paso 2 para después del parseo.
**/
void ProjectManager::WxfbAutoCheckStep1() {
	
	if (loading || !wxfb || !wxfb->activate_integration || !wxfb->autoupdate_projects || wxfb->autoupdate_projects_temp_disabled || wxfb->working) return;
	
	if (parser->working) {
		class LaunchProjectWxfbAutoUpdateStep1Action : public Parser::OnEndAction {
		public: void Run() override { if (project) project->WxfbAutoCheckStep1(); }
		};
		parser->OnEnd(new LaunchProjectWxfbAutoUpdateStep1Action());
		return;
	}
	
	// primero una verificación rapida para evitar que este evento moleste muy seguido
	bool something_changed=false;
	for(int i=0;i<wxfb->projects.GetSize();i++) {
		// ver si hay que regenerar (comparando con fflag)
		wxString fbp_file=wxfb->projects[i];
		wxString fflag = fbp_file.Mid(0,fbp_file.Len()-4)+".flg";
		if (!wxFileName::FileExists(fflag) || wxFileName(fbp_file).GetModificationTime()>wxFileName(fflag).GetModificationTime()) {
			something_changed=true;
			break;
		}
	}
	if (!something_changed) return;
	SaveAll(false); /// @todo: ver de sacar esto (para que al actualizar los fuentes agregando metodos o lo que sea no se pierdan los cambios sin guardar)
	
	WxfbAutoCheckData *old_data = new WxfbAutoCheckData(); // for comparing after parsing updated files and detecting new/deleted windows
	if (!project->WxfbGenerate(true)) { delete old_data; return; }
	
	class LaunchProjectWxfbAutoUpdateStep2Action : public Parser::OnEndAction {
	private: WxfbAutoCheckData *m_data;
	public: void Run() override { if (project) project->WxfbAutoCheckStep2(m_data); delete m_data; }
	public: LaunchProjectWxfbAutoUpdateStep2Action(WxfbAutoCheckData *_data):m_data(_data) {}
	};
	parser->Parse(false);
	parser->OnEnd(new LaunchProjectWxfbAutoUpdateStep2Action(old_data),true);
}

void ProjectManager::WxfbAutoCheckStep2(WxfbAutoCheckData *old_data) {
	boolFlagGuard wxfb_working_guard(wxfb->working);
	WxfbAutoCheckData new_data;
	if (wxfb->update_methods) { // update already present inherited classes' methods (with new events)
		for(int i=0;i<new_data.user_classes.GetSize();i++) {
			WxfbUpdateClass(new_data.user_fathers[i],new_data.user_classes[i]);
		}
	}
	if (wxfb->update_class_list) { // create new inherited classes (from new wxfb base classes)
		SingleList<wxString> bases;
		for(int i=0;i<new_data.wxfb_classes.GetSize();i++) { 
			if (!old_data->wxfb_classes.Contains(new_data.wxfb_classes[i])) {
				bases.Add(new_data.wxfb_classes[i]);
			}
		}
		for(int i=0;i<bases.GetSize();i++) { 
			new mxWxfbInheriter(main_window,bases[i],false);
		}
	}
		
	if (wxfb->update_class_list) {// deleted unused inherited classes (from wxfb base classes that where deleted from the wxfb project)
		// find out wich wxfb classes where deleted
		SingleList<wxString> bases;
		for(int i=0;i<old_data->wxfb_classes.GetSize();i++) { 
			if (!new_data.wxfb_classes.Contains(old_data->wxfb_classes[i])) {
				bases.Add(old_data->wxfb_classes[i]);
			}
		}
		// find out wich user classes where inherited from the deleted ones
		SingleList<wxString> children;
		SingleList<wxString> fathers;
		pd_inherit *item=parser->first_inherit;
		while (item->next) {
			item=item->next;
			if (bases.Contains(item->father)) {
				children.Add(item->son);
				fathers.Add(item->father);
			}
		}
		// offer the user to deleted those inherited classes
		for(int i=0;i<children.GetSize();i++) {
			// obtener archivos de dichas clases
			pd_class *pd_aux = parser->GetClass(children[i]);
			if (!pd_aux) continue;
			wxString fname1 = pd_aux->file->name;
			wxString fname2 = mxUT::GetComplementaryFile(fname1);
			// buscarlos en el proyecto
			project_file_item *fitem1=FindFromFullPath(fname1),*fitem2=FindFromFullPath(fname2);
			if (!fitem1&&!fitem2) continue;
			if (!fitem1) fname1=""; 
			if (!fitem2) fname2="";
			// preguntar
			wxString filenames; filenames<<fname1<<(fname1.Len()&&fname2.Len()?"\n":"")<<fname2;
			mxMessageDialog::mdAns ans =
				mxMessageDialog(main_window,LANG3(WXFB_ASK_BEFORE_AUTO_DELETING,""
											"ZinjaI ha detectado que la clase <{1}> hereda de\n"
											"otra clase anteriormente autogenerada por wxFormBuilder\n"
											"(<{2}>) que ha sido eliminada en el diseñador.\n"
											"¿Desea eliminar los fuentes correspondiente a dicha clase?\n"
											"Los fuentes son: <{3}>\n"
											"\n"
											"Advertencia: debe estar seguro de que no contienen definiciones ajenas\n"
											"a dicha clase, y de que la clase base no ha sido solo renombrada.",
											children[i],fathers[i],filenames))
					.Check1(LANG(PROJMNGR_DELETE_FROM_DISK,"Eliminar el archivo del disco"),false)
					.Title(LANG(GENERAL_WARNING,"Advertencia")).ButtonsYesNo().IconWarning().Run();
			// eliminar
			if (ans.yes) {
				if (fitem1) DeleteFile(fitem1,ans.check1);
				if (fitem2) DeleteFile(fitem2,ans.check1);
			}
		}
	}
}


bool ProjectManager::WxfbNewClass(wxString base_name, wxString name) {
	wxString base_header = parser->GetClass(base_name)->file->name;
	wxString folder;
	int pos1=name.Find('\\',true);
	int pos2=name.Find('/',true);
	if (pos1!=wxNOT_FOUND && pos2!=wxNOT_FOUND) {
		int pos = pos1>pos2?pos1:pos2;
		folder=name.Mid(0,pos);
		name=name.Mid(pos+1);
	} else if (pos1!=wxNOT_FOUND) {
		folder=name.Mid(0,pos1);
		name=name.Mid(pos1+1);
	} else if (pos2!=wxNOT_FOUND) {
		folder=name.Mid(0,pos2);
		name=name.Mid(pos2+1);
	}
	if (folder.Len()) {
		folder = DIR_PLUS_FILE(project->path,folder);
		if (!wxFileName::DirExists(folder)) {
			mxMessageDialog::mdAns ans = 
				mxMessageDialog(main_window,LANG1(PROJECT_DIRNOTFOUND_CREATE,""
												  "El directorio \"<{1}>\" no existe. Desea crearlo?",folder))
					.Title(LANG(GENERAL_ERROR,"Error")).ButtonsYesNo().IconQuestion().Run();
			if (ans.yes) {
				wxFileName::Mkdir(folder);
				if (!wxFileName::DirExists(folder)) {
					mxMessageDialog(main_window,LANG(PROJECT_CANT_CREATE_DIR,"No se pudo crear el directorio."))
						.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
					return false;
				}
			} else {
				return false;
			}
		}
	} else
		folder=project->path;
	if (wxNOT_FOUND!=name.Find(' ') || wxNOT_FOUND!=name.Find('-') 
		|| wxNOT_FOUND!=name.Find('<') || wxNOT_FOUND!=name.Find('?') 
		|| wxNOT_FOUND!=name.Find('>') || wxNOT_FOUND!=name.Find('*') 
		|| wxNOT_FOUND!=name.Find('.') || wxNOT_FOUND!=name.Find(':') ) 
	{
		mxMessageDialog(main_window,LANG(PROJECT_INVALID_CLASS_NAME,""
										 "El nombre de la clase no puede incluir ni espacios ni operadores"))
			.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
		return false;
	}
	// controlar que no exista
	wxString cpp_name = DIR_PLUS_FILE(project->path,(folder.Len()?DIR_PLUS_FILE(folder,name):name)+".cpp");
	wxString h_name = DIR_PLUS_FILE(project->path,(folder.Len()?DIR_PLUS_FILE(folder,name):name)+".h");
	if (wxFileName::FileExists(cpp_name) || wxFileName::FileExists(h_name)) {
		mxMessageDialog::mdAns ans = 
			mxMessageDialog(main_window,LANG(PROJECT_WXFB_NEWFILE_EXISTS,""
											 "Ya existe un archivo con ese nombre. ¿Desea reemplazarlo?"))
			.Title(cpp_name).ButtonsYesNo().IconError().Run();
		if (ans.no) return false;
	}
	
	wxArrayString methods;
	GetFatherMethods(base_header,base_name,methods);
	// crear el cpp
	wxTextFile cpp_file(cpp_name);
	cpp_file.Create();
	cpp_file.AddLine(wxString("#include \"")+name+".h\"");
	cpp_file.AddLine("");
	cpp_file.AddLine(name+"::"+name+"(wxWindow *parent) : "+base_name+"(parent) {");
	cpp_file.AddLine("\t");
	cpp_file.AddLine("}");
	cpp_file.AddLine("");
	for (unsigned int i=0;i<methods.GetCount();i++) {
		cpp_file.AddLine(methods[i].BeforeFirst(' ')+" "+name+"::"+methods[i].AfterFirst(' ')+" {");
		cpp_file.AddLine("\tevent.Skip();");
		cpp_file.AddLine("}");
		cpp_file.AddLine("");
	}
	cpp_file.AddLine(name+"::~"+name+"() {");
	cpp_file.AddLine("\t");
	cpp_file.AddLine("}");
	cpp_file.AddLine("");
	cpp_file.Write();
	cpp_file.Close();
	// crear el h
	wxString def=name;
	def.MakeUpper();
	wxTextFile h_file(h_name);
	h_file.Create();
	h_file.AddLine(wxString("#ifndef ")+def+"_H");
	h_file.AddLine(wxString("#define ")+def+"_H");
	wxFileName fn_base_header(base_header);
	fn_base_header.MakeRelativeTo(folder);
	h_file.AddLine(wxString("#include \"")+fn_base_header.GetFullPath()+"\"");
	h_file.AddLine("");
	h_file.AddLine(wxString("class ")+name+" : public "+base_name+" {");
	h_file.AddLine("\t");
	h_file.AddLine("private:");
	h_file.AddLine("\t");
	h_file.AddLine("protected:");
	for (unsigned int i=0;i<methods.GetCount();i++)
		h_file.AddLine(wxString("\t")<<methods[i]+";");
	h_file.AddLine("\t");
	h_file.AddLine("public:");
	h_file.AddLine(wxString("\t")+name+"(wxWindow *parent=NULL);");
	h_file.AddLine(wxString("\t~")+name+"();");
	h_file.AddLine("};");
	h_file.AddLine("");
	h_file.AddLine("#endif");
	h_file.AddLine("");
	h_file.Write();
	h_file.Close();
	// abrir
	main_window->OpenFile(cpp_name,true);
	main_window->OpenFile(h_name,true);
	return true;
}

void ProjectManager::SetActiveConfiguration (project_configuration * aconf) {
	active_configuration=aconf;
	main_window->SetToolchainMode(Toolchain::SelectToolchain().IsExtern());
}

wxString ProjectManager::GetTempFolder (bool create) {
	return GetTempFolderEx(project->path,create);
}

wxString ProjectManager::GetTempFolderEx (wxString path, bool create) {
	temp_folder_short = active_configuration->temp_folder;
	temp_folder = wxFileName(DIR_PLUS_FILE(path,active_configuration->temp_folder)).GetFullPath();
	if (create && temp_folder.Len() && !wxFileName::DirExists(temp_folder))
		wxFileName::Mkdir(temp_folder,0777,wxPATH_MKDIR_FULL);
	return temp_folder;
}

void ProjectManager::CleanAll ( ) {
	project_configuration *act=active_configuration;
	for(int i=0;i<configurations_count;i++) { 
		SetActiveConfiguration(configurations[i]);
		Clean();
	}
	SetActiveConfiguration(act);
}

doxygen_configuration * ProjectManager::GetDoxygenConfiguration ( ) {
	if (!doxygen) doxygen=new doxygen_configuration(project_name);
	return doxygen;
}

valgrind_configuration *ProjectManager::GetValgrindConfiguration ( ) {
	if (!valgrind) valgrind=new valgrind_configuration();
	return valgrind;
}

wxfb_configuration * ProjectManager::GetWxfbConfiguration (bool create_activated) {
	if (!wxfb) wxfb=new wxfb_configuration(create_activated);
	return wxfb;
}

void ProjectManager::SetFileReadOnly (project_file_item * item, bool read_only) {
	item->SetReadOnly(read_only);
	mxSource *src = main_window->IsOpen(item->GetTreeItem());
	if (src) src->SetReadOnlyMode(read_only?ROM_ADD_PROJECT:ROM_DEL_PROJECT);
}

void ProjectManager::SetFileHideSymbols (project_file_item * item, bool hide_symbols) {
	item->SetSymbolsVisible(!hide_symbols);
	parser->SetHideSymbols(item->GetFullPath(),hide_symbols);
}

project_library *ProjectManager::GetDefaultLib(project_configuration *conf) {
	for(JavaVectorIterator<project_library> lib(conf->libs_to_build); lib.IsValid(); lib.Next())
		if (lib->default_lib) return lib;
	return nullptr;
}

void ProjectManager::MoveLibToBuild (project_configuration * conf, int pos, bool up) {
	EXPECT_OR( (up&&pos>0) || (!up&&pos<conf->libs_to_build.GetSize()-1), return );
	project_library *lib1 = conf->libs_to_build.Release(pos);
	int pos2 = pos + (up?-1:1);
	project_library *lib2 = conf->libs_to_build.Release(pos2);
	conf->libs_to_build.Set(pos,lib2);
	conf->libs_to_build.Set(pos2,lib1);
}

/**
* Gets a valid cppcheck_configuration. Creates and initializes it if it does not exists.
*
* @param save_in_project   if it needs to create a new instance, and this param is 
*                          true, this configuration will be stored in project's file;
*                          otherwise, it will be temporal and will be loose when
*                          closing the project
**/
cppcheck_configuration * ProjectManager::GetCppCheckConfiguration(bool save_in_project) {
	if (!cppcheck)
		cppcheck = new cppcheck_configuration(save_in_project);
	return cppcheck;
}

wxString project_library::GetPath(ProjectManager *project) const {
	wxString retval = m_path;
	mxUT::ParameterReplace(retval,"${TEMP_DIR}",project->active_configuration->temp_folder);
	return retval;
}

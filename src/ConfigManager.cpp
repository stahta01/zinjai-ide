#include "ConfigManager.h"

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/dir.h>
#ifdef __APPLE__
#	include <wx/stc/stc.h>
#	include "mxHelpWindow.h"
#endif

#include "mxUtils.h"
#include "version.h"
#include "mxMessageDialog.h"
#include "error_recovery.h"
#include "ProjectManager.h"
#include "mxPreferenceWindow.h"
#include "Language.h"
#include "mxMainWindow.h"
#include "mxColoursEditor.h"
#include "Toolchain.h"
#include "MenusAndToolsConfig.h"
#include "CustomTools.h"
#include "mxSplashScreen.h"
#include "IniFile.h"

ConfigManager *config;

/**
* @brief Config lines from main config files related to toolbar settings, 
*
* important: this is still here just for backward compatibility, since 20140723
* these lines now belong to a different config file that is read later
*
* old toolbars and menus related config lines are processed in a delayed mode 
* because we need to have an initialized MenusAndToolsConfig instance to do it,
* but MenusAndToolsConfig needs an initialized ConfigManager to use the proper 
* language
*
* ver la descripción de la clase para entender la secuencia de inicialización
**/
struct DelayedConfigLines {
	wxArrayString toolbars_keys;
	wxArrayString toolbars_values;
} *s_delayed_config_lines;

ConfigManager::ConfigManager(wxString a_path):custom_tools(MAX_CUSTOM_TOOLS) {
	config=this;
	s_delayed_config_lines = nullptr; 
	zinjai_dir = a_path;
//	zinjai_bin_dir = DIR_PLUS_FILE(zinjai_dir,"bin");
//	zinjai_third_dir = DIR_PLUS_FILE(zinjai_dir,"third");
	LoadDefaults();
}

void ConfigManager::DoInitialChecks() {
#ifdef __WIN32__
#else
	
	// elegir un explorador de archivos
	if (Files.explorer_command=="<<sin configurar>>") { // tratar de detectar automaticamente un terminal adecuado
		if (mxUT::GetOutput("xdg-open --version").Len())
			Files.explorer_command = "xdg-open";
		else if (mxUT::GetOutput("nemo --version").Len())
			Files.explorer_command = "nemo";
		else if (mxUT::GetOutput("dolphin --version").Len())
			Files.explorer_command = "dolphin";
		else if (mxUT::GetOutput("konqueror --version").Len())
			Files.explorer_command = "konqueror";
		else if (mxUT::GetOutput("nautilus --version").Len())
			Files.explorer_command = "nautilus";
		else if (mxUT::GetOutput("thunar --version").Len())
			Files.explorer_command = "thunar";
	} 
	
	// elegir un terminal
	if (Files.terminal_command=="<<sin configurar>>") { // tratar de detectar automaticamente un terminal adecuado
		wxString xterm_error_msg = LANG(CONFIG_NO_TERMINAL_FOUND,""
			"No se ha encontrado una terminal conocida. Se recomienda instalar\n"
			"xterm; luego configure el parametro \"Comando del Terminal\" en la\n"
			"pestaña \"Rutas 1\" del cuadro de \"Preferencias\".");
		LinuxTerminalInfo::Initialize();
		for(int i=0;i<LinuxTerminalInfo::count;i++) { 
			if (LinuxTerminalInfo::list[i].Test()) {
				Files.terminal_command = LinuxTerminalInfo::list[i].run_command;
				if (LinuxTerminalInfo::list[i].warning) {
					xterm_error_msg = LANG1(CONFIG_TERMINAL_WARNING,"La aplicación terminal que se ha encontrado instalada\n"
																	"es <{1}>. Algunas versiones de esta terminal pueden generar\n"
																	"problemas al intentar ejecutar un programa o proyecto. Si no logra\n"
																	"ejecutar correctamente desde ZinjaI ninguno de los programas/proyectos\n"
																	"que compile, intente configurar otra terminal desde el cuadro de\n"
																	"\"Preferencias\". La terminal recomendada es xterm).",LinuxTerminalInfo::list[i].name);
				} else 
					xterm_error_msg = "";
				break;
			}
		}
		if (xterm_error_msg.Len()) {
			if (CheckComplaintAndInstall(nullptr,
				LinuxTerminalInfo::list[0].test_command,
				LANG(CONFIG_TERMINAL,"Terminal de ejecución"),
				xterm_error_msg, "xterm","","terminal")) 
			{
				Files.terminal_command = LinuxTerminalInfo::list[0].run_command;
			}
		}
	}
	
	// verificar si hay compilador
	if (!Init.compiler_seen) {
		wxString gcc_version = mxUT::GetOutput("g++ --version");
		if (gcc_version.Len()) {
			Init.compiler_seen = true;
#ifdef __APPLE__
			// in new mac systems, g++ command is just a wrapper around clang
			if (gcc_version.Contains("clang")) {
				Files.toolchain = "clang"; 
				Toolchain::SelectToolchain(); 
			}
#endif
		} else {
			// try to use clang if g++ not found
			wxArrayString toolchains;
			Toolchain::GetNames(toolchains,true);
			if (toolchains.Index("clang")!=wxNOT_FOUND && mxUT::GetOutput("clang --version").Len()) {
				Files.toolchain = "clang"; 
				Toolchain::SelectToolchain(); 
				Init.compiler_seen = true;
			} else {
				// show error and try to install gcc
				Init.compiler_seen = CheckComplaintAndInstall(
					nullptr,"g++ --version",LANG(CONFIG_COMPILER,"Compilador C++"),
					LANG(CONFIG_COMPILER_NOT_FOUND,"No se ha encontrado un compilador para C++ (g++ o clang). Debe instalarlo\n"
												   "con el gestor de paquetes que corresponda a su distribución\n"
												   "(apt-get, yum, yast, installpkg, etc.)"), 
					"build-essential");
			}
		}
	}
	
	// verificar si hay depurador
	if (!Init.debugger_seen) {
#ifdef __APPLE__
		Init.debugger_seen = CheckComplaintAndInstall(
			nullptr, config->Files.debugger_command+" --version",
			LANG(CONFIG_DEBUGGER,"Depurador"),
			LANG(CONFIG_DEBUGGER_NOT_FOUND,"No se ha encontrado el depurador (gdb). Debe instalarlo con\n"
			"el gestor de paquetes que corresponda a su distribución\n"
			"(apt-get, yum, yast, installpkg, etc.)"),
			"gdb");
#else
		Init.debugger_seen = CheckComplaintAndInstall(
			nullptr, config->Files.debugger_command+" --version",
			LANG(CONFIG_DEBUGGER,"Depurador"),
			LANG(CONFIG_DEBUGGER_NOT_FOUND,"No se ha encontrado el depurador (gdb). Debe instalarlo con\n"
			"el gestor de paquetes que corresponda a su distribución\n"
			"(apt-get, yum, yast, installpkg, etc.)"),
			"gdb");
#endif	
	}
#endif	
	Toolchain::SelectToolchain();
}
	
bool ConfigManager::Load() {
	IniFileReader fil(m_filename);
	if (!fil.IsOk()) return false;
	wxArrayString last_files; // para compatibilidad hacia atras, guarda el historial unificado y despues lo divide
	for ( wxString section = fil.GetNextSection(); !section.IsEmpty(); section = fil.GetNextSection() ) {
		
		if (section=="Styles") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="print_size") Styles.print_size = p.AsInt();
				else if (p.Key()=="font_size") Styles.font_size = p.AsInt();
				else if (p.Key()=="font_name") Styles.font_name = p.AsString();
				else if (p.Key()=="colour_theme") Init.colour_theme = p.AsString();
				else if (p.Key()=="dark") { if (p.AsBool()) Init.colour_theme="inverted.zcs";	} // just for backward compatibility
			}
			
		} else if (section=="Source") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="tabWidth") Source.tabWidth = p.AsInt();
				else if (p.Key()=="edgeColumn") Source.edgeColumn = p.AsInt();
				else if (p.Key()=="alignComments") Source.alignComments = p.AsInt();
				else if (p.Key()=="tabUseSpaces") Source.tabUseSpaces = p.AsBool();
				else if (p.Key()=="indentPaste") Source.indentPaste = p.AsBool();
				else if (p.Key()=="smartIndent") Source.smartIndent = p.AsBool();
				else if (p.Key()=="bracketInsertion") Source.bracketInsertion = p.AsBool();
				else if (p.Key()=="syntaxEnable") Source.syntaxEnable = p.AsBool();
				else if (p.Key()=="foldEnable") Source.foldEnable = p.AsBool();
				else if (p.Key()=="indentEnable") Source.indentEnable = p.AsBool();
				else if (p.Key()=="whiteSpace") Source.whiteSpace = p.AsBool();
				else if (p.Key()=="lineNumber") Source.lineNumber = p.AsBool();
				else if (p.Key()=="overType") Source.overType = p.AsBool();
				else if (p.Key()=="autocompFilters") Source.autocompFilters = p.AsBool();
				else if (p.Key()=="callTips") Source.callTips = p.AsBool();
				else if (p.Key()=="autocompTips") Source.autocompTips = p.AsBool();
				else if (p.Key()=="autotextEnabled") Source.autotextEnabled = p.AsBool();
				else if (p.Key()=="autocloseStuff") Source.autocloseStuff = p.AsBool();
				else if (p.Key()=="toolTips") Source.toolTips = p.AsBool();
				else if (p.Key()=="autoCompletion") Source.autoCompletion = p.AsInt();
				else if (p.Key()=="avoidNoNewLineWarning") Source.avoidNoNewLineWarning = p.AsBool();
			}		
		} else if (section=="Debug") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="autohide_panels") Debug.autohide_panels = p.AsBool();
				else if (p.Key()=="autohide_toolbars") Debug.autohide_toolbars = p.AsBool();
				else if (p.Key()=="allow_edition") Debug.allow_edition = p.AsBool();
				else if (p.Key()=="format") Debug.format = p.AsString();
				else if (p.Key()=="macros_file") Debug.macros_file = p.AsString();
				else if (p.Key()=="compile_again") Debug.compile_again = p.AsBool();
				else if (p.Key()=="use_colours_for_inspections") Debug.use_colours_for_inspections = p.AsBool();
				else if (p.Key()=="inspections_can_have_side_effects") Debug.inspections_can_have_side_effects = p.AsBool();
				else if (p.Key()=="raise_main_window") Debug.raise_main_window = p.AsBool();
				else if (p.Key()=="always_debug") Debug.always_debug = p.AsBool();
//				else if (p.Key()=="close_on_normal_exit") Debug.close_on_normal_exit = p.AsBool();
				else if (p.Key()=="show_do_that") Debug.show_do_that = p.AsBool();
				else if (p.Key()=="catch_throw") Debug.catch_throw = p.AsBool();
				else if (p.Key()=="auto_solibs") Debug.auto_solibs = p.AsBool();
				else if (p.Key()=="readnow") Debug.readnow = p.AsBool();
				else if (p.Key()=="inspections_on_right") Debug.inspections_on_right = p.AsBool();
				else if (p.Key()=="show_thread_panel") Debug.show_thread_panel = p.AsBool();
				else if (p.Key()=="show_log_panel") Debug.show_log_panel = p.AsBool();
				else if (p.Key()=="return_focus_on_continue") Debug.return_focus_on_continue = p.AsBool();
				else if (p.Key()=="improve_inspections_by_type") Debug.improve_inspections_by_type = p.AsBool();
#ifdef __linux__
				else if (p.Key()=="enable_core_dump") Debug.enable_core_dump = p.AsBool();
#endif
#ifdef __WIN32__
				else if (p.Key()=="no_debug_heap") Debug.no_debug_heap = p.AsBool();
#endif
				else if (p.Key()=="use_blacklist") Debug.use_blacklist = p.AsBool();
				else if (p.Key()=="blacklist") Debug.blacklist.Add(p.AsString());
				else if (p.Key()=="inspection_improving_template") {
					Debug.inspection_improving_template_from.Add(p.AsString().BeforeFirst('|'));
					Debug.inspection_improving_template_to.Add(p.AsString().AfterFirst('|'));
				}
			}
			
		} else if (section=="Running") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="compiler_options") Running.cpp_compiler_options = p.AsString(); //just for backward compatibility
				else if (p.Key()=="cpp_compiler_options") Running.cpp_compiler_options = p.AsString();
				else if (p.Key()=="c_compiler_options") Running.c_compiler_options = p.AsString();
				else if (p.Key()=="wait_for_key") Running.wait_for_key = p.AsBool();
				else if (p.Key()=="always_ask_args") Running.always_ask_args = p.AsBool();
				else if (p.Key()=="check_includes") Running.check_includes = p.AsBool();
			}
		
		} else if (section=="Help") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
//				if (p.Key()=="quickhelp_index") Help.quickhelp_index = p.AsString();
				if (p.Key()=="wxhelp_index") Help.wxhelp_index = p.AsString();
				else if (p.Key()=="autocomp_indexes") Help.autocomp_indexes = p.AsString();
				else if (p.Key()=="min_len_for_completion") Help.min_len_for_completion = p.AsInt();
				else if (p.Key()=="show_extra_panels") Help.show_extra_panels = p.AsBool();
			}
			
		} else if (section=="Init") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="left_panels") Init.left_panels = p.AsBool();
				else if (p.Key()=="show_beginner_panel") Init.show_beginner_panel = p.AsBool();
				else if (p.Key()=="show_minimap_panel") Init.show_minimap_panel = p.AsBool();
				else if (p.Key()=="show_welcome") Init.show_welcome = p.AsBool();
				else if (p.Key()=="show_tip_on_startup") Init.show_tip_on_startup = p.AsBool();
				else if (p.Key()=="new_file") Init.new_file = p.AsInt();
				else if (p.Key()=="version") Init.version = p.AsInt();
				else if (p.Key()=="pos_x") Init.pos_x = p.AsInt();
				else if (p.Key()=="pos_y") Init.pos_y = p.AsInt();
				else if (p.Key()=="size_x") Init.size_x = p.AsInt();
				else if (p.Key()=="size_y") Init.size_y = p.AsInt();
				else if (p.Key()=="lang_es") Init.lang_es = p.AsBool();
				else if (p.Key()=="maximized") Init.maximized = p.AsBool();
				else if (p.Key()=="zinjai_server_port") Init.zinjai_server_port = p.AsInt();
//				else if (p.Key()=="load_sharing_server") Init.load_sharing_server = p.AsBool();
				else if (p.Key()=="save_project") Init.save_project = p.AsBool();
//				else if (p.Key()=="close_files_for_project") Init.close_files_for_project = p.AsBool();
				else if (p.Key()=="always_add_extension") Init.always_add_extension = p.AsBool();
				else if (p.Key()=="autohide_menus_fs") Init.autohide_menus_fs = p.AsBool();
				else if (p.Key()=="autohide_panels_fs") Init.autohide_panels_fs = p.AsBool();
				else if (p.Key()=="autohide_toolbars_fs") Init.autohide_toolbars_fs = p.AsBool();
				else if (p.Key()=="check_for_updates") Init.check_for_updates = p.AsBool();
				else if (p.Key()=="prefer_explorer_tree") Init.prefer_explorer_tree = p.AsBool();
				else if (p.Key()=="cppcheck_seen") Init.cppcheck_seen = p.AsBool();
#ifndef __WIN32__
				else if (p.Key()=="valgrind_seen") Init.valgrind_seen = p.AsBool();
				else if (p.Key()=="compiler_seen") Init.compiler_seen = p.AsBool();
				else if (p.Key()=="debugger_seen") Init.debugger_seen = p.AsBool();
#endif
				else if (p.Key()=="doxygen_seen") Init.doxygen_seen = p.AsBool();
				else if (p.Key()=="wxfb_seen") Init.wxfb_seen = p.AsBool();
				else if (p.Key()=="show_explorer_tree") Init.show_explorer_tree = p.AsBool();
				else if (p.Key()=="graphviz_dot") Init.graphviz_dot = p.AsBool();
				else if (p.Key()=="history_len") Init.history_len = p.AsInt();
				else if (p.Key()=="inherit_num") Init.inherit_num = p.AsInt();
				//				else if (p.Key()=="forced_compiler_options") Init.forced_compiler_options = p.AsString();
				//				else if (p.Key()=="forced_linker_options") Init.forced_linker_options = p.AsString();
				else if (p.Key()=="proxy") Init.proxy = p.AsString();
				else if (p.Key()=="language_file") Init.language_file = p.AsString();
				else if (p.Key()=="max_errors") Init.max_errors = p.AsInt();
				else if (p.Key()=="max_jobs") Init.max_jobs = p.AsInt();
				else if (p.Key()=="wrap_mode") Init.wrap_mode = p.AsInt();
				else if (p.Key()=="singleton") Init.singleton = p.AsBool();
				else if (p.Key()=="stop_compiling_on_error") Init.stop_compiling_on_error = p.AsBool();
				else if (p.Key()=="autohide_panels") Init.autohide_panels = p.AsBool();
				else if (p.Key()=="use_cache_for_subcommands") Init.use_cache_for_subcommands = p.AsBool();
				else if (p.Key()=="beautify_compiler_errors") Init.beautify_compiler_errors = p.AsBool();
				else if (p.Key()=="fullpath_on_project_tree") Init.fullpath_on_project_tree = p.AsBool();
				else if (p.Key()=="colour_theme") Init.colour_theme = p.AsString();
				else if (p.Key()=="complements_timestamp") Init.complements_timestamp = p.AsString();
#if defined(__APPLE__) && defined(__STC_ZASKAR)
				else if (p.Key()=="mac_stc_zflags") Init.mac_stc_zflags = p.AsInt();
#endif
			}
			
		} else if (section=="Files") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key()=="toolchain") Files.toolchain = p.AsString();
				else if (p.Key()=="debugger_command") Files.debugger_command = p.AsString();
				else if (p.Key()=="terminal_command") Files.terminal_command = p.AsString();
				else if (p.Key()=="explorer_command") Files.explorer_command = p.AsString();
				else if (p.Key()=="c_template") Files.c_template = p.AsString();
				else if (p.Key()=="cpp_template") Files.cpp_template = p.AsString();
				else if (p.Key()=="default_template") Files.default_template = p.AsString();
				else if (p.Key()=="default_project") Files.default_project = p.AsString();
				else if (p.Key()=="autocodes_file") Files.autocodes_file = p.AsString();
				else if (p.Key()=="skin_dir") Files.skin_dir = p.AsString();
				else if (p.Key()=="temp_dir") Files.temp_dir = p.AsString();
				else if (p.Key()=="img_viewer") Files.img_viewer = p.AsString();
				else if (p.Key()=="xdot_command") Files.xdot_command = p.AsString();
				//				else if (p.Key()=="graphviz_dir") Files.graphviz_dir = p.AsString();
				else if (p.Key()=="browser_command") Files.browser_command = p.AsString();
				else if (p.Key()=="cppcheck_command") Files.cppcheck_command = p.AsString();
#ifndef __WIN32__
				else if (p.Key()=="valgrind_command") Files.valgrind_command = p.AsString();
#endif
				else if (p.Key()=="doxygen_command") Files.doxygen_command = p.AsString();
				else if (p.Key()=="wxfb_command") Files.wxfb_command = p.AsString();
				else if (p.Key()=="project_folder") Files.project_folder = p.AsString();
				else if (p.Key()=="last_project_dir") Files.last_project_dir = p.AsString();
				else if (p.Key()=="last_dir") Files.last_dir = p.AsString();
				else if (p.Key().StartsWith("last_file_")) {
					last_files.Add(p.AsString());
				} else if (p.Key().StartsWith("last_source_")) {
					long l;
					if (p.Key().Mid(12).ToLong(&l) && l>=0 && l<CM_HISTORY_MAX_LEN)
						Files.last_source[l]=p.AsString();
				} else if (p.Key().StartsWith("last_project_")	) {
					long l;
					if (p.Key().Mid(13).ToLong(&l) && l>=0 && l<CM_HISTORY_MAX_LEN)
						Files.last_project[l]=p.AsString();
				}
			}
			
		} else if (section=="Columns") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (p.Key().StartsWith("inspections_grid_")	) {
					long l;
					if (p.Key().Mid(17).ToLong(&l) && l>=0 && l<IG_COLS_COUNT)
						Cols.inspections_grid[l] = p.AsBool();
				} else if (p.Key().StartsWith("backtrace_grid_")	) {
					long l;
					if (p.Key().Mid(15).ToLong(&l) && l>=0 && l<BG_COLS_COUNT)
						Cols.backtrace_grid[l] = p.AsBool();
				}
//				} else if (p.Key().StartsWith("threadlist_grid_")	) {
//					if (p.Key().Mid(15).ToLong(&l) && l>=0 && l<TG_COLS_COUNT)
//						Cols.threadlist_grid[l] = p.AsBool();
//				}
			}
			
		} else if (section=="CustomTools") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				custom_tools.ParseConfigLine(p.Key(),p.AsString());
			}
			
		} else if (section=="Toolbars") {
			for( IniFileReader::Pair p = fil.GetNextPair(); p.IsOk(); p = fil.GetNextPair() ) {
				if (!s_delayed_config_lines) s_delayed_config_lines = new DelayedConfigLines;
				s_delayed_config_lines->toolbars_keys.Add(p.Key());
				s_delayed_config_lines->toolbars_values.Add(p.AsString());
			}
		}
		
	}
	
#ifdef __APPLE__
	if (Init.version<20170926 && Files.debugger_command=="gdb") Files.debugger_command = "~/.zinjai/gdb.bin";
	if (Init.version<20170605) Files.terminal_command = DIR_PLUS_FILE("bin","mac-terminal-wrapper.bin"); // some installations still have an invalid configuration
#endif
	if (Init.version<20100806) Files.terminal_command.Replace("ZinjaI - Consola de Ejecucion","${TITLE}"); // NO USAR ACENTOS, PUEDE ROMER EL X!!!! (me daba un segfault en la libICE al poner el ó en EjeuciÓn)
	if (Init.version<20101112 && Help.autocomp_indexes.Len()) Help.autocomp_indexes<<",STL_Iteradores";
	if (Init.version<20110418) Debug.use_colours_for_inspections=true;
	if (Init.version<20110420) Init.check_for_updates=true;
	
//#ifdef __WIN32__
//	if (Init.version<20120208 && !Init.forced_linker_options.Contains("-static-libstdc++")) Init.forced_linker_options<<" -static-libstdc++";
//#else
//	// " -static-libstdc++" estaba por error, solo se deberia haber agregado en windows, pero por error el if de arriba estaba fuera del #ifdef
//	if (Init.version<20120229 && Init.forced_linker_options.Contains(" -static-libstdc++")) Init.forced_linker_options.Replace(" -static-libstdc++","",false);
//#endif
	if (Init.version<20100828) {
//#ifdef __WIN32__
//		Init.forced_compiler_options<<" --show-column";
//#endif
		if (!Running.cpp_compiler_options.Contains("-O")) {
			if (!Running.cpp_compiler_options.EndsWith(" "))
				Running.cpp_compiler_options<<" ";
			Running.cpp_compiler_options<<"-O0";
		}
	}
	
//	if (Init.version<20130730) {
//		if (Running.cpp_compiler_options.Contains("-O0"))
//			Running.cpp_compiler_options.Replace("-O0","-Og");
//	}
	
	if (Init.version<20131223) {
		if (Running.cpp_compiler_options.Contains("-Og"))
			Running.cpp_compiler_options.Replace("-Og","-O0");
	}
	
	if (Init.version<20140319) {
		if (Help.autocomp_indexes.Contains("STL"))
			Help.autocomp_indexes+=",AAA_STL,AAA_STL_11";
		if (Help.autocomp_indexes.Contains("AAA_Estandar"))
			Help.autocomp_indexes+=",AAA_Estandar_Cpp_11";
		if (Help.autocomp_indexes.Contains("AAA_Palabras"))
			Help.autocomp_indexes+=",AAA_Palabras_Reservadas_11";
	}
	
	if (Init.version<20140606 && Files.terminal_command=="xterm -T \"${TITLE}\" -e") Files.terminal_command="xterm -T \"${TITLE}\" -fa \"Liberation Mono\" -fs 12 -e";
		
	if (last_files.GetCount()) {
		int ps=0, pp=0;
		for (unsigned int i=0;i<last_files.GetCount();i++) {
			if (wxFileName(last_files[i]).GetExt().CmpNoCase(PROJECT_EXT)==0)
				Files.last_project[pp++]=last_files[i];
			else
				Files.last_source[ps++]=last_files[i];
		}
	}
	if (Init.version<20140704) {
		if (Init.proxy=="") Init.proxy="$http_proxy";
	}
	SetDefaultInspectionsImprovingTemplates(Init.version);
		
#ifdef _STC_HAS_ZASKARS_RESHOW
	if (Init.version<20141127) {
		if (Source.autoCompletion) Source.autoCompletion=2;
		Source.autocompFilters=true;
	}
#endif
	if (Init.version<20141212 && Debug.blacklist.GetCount()==1) {
		wxString orig = Debug.blacklist[0]; Debug.blacklist.Clear();
		mxUT::Split(orig,config->Debug.blacklist,true,false);
	}
	
	if (Init.version<20141218) {
		for(int i=0;i<custom_tools.GetCount();i++) { 
			if (custom_tools[i].output_mode>=2)
				custom_tools[i].output_mode++;
		}
	}
#ifdef __WIN32__
	if (Init.version<20150708) {
		Running.cpp_compiler_options += " -finput-charset=iso-8859-1 -fexec-charset=cp437";
		Running.c_compiler_options += " -finput-charset=iso-8859-1 -fexec-charset=cp437";
	}
#else 
	if (Init.version==20150817) { // fix a buggy default setting in previous version
		Running.cpp_compiler_options.Replace("-finput-charset=iso-8859-1 -fexec-charset=cp437","");
		Running.c_compiler_options.Replace("-finput-charset=iso-8859-1 -fexec-charset=cp437","");
	}
#endif
	
	if (Init.version<20160812) {
		for(unsigned int i=0;i<Debug.blacklist.GetCount();i++)
			Debug.blacklist[i] = wxString("file ")+mxUT::Quotize(Debug.blacklist[i]);
	}
	
	
	if (Init.version<20161130) {
		if (Files.toolchain=="gcc-mingw32") Files.toolchain="mingw32-gcc6";
#ifdef __WIN32__
		if (Help.wxhelp_index=="MinGW\\wx\\docs\\wx_contents.html") Help.wxhelp_index="";
#endif
	} else if (Init.version<20170828)
		if (Files.toolchain=="gcc5-mingw32") Files.toolchain="mingw32-gcc6";
	
	return true;
}
	
bool ConfigManager::Save(){
	wxTextFile fil(m_filename);
	if (fil.Exists())
		fil.Open();
	else
		fil.Create();
	fil.Clear();
	
#ifdef __WIN32__
	fil.AddLine(wxString("# generado por ZinjaI-w32-")<<VERSION);
#elif defined(__APPLE__)
	fil.AddLine(wxString("# generado por ZinjaI-mac-")<<VERSION);
#else
	fil.AddLine(wxString("# generado por ZinjaI-lnx-")<<VERSION);
#endif
	fil.AddLine("[Init]");
	CFG_BOOL_WRITE_DN("left_panels",Init.left_panels);
	CFG_BOOL_WRITE_DN("show_welcome",Init.show_welcome);
	CFG_BOOL_WRITE_DN("show_beginner_panel",Init.show_beginner_panel);
	CFG_BOOL_WRITE_DN("show_minimap_panel",Init.show_minimap_panel);
	CFG_BOOL_WRITE_DN("show_tip_on_startup",Init.show_tip_on_startup);
	CFG_GENERIC_WRITE_DN("new_file",Init.new_file);
	CFG_GENERIC_WRITE_DN("version",VERSION);
	CFG_GENERIC_WRITE_DN("pos_x",Init.pos_x);
	CFG_GENERIC_WRITE_DN("pos_y",Init.pos_y);
	CFG_GENERIC_WRITE_DN("size_x",Init.size_x);
	CFG_GENERIC_WRITE_DN("size_y",Init.size_y);
	CFG_BOOL_WRITE_DN("maximized",Init.maximized);
	CFG_BOOL_WRITE_DN("lang_es",Init.lang_es);
	CFG_GENERIC_WRITE_DN("zinjai_server_port",Init.zinjai_server_port);
	CFG_BOOL_WRITE_DN("save_project",Init.save_project);
//	CFG_BOOL_WRITE_DN("close_files_for_project",Init.close_files_for_project);
	CFG_BOOL_WRITE_DN("always_add_extension",Init.always_add_extension);
	CFG_BOOL_WRITE_DN("autohide_toolbars_fs",Init.autohide_toolbars_fs);
	CFG_BOOL_WRITE_DN("autohide_panels_fs",Init.autohide_panels_fs);
	CFG_BOOL_WRITE_DN("autohide_menus_fs",Init.autohide_menus_fs);
	CFG_BOOL_WRITE_DN("check_for_updates",Init.check_for_updates);
	CFG_BOOL_WRITE_DN("prefer_explorer_tree",Init.prefer_explorer_tree);
	CFG_BOOL_WRITE_DN("cppcheck_seen",Init.cppcheck_seen);
#ifndef __WIN32__
	CFG_BOOL_WRITE_DN("valgrind_seen",Init.valgrind_seen);
	CFG_BOOL_WRITE_DN("compiler_seen",Init.compiler_seen);
	CFG_BOOL_WRITE_DN("debugger_seen",Init.debugger_seen);
#endif
	CFG_BOOL_WRITE_DN("doxygen_seen",Init.doxygen_seen);
	CFG_BOOL_WRITE_DN("wxfb_seen",Init.wxfb_seen);
	CFG_BOOL_WRITE_DN("show_explorer_tree",Init.show_explorer_tree);
	CFG_BOOL_WRITE_DN("graphviz_dot",Init.graphviz_dot);
	CFG_GENERIC_WRITE_DN("inherit_num",Init.inherit_num);
	CFG_GENERIC_WRITE_DN("history_len",Init.history_len);
	CFG_GENERIC_WRITE_DN("proxy",Init.proxy);
	CFG_GENERIC_WRITE_DN("language_file",Init.language_file);
	CFG_GENERIC_WRITE_DN("max_errors",Init.max_errors);
	CFG_GENERIC_WRITE_DN("max_jobs",Init.max_jobs);
	CFG_GENERIC_WRITE_DN("wrap_mode",Init.wrap_mode);
	CFG_BOOL_WRITE_DN("singleton",Init.singleton);
	CFG_BOOL_WRITE_DN("stop_compiling_on_error",Init.stop_compiling_on_error);
	CFG_BOOL_WRITE_DN("autohide_panels",Init.autohide_panels);
	CFG_BOOL_WRITE_DN("fullpath_on_project_tree",Init.fullpath_on_project_tree);
	CFG_BOOL_WRITE_DN("use_cache_for_subcommands",Init.use_cache_for_subcommands);
	CFG_BOOL_WRITE_DN("beautify_compiler_errors",Init.beautify_compiler_errors);
	CFG_GENERIC_WRITE_DN("colour_theme",Init.colour_theme);
	CFG_GENERIC_WRITE_DN("complements_timestamp",Init.complements_timestamp);
#ifdef __APPLE__
	CFG_GENERIC_WRITE_DN("mac_stc_zflags",Init.mac_stc_zflags);
#endif				
	fil.AddLine("");

	fil.AddLine("[Debug]");
	CFG_BOOL_WRITE_DN("allow_edition",Debug.allow_edition);
	CFG_BOOL_WRITE_DN("autohide_panels",Debug.autohide_panels);
	CFG_BOOL_WRITE_DN("autohide_toolbars",Debug.autohide_toolbars);
	CFG_BOOL_WRITE_DN("use_colours_for_inspections",Debug.use_colours_for_inspections);
	CFG_BOOL_WRITE_DN("inspections_can_have_side_effects",Debug.inspections_can_have_side_effects);
	CFG_BOOL_WRITE_DN("raise_main_window",Debug.raise_main_window);
	CFG_BOOL_WRITE_DN("compile_again",Debug.compile_again);
	CFG_BOOL_WRITE_DN("always_debug",Debug.always_debug);
	CFG_BOOL_WRITE_DN("use_blacklist",Debug.use_blacklist);
	if (Debug.format.Len()) CFG_GENERIC_WRITE_DN("format",Debug.format);
	for(unsigned int i=0;i<Debug.blacklist.GetCount();i++) 
		CFG_GENERIC_WRITE_DN("blacklist",Debug.blacklist[i]);
	CFG_GENERIC_WRITE_DN("macros_file",Debug.macros_file);
	CFG_BOOL_WRITE_DN("show_do_that",Debug.show_do_that);
	CFG_BOOL_WRITE_DN("show_thread_panel",Debug.show_thread_panel);
	CFG_BOOL_WRITE_DN("show_log_panel",Debug.show_log_panel);
	CFG_BOOL_WRITE_DN("inspections_on_right",Debug.inspections_on_right);
	CFG_BOOL_WRITE_DN("readnow",Debug.readnow);
	CFG_BOOL_WRITE_DN("catch_throw",Debug.catch_throw);
	CFG_BOOL_WRITE_DN("auto_solibs",Debug.auto_solibs);
	CFG_BOOL_WRITE_DN("return_focus_on_continue",Debug.return_focus_on_continue);
	CFG_BOOL_WRITE_DN("improve_inspections_by_type",Debug.improve_inspections_by_type);
#ifdef __linux__
	CFG_BOOL_WRITE_DN("enable_core_dump",Debug.enable_core_dump);
#endif
#ifdef __WIN32__
	CFG_BOOL_WRITE_DN("no_debug_heap",Debug.no_debug_heap);
#endif
	for(unsigned int i=0;i<Debug.inspection_improving_template_from.GetCount();i++)
		CFG_GENERIC_WRITE_DN("inspection_improving_template",Debug.inspection_improving_template_from[i]+"|"+Debug.inspection_improving_template_to[i]);
	fil.AddLine("");
	
	fil.AddLine("[Styles]");
	CFG_GENERIC_WRITE_DN("print_size",Styles.print_size);
	CFG_GENERIC_WRITE_DN("font_size",Styles.font_size);
	CFG_GENERIC_WRITE_DN("font_name",Styles.font_name);
	CFG_GENERIC_WRITE_DN("colour_theme",Init.colour_theme);
	fil.AddLine("");

	fil.AddLine("[Source]");
	CFG_BOOL_WRITE_DN("smartIndent",Source.smartIndent);
	CFG_BOOL_WRITE_DN("indentPaste",Source.indentPaste);
	CFG_BOOL_WRITE_DN("bracketInsertion",Source.bracketInsertion);
	CFG_BOOL_WRITE_DN("syntaxEnable",Source.syntaxEnable);
	CFG_BOOL_WRITE_DN("foldEnable",Source.foldEnable);
	CFG_BOOL_WRITE_DN("indentEnable",Source.indentEnable);
	CFG_BOOL_WRITE_DN("whiteSpace",Source.whiteSpace);
	CFG_BOOL_WRITE_DN("lineNumber",Source.lineNumber);
	CFG_BOOL_WRITE_DN("overType",Source.overType);
	CFG_BOOL_WRITE_DN("autocompFilters",Source.autocompFilters);
	CFG_BOOL_WRITE_DN("callTips",Source.callTips);
	CFG_BOOL_WRITE_DN("autocompTips",Source.autocompTips);
	CFG_BOOL_WRITE_DN("toolTips",Source.toolTips);
	CFG_BOOL_WRITE_DN("autotextEnabled",Source.autotextEnabled);
	CFG_BOOL_WRITE_DN("autocloseStuff",Source.autocloseStuff);
	CFG_GENERIC_WRITE_DN("autoCompletion",Source.autoCompletion);
	CFG_BOOL_WRITE_DN("avoidNoNewLineWarning",Source.avoidNoNewLineWarning);
	CFG_GENERIC_WRITE_DN("edgeColumn",Source.edgeColumn);
	CFG_GENERIC_WRITE_DN("alignComments",Source.alignComments);
	CFG_GENERIC_WRITE_DN("tabWidth",Source.tabWidth);
	CFG_BOOL_WRITE_DN("tabUseSpaces",Source.tabUseSpaces);
	fil.AddLine("");

	fil.AddLine("[Running]");
	CFG_GENERIC_WRITE_DN("cpp_compiler_options",Running.cpp_compiler_options);
	CFG_GENERIC_WRITE_DN("c_compiler_options",Running.c_compiler_options);
	CFG_BOOL_WRITE_DN("wait_for_key",Running.wait_for_key);
	CFG_BOOL_WRITE_DN("always_ask_args",Running.always_ask_args);
	CFG_BOOL_WRITE_DN("check_includes",Running.check_includes);
	fil.AddLine("");

	fil.AddLine("[Help]");
	CFG_GENERIC_WRITE_DN("wxhelp_index",Help.wxhelp_index);
	CFG_GENERIC_WRITE_DN("autocomp_indexes",Help.autocomp_indexes);
	CFG_GENERIC_WRITE_DN("min_len_for_completion",Help.min_len_for_completion);
	CFG_BOOL_WRITE_DN("show_extra_panels",Help.show_extra_panels);
	fil.AddLine("");

	fil.AddLine("[Files]");
	CFG_GENERIC_WRITE_DN("temp_dir",Files.temp_dir);
	CFG_GENERIC_WRITE_DN("img_viewer",Files.img_viewer);
	CFG_GENERIC_WRITE_DN("xdot_command",Files.xdot_command);
	CFG_GENERIC_WRITE_DN("skin_dir",Files.skin_dir);
	CFG_GENERIC_WRITE_DN("debugger_command",Files.debugger_command);
	CFG_GENERIC_WRITE_DN("toolchain",Files.toolchain);
	CFG_GENERIC_WRITE_DN("cppcheck_command",Files.cppcheck_command);
	CFG_GENERIC_WRITE_DN("valgrind_command",Files.valgrind_command);
	CFG_GENERIC_WRITE_DN("terminal_command",Files.terminal_command);
	CFG_GENERIC_WRITE_DN("explorer_command",Files.explorer_command);
	CFG_GENERIC_WRITE_DN("c_template",Files.c_template);
	CFG_GENERIC_WRITE_DN("cpp_template",Files.cpp_template);
	CFG_GENERIC_WRITE_DN("default_template",Files.default_template);
	CFG_GENERIC_WRITE_DN("default_project",Files.default_project);
	CFG_GENERIC_WRITE_DN("autocodes_file",Files.autocodes_file);
	CFG_GENERIC_WRITE_DN("doxygen_command",Files.doxygen_command);
	CFG_GENERIC_WRITE_DN("wxfb_command",Files.wxfb_command);
	CFG_GENERIC_WRITE_DN("browser_command",Files.browser_command);
	CFG_GENERIC_WRITE_DN("project_folder",Files.project_folder);
	CFG_GENERIC_WRITE_DN("last_dir",Files.last_dir);
	CFG_GENERIC_WRITE_DN("last_project_dir",Files.last_project_dir);
	for (int i=0;i<CM_HISTORY_MAX_LEN;i++)
		if (Files.last_source[i].Len()) fil.AddLine(wxString("last_source_")<<i<<"="<<Files.last_source[i]);
	for (int i=0;i<CM_HISTORY_MAX_LEN;i++)
		if (Files.last_project[i].Len()) fil.AddLine(wxString("last_project_")<<i<<"="<<Files.last_project[i]);
	fil.AddLine("");
	
	fil.AddLine("[Columns]");
	for (int i=0;i<IG_COLS_COUNT;i++)
		fil.AddLine(wxString("inspections_grid_")<<i<<"="<<(Cols.inspections_grid[i]?"1":"0"));
	for (int i=0;i<BG_COLS_COUNT;i++)
		fil.AddLine(wxString("backtrace_grid_")<<i<<"="<<(Cols.backtrace_grid[i]?"1":"0"));
//	for (int i=0;i<TG_COLS_COUNT;i++)
//		fil.AddLine(wxString("threadlist_grid_")<<i<<"="<<(Cols.threadlist_grid[i]?"1":"0"));
	fil.AddLine("");
	
	fil.AddLine("[CustomTools]");
	custom_tools.WriteConfig(fil);
	fil.AddLine("");
	
//	menu_data->SaveShortcutsSettings(DIR_PLUS_FILE(home_dir,"shortcuts.zsc")); // se hace en e Ok del mxShortcutsDialog
	menu_data->SaveToolbarsSettings(DIR_PLUS_FILE(config_dir,"toolbar.ztb"));
	
	fil.Write();
	fil.Close();

	return true;
}
	
static void EnsurePathExists(const wxString &path) {
	if (!wxFileName::DirExists(path)) wxFileName::Mkdir(path);
}

void ConfigManager::LoadDefaults(){

	// crear el directorio para zinjai si no existe
	config_dir = "config.here";
	if (!wxFileName::DirExists(config_dir))
		config_dir = DIR_PLUS_FILE(wxFileName::GetHomeDir(),_if_win32("zinjai",".zinjai"));
	EnsurePathExists(config_dir);
	m_filename = DIR_PLUS_FILE(config_dir,"config");
	
	// establecer valores predeterminados para todas las estructuras
	Files.temp_dir=DIR_PLUS_FILE(config_dir,"tmp");
	Files.skin_dir="imgs";
#ifdef __WIN32__
	Files.toolchain="gcc6-mingw32";
	Files.runner_command=DIR_	Files.debugger_command="gdb";
PLUS_FILE("bin","runner.exe");
	Files.terminal_command="";
	Files.explorer_command="explorer";
	Files.img_viewer="";
	Files.doxygen_command="c:\\archivos de programa\\doxygen\\bin\\doxygen.exe";
	Files.wxfb_command="";
	Files.browser_command=""; // use "bin\\shellexecute.exe";
#elif defined(__APPLE__)
	Files.toolchain="gcc";
	Files.debugger_command="~/.zinjai/gdb.bin";
	Files.runner_command=DIR_PLUS_FILE("bin","runner.bin");
	Files.terminal_command=DIR_PLUS_FILE("bin","mac-terminal-wrapper.bin");
	Files.explorer_command="open";
	Files.img_viewer="open";
	Files.doxygen_command="/Applications/Doxygen.app/Contents/Resources/doxygen";
	Files.wxfb_command="/Applications/wxFormBuilder.app/Contents/MacOS/wxformbuilder";
	Files.browser_command="open";
#else
	Files.toolchain="gcc";
	Files.debugger_command="gdb";
	Files.runner_command=DIR_PLUS_FILE(GetZinjaiBinDir(),"runner.bin");
	Files.explorer_command="<<sin configurar>>";
	Files.terminal_command="<<sin configurar>>";
	Files.img_viewer="";
	Files.wxfb_command="wxformbuilder";
	Files.cppcheck_command="cppcheck";
	Files.valgrind_command="valgrind";
	Files.doxygen_command="doxygen";
	Files.browser_command="firefox";
#endif
	Files.project_folder = (config_dir=="config.here")? "projects" : DIR_PLUS_FILE(wxFileName::GetHomeDir(),"projects");
	Files.cpp_template = Files.default_template="default_14.tpl";
	Files.c_template = "default_c.tpl";
	Files.default_project="<main>";
	Files.autocodes_file=DIR_PLUS_FILE(config_dir,"autocodes");
	for (int i=0;i<CM_HISTORY_MAX_LEN;i++)
		Files.last_source[i]="";
	for (int i=0;i<CM_HISTORY_MAX_LEN;i++)
		Files.last_project[i]="";
	Files.last_dir=wxFileName::GetHomeDir();
	Files.last_project_dir=wxFileName::GetHomeDir();

	Init.show_minimap_panel=false;
	Init.show_beginner_panel=false;
	Init.show_welcome=true;
	Init.show_tip_on_startup=true;
	Init.left_panels=false;
	Init.new_file=2;
	Init.version=0;
	Init.pos_x=Init.pos_y=0;
	Init.size_x=Init.size_y=0;
	Init.maximized=true;
	Init.lang_es=false;
	Init.zinjai_server_port=46527;
//	Init.load_sharing_server=false;
	Init.save_project=false;
//	Init.close_files_for_project=false;
	Init.always_add_extension=false;
	Init.autohide_toolbars_fs=true;
	Init.autohide_menus_fs=false;
	Init.autohide_panels_fs=true;
	Init.check_for_updates=false;
	Init.inherit_num=3;
	Init.history_len=10;
	Init.prefer_explorer_tree=false;
	Init.graphviz_dot=true;
	Init.show_explorer_tree=false;
	
	Init.cppcheck_seen=false;
#ifndef __WIN32__
	Init.valgrind_seen=false;
	Init.compiler_seen=false;
	Init.debugger_seen=false;
#endif
	Init.wxfb_seen=false;
	Init.doxygen_seen=false;
	Init.singleton=true;
	Init.stop_compiling_on_error=true;
	Init.autohide_panels=false;
	Init.use_cache_for_subcommands=true;
	Init.beautify_compiler_errors=true;
	Init.fullpath_on_project_tree=false;

	Styles.print_size=8;
	Styles.font_size=10;
	Styles.font_name=wxFont(10,wxMODERN,wxNORMAL,wxNORMAL).GetFaceName();
	Init.colour_theme="default.zcs";
	
	Init.wrap_mode=1;
	Source.smartIndent=true;
	Source.indentPaste=true;
	Source.bracketInsertion=true;
	Source.syntaxEnable=true;
	Source.whiteSpace=false;
	Source.lineNumber=true;
	Source.indentEnable=true;
	Source.foldEnable=true;
	Source.overType=false;
	Source.autocloseStuff=false;
	Source.autotextEnabled=true;
	Source.toolTips=true;
	Source.edgeColumn=80;
	Source.alignComments=80;
	Source.tabWidth=4;
	Source.tabUseSpaces=false;
	Source.autoCompletion=2;
	Source.autocompFilters=true;
	Source.callTips=true;
	Source.autocompTips=true;
#ifdef __APPLE__
	Source.autocompTips=false;
	Init.mac_stc_zflags=-1;
#endif
	Source.avoidNoNewLineWarning=true;

	Running.cpp_compiler_options="-Wall -pedantic-errors -O0";
	Running.c_compiler_options="-Wall -pedantic-errors -O0";
#ifdef __WIN32__
	Running.c_compiler_options+=" -finput-charset=iso-8859-1 -fexec-charset=cp437";
	Running.cpp_compiler_options+=" -finput-charset=iso-8859-1 -fexec-charset=cp437";
#endif
	Running.wait_for_key=true;
	Running.always_ask_args=false;
	Running.check_includes=true;
	
#ifdef __WIN32__
	Init.proxy="";
#else
	Init.proxy="$http_proxy";
#endif
	Init.language_file="spanish";
	Init.max_errors=500;
	Init.max_jobs=wxThread::GetCPUCount();
	if (Init.max_jobs<1) Init.max_jobs=1;
	
	Help.wxhelp_index="";
	Help.cppreference_dir="cppreference/en";
	Help.guihelp_dir="guihelp";
	Help.autocomp_indexes="AAA_Directivas_de_Preprocesador,AAA_Estandar_C,AAA_Estandar_Cpp,AAA_STL,AAA_Palabras_Reservadas,AAA_Estandar_Cpp_11,AAA_STL_11,AAA_Palabras_Reservadas_11";
	Help.min_len_for_completion=3;
	Help.show_extra_panels=true;
	
	Debug.use_colours_for_inspections = true;
	Debug.inspections_can_have_side_effects = false;
	Debug.allow_edition = false;
	Debug.autohide_panels = true;
	Debug.autohide_toolbars = true;
//	Debug.close_on_normal_exit = true;
	Debug.always_debug = false;
	Debug.raise_main_window = true;
	Debug.compile_again = true;
	Debug.format = "";
	Debug.macros_file = DIR_PLUS_FILE(config_dir,"debug_macros.gdb");
	Debug.inspections_on_right = false;
	Debug.show_thread_panel = false;
	Debug.show_log_panel = false;
	Debug.catch_throw = true;
	Debug.auto_solibs = false;
	Debug.readnow = false;
	Debug.show_do_that = false;
	Debug.return_focus_on_continue = true;
	Debug.improve_inspections_by_type = true;
#ifdef __linux__
	Debug.enable_core_dump = false;
#endif
#ifdef __WIN32__
	Debug.no_debug_heap = true;
#endif
	Debug.use_blacklist = true;
//	SetDefaultInspectionsImprovingTemplates(); // not needed, done (only on first run) in ConfigManager::Load when version<20140924
	
	for (int i=0;i<IG_COLS_COUNT;i++)
		Cols.inspections_grid[i]=true;
	for (int i=0;i<BG_COLS_COUNT;i++)
		Cols.backtrace_grid[i]=true;
//	for (int i=0;i<TG_COLS_COUNT;i++)
//		Cols.threadlist_grid[i]=true;
	
}

bool ConfigManager::CheckWxfbPresent() {
	if (config->Init.wxfb_seen && !config->Files.wxfb_command.IsEmpty()) return true;
	wxfb_configuration *wxfb_conf = project?project->GetWxfbConfiguration():nullptr;
	boolFlagGuard wxfb_working_guard(project?&(wxfb_conf->working):nullptr);
	wxString out, check_command = mxUT::Quotize(config->Files.wxfb_command)+" -h";
#ifdef __WIN32__
	if (!config->Files.wxfb_command.IsEmpty())
		out = mxUT::GetOutput(check_command,true);
	if (!out.Len()) {
		if (wxFileName::FileExists("c:\\archivos de programa\\wxformbuilder\\wxformbuilder.exe"))
			out = config->Files.wxfb_command="c:\\archivos de programa\\wxformbuilder\\wxformbuilder.exe";
		else if (wxFileName::FileExists("c:\\Program Files\\wxformbuilder\\wxformbuilder.exe"))
			out = config->Files.wxfb_command="c:\\Program Files\\wxformbuilder\\wxformbuilder.exe";
		else if (wxFileName::FileExists("c:\\Program Files (x86)\\wxformbuilder\\wxformbuilder.exe"))
			out = config->Files.wxfb_command="c:\\Program Files (x86)\\wxformbuilder\\wxformbuilder.exe";
	}
#else
	if (config->Files.wxfb_command.IsEmpty()) config->Files.wxfb_command = "wxformbuilder";
#endif
	config->Init.wxfb_seen = out.Len();
	if (config->Init.wxfb_seen) return true;
	
	// si no estaba
	wxString message = LANG(PROJMNGR_WXFB_NOT_FOUND,"El proyecto utiliza wxFormBuilder, pero este software\n"
							"no se encuentra correctamente instalado/configurado en\n"
							"su PC. Si ya se encuentra instalado debe configurar su\n"
							"ubicación en el cuadro de \"Preferencias\".");
	if (wxfb_conf->autoupdate_projects && !wxfb_conf->autoupdate_projects_temp_disabled) {
		wxfb_conf->autoupdate_projects_temp_disabled = true;
		message += "\n\n";
		message += LANG(PROJMNGR_REGENERATING_ERROR_2,""
						"La actualización automática de estos proyectos\nse deshabilitará temporalmente.");
		
	}
	bool just_installed =
		CheckComplaintAndInstall(main_window, check_command, LANG(GENERAL_WARNING,"Advertencia"), 
								 message, "wxformbuilder", "http://wxformbuilder.org","wxformbuilder");
	// puede ser que lo acabe de instalar el CheckComplaintAndInstall con apt-get
	if (just_installed) {
		wxfb_conf->autoupdate_projects_temp_disabled = false;
		config->Init.wxfb_seen = true;
	}
	return config->Init.wxfb_seen;
}

bool ConfigManager::CheckDoxygenPresent() {
	if (config->Init.doxygen_seen) return true;
	wxString out;
#ifdef __WIN32__
	if (config->Files.doxygen_command.Len())
		out = mxUT::GetOutput(mxUT::Quotize(config->Files.doxygen_command)+" --version",true);
	if (!out.Len()) {
		if (wxFileName::FileExists("c:\\archivos de programa\\doxygen\\bin\\doxygen.exe"))
			out = config->Files.doxygen_command="c:\\archivos de programa\\doxygen\\bin\\doxygen.exe";
		else if (wxFileName::FileExists("c:\\Program Files\\doxygen\\bin\\doxygen.exe"))
			out = config->Files.doxygen_command="c:\\Program Files\\doxygen\\bin\\doxygen.exe";
		else if (wxFileName::FileExists("c:\\Program Files (x86)\\doxygen\\bin\\doxygen.exe"))
			out = config->Files.doxygen_command="c:\\Program Files (x86)\\doxygen\\bin\\doxygen.exe";
	}
#else
	if (!config->Files.doxygen_command.Len()) config->Files.doxygen_command="doxygen";
#endif
	config->Init.doxygen_seen = out.Len() || CheckComplaintAndInstall(
		main_window,
		mxUT::Quotize(config->Files.doxygen_command)+" --version",
		LANG(GENERAL_WARNING,"Advertencia"),
		LANG(MAINW_DOXYGEN_MISSING,"Doxygen no se encuentra correctamente instalado/configurado\n"
								   "en su pc. Si ya se encuentra instalado,debe configurar su\n"
								   "ubiciación en el cuadro de \"Preferencias\"."),
		"doxygen","http://www.doxygen.org","doxygen");
	return config->Init.doxygen_seen;
}

#ifndef __WIN32__
bool ConfigManager::CheckValgrindPresent() {
	if (config->Init.valgrind_seen) return true;
	if (!config->Files.valgrind_command.Len()) config->Files.valgrind_command="valgrind";
	config->Init.valgrind_seen = CheckComplaintAndInstall(
		main_window,
		mxUT::Quotize(config->Files.valgrind_command)+" --version",
		LANG(GENERAL_WARNING,"Advertencia"),
		LANG(MAINW_VALGRIND_MISSING,""
			 "Valgrind no se encuentra correctamente instalado/configurado\n"
			 "en su pc. Si ya se encuentra instalado deve configurar su\n"
			 "ubiciacion en la pestaña en el cuadro de \"Preferencias\"."),
		"valgrind","http://valgrind.org","valgrind");
	return config->Init.valgrind_seen;
}
#endif

bool ConfigManager::CheckCppCheckPresent() {
	if (config->Init.cppcheck_seen) return true;
	wxString out;
#ifdef __WIN32__
	if (config->Files.cppcheck_command.Len())
		out = mxUT::GetOutput(mxUT::Quotize(config->Files.cppcheck_command)+" --version",true);
	if (!out.Len()) {
		if (wxFileName::FileExists("c:\\archivos de programa\\cppcheck\\cppcheck.exe"))
			out=config->Files.cppcheck_command="c:\\archivos de programa\\cppcheck\\cppcheck.exe";
		else if (wxFileName::FileExists("c:\\Program Files\\cppcheck\\cppcheck.exe"))
			out=config->Files.cppcheck_command="c:\\Program Files\\cppcheck\\cppcheck.exe";
		else if (wxFileName::FileExists("c:\\Program Files (x86)\\cppcheck\\cppcheck.exe"))
			out=config->Files.cppcheck_command="c:\\Program Files (x86)\\cppcheck\\cppcheck.exe";
	}
#else
	if (!config->Files.cppcheck_command.Len()) config->Files.cppcheck_command="cppcheck";
#endif
	config->Init.cppcheck_seen = out.Len() || CheckComplaintAndInstall(
		main_window, 
		mxUT::Quotize(config->Files.cppcheck_command)+" --version",
		LANG(GENERAL_WARNING,"Advertencia"),
		LANG(MAINW_CPPCHECK_MISSING,"CppCheck no se encuentra correctamente instalado/configurado\n"
									"en su pc. Si ya se encuentra instalado debe configurar su\n"
									"ubiciación en el cuadro de \"Preferencias\"."),
		"cppcheck","http://cppcheck.sourceforge.net","cppcheck");
	return config->Init.cppcheck_seen;
}

void ConfigManager::RecalcStuff ( ) {
	// setup some required paths
	temp_dir = DIR_PLUS_FILE(zinjai_dir,Files.temp_dir);
	EnsurePathExists(temp_dir);
	if (zinjai_dir.EndsWith("\\")||zinjai_dir.EndsWith("/")) zinjai_dir.RemoveLast();
	if (temp_dir.EndsWith("\\")||temp_dir.EndsWith("/")) temp_dir.RemoveLast();
	// poner el idioma del compilador en castellano
	if (config->Init.lang_es) {
		wxSetEnv("LANG","es_ES.ISO8859-1");
//		wxSetEnv("LANGUAGE","es_ES");
	} else {
		wxSetEnv("LANG","en_US.ISO8859-1");
//		wxSetEnv("LANGUAGE","en_US");
	}
}

void ConfigManager::FinishLoading ( ) {
	
	// load language translations
	
//#ifndef __APPLE__
	if (Init.language_file!="spanish") 
//#endif
	{
		DEBUG_INFO("Loading language...");
		try_to_load_language();
	}
	
	// load syntax highlighting colors' scheme
	DEBUG_INFO("Loading colours...");
	color_theme::Initialize();
	if (Init.colour_theme.IsEmpty()) g_ctheme->Load(DIR_PLUS_FILE(config_dir,"colours.zcs"));
	else g_ctheme->Load(mxUT::WichOne(Init.colour_theme,"colours",true));
	
	// apply complement's patches to current config
	DEBUG_INFO("Looking for patchs from complements...");
	ApplyPatchsFromComplements();
	
	// check if extern tools are present and set some paths
	DEBUG_INFO("Loding toolchains...");
	Toolchain::LoadToolchains();
	DoInitialChecks(); 
	RecalcStuff();
	
	// create regular menus and toolbars' data
	DEBUG_INFO("Creating menues...");
	menu_data = new MenusAndToolsConfig();
	if (s_delayed_config_lines) { // old way
		for(unsigned int i=0;i<s_delayed_config_lines->toolbars_keys.GetCount();i++)
			menu_data->ParseToolbarConfigLine(s_delayed_config_lines->toolbars_keys[i],s_delayed_config_lines->toolbars_values[i]); 
		delete s_delayed_config_lines; s_delayed_config_lines=nullptr;
	} else { // new way
		menu_data->LoadShortcutsSettings(DIR_PLUS_FILE(config_dir,"shortcuts.zsc"));
		menu_data->LoadToolbarsSettings(DIR_PLUS_FILE(config_dir,"toolbar.ztb"));
	}
	if (Init.version<20141030) { 
		menu_data->GetToolbarPosition(MenusAndToolsConfig::tbDEBUG)="t3";
		menu_data->GetToolbarPosition(MenusAndToolsConfig::tbSTATUS)="t3";
		menu_data->GetToolbarPosition(MenusAndToolsConfig::tbPROJECT)="T1";
	}
#if defined(__APPLE__) && defined(__STC_ZASKAR)
	DEBUG_INFO("Fixing mac stuff...");
	if (Init.mac_stc_zflags==-1) {
		Init.mac_stc_zflags = 0;
		if (mxMessageDialog(nullptr,LANG(CONFIG_MAC_DEADKEYS_PROBLEM_QUESTION,""
						"¿Utiliza un teclado en español donde '[' y '{' se ingresan con\n"
						"AltGr y las teclas que están a la derecha de las teclas 'P' y 'Ñ'?\n"
						"\n"
						"Nota: si más tarde detecta problemas con estas teclas puede volver\n"
						"a cambiar esta configuación desde el cuadro de preferencias.\n"
					)).IconQuestion().ButtonsYesNo().Run().yes) 
		{
			Init.mac_stc_zflags = ZF_FIXDEADKEYS_ESISO;
		}
	}
	wxSTC_SetZaskarsFlags(Init.mac_stc_zflags);
#endif
	
	DEBUG_INFO("Initializing error recovery system...");
	// initialize error recovery system
	er_init(temp_dir.char_str());
	
}

bool ConfigManager::Initialize(const wxString & a_path) {
	config = new ConfigManager(a_path);
	bool first_time = !config->Load();
	if (first_time) {
		config->SetDefaultInspectionsImprovingTemplates(0);
	}
	return first_time;
}


void ConfigManager::AddInspectionImprovingTemplate(const wxString &from, const wxString &to, bool replace) {
	int pos = Debug.inspection_improving_template_from.Index(from);
	if (pos!=wxNOT_FOUND) {
		if (replace)
			Debug.inspection_improving_template_to[pos] = to;
		return;
	}
	Debug.inspection_improving_template_from.Add(from);
	Debug.inspection_improving_template_to.Add(to);
}

void ConfigManager::SetDefaultInspectionsImprovingTemplates (int version) {
	if (version<20150226) {
		AddInspectionImprovingTemplate("std::vector<${T}, std::allocator<${T}> >",">pvector ${EXP}");
		AddInspectionImprovingTemplate("std::stack<${T}, std::deque<${T}, std::allocator<${T}> > >",">pstack ${EXP}");
		AddInspectionImprovingTemplate("std::deque<${T}, std::allocator<${T}> >",">pdeque ${EXP}");
		AddInspectionImprovingTemplate("std::queue<${T}, std::deque<${T}, std::allocator<${T}> > >",">pqueue ${EXP}");
		AddInspectionImprovingTemplate("std::priority_queue<${T}, std::vector<${T}, std::allocator<${T}> >, ${C} >",">ppqueue ${EXP}");
		AddInspectionImprovingTemplate("std::bitset<${N}>",">pbitset ${EXP}");
	}
	if (version<20150820) {
		AddInspectionImprovingTemplate("std::set<${T}, ${C}, std::allocator<${T}> >",">pset ${EXP} \'${T}\'",true);
		AddInspectionImprovingTemplate("std::list<${T}, std::allocator<${T}> >",">plist ${EXP} \'${T}\'",true);
		AddInspectionImprovingTemplate("std::map<${T1}, ${T2}, ${C}, std::allocator<${P}> >",">pmap ${EXP} \'${T1}\' \'${T2}\'",true);
		AddInspectionImprovingTemplate("std::list<${T}, std::allocator<${T}> >::iterator","*((${T}*)(${EXP}._M_node+1)) @1",true); // list<T>::iterator exp
		AddInspectionImprovingTemplate("std::_List_iterator<${T}>","*((${T}*)(${EXP}._M_node+1)) @1",true); // auto exp = list<T>().begin();
		AddInspectionImprovingTemplate("std::string","(${EXP})._M_dataplus._M_p",true); // string exp
		AddInspectionImprovingTemplate("std::basic_string<char, std::char_traits<char>, std::allocator<char> >","(${EXP})._M_dataplus._M_p",true); // list<string>::iterator it, exp = *it
	}
	if (version<20161017) {
		AddInspectionImprovingTemplate("std::${NS}::list<${T}, std::allocator<${T}> >::iterator","*((${T}*)(${EXP}._M_node+1)) @1",true); // list<T>::iterator exp
		AddInspectionImprovingTemplate("std::${NS}::list<${T}, std::allocator<${T}> >",">plist ${EXP} \'${T}\'",true);
	}
}

bool ConfigManager::CheckComplaintAndInstall(wxWindow *parent, const wxString &check_command, const wxString &what,
											 const wxString &error_msg, const wxString &pkgname, 
											 const wxString &website, const wxString &preferences_field) 
{
	wxString check_output = mxUT::GetOutput(check_command,true,false);
	if (check_output.Len() && !check_output.StartsWith("execvp")) return true; // si anda, ya esta instalada
	return ComplaintAndInstall(parent,check_command,what,error_msg,pkgname,website,preferences_field);
}


bool ConfigManager::ComplaintAndInstall(wxWindow *parent, const wxString &check_command, const wxString &what,
											 const wxString &error_msg, const wxString &pkgname, 
											 const wxString &website, const wxString &preferences_field) 
{
	wxString apt_message = GetTryToInstallCheckboxMessage(); // ver si tenemos apt-get
#ifdef __APPLE__
	if (pkgname=="gdb") apt_message = LANG(CONFIG_APTGET_BUILD_ESSENTIAL,"Intentar instalar ahora");
#endif
	wxString web_msg = website.Len()?(LANG1(CONFIG_GOTO_PACKAGE_WEBSITE,"Abrir sitio web (<{1}>)",website)):"";
	wxString pref_msg = preferences_field.Len()?(LANG(CONFIG_OPEN_PREFERENCES,"Abrir el cuadro de Preferencias")):"";
	
	mxMessageDialog::mdAns ans = mxMessageDialog(parent,error_msg).Title(what).IconWarning()
		.Check1(pref_msg,apt_message.IsEmpty()).Check2(apt_message.IsEmpty()?web_msg:apt_message,true).Run(); // informar/preguntar
	if (ans.check2 && !apt_message.IsEmpty()) { // si había apt-get, 
		TryToInstallWithAptGet(parent,what,pkgname); // intentar instalar
		wxString check_output = check_command.IsEmpty()?"":mxUT::GetOutput(check_command,true,false);
		if (check_output.Len() && !check_output.StartsWith("execvp")) // si anda, ya esta instalada
			return true; 
		// si falló apt-get, avisar e intentar abrir el sitio web de descarga
#ifdef __APPLE__
		if (pkgname=="gdb") return false;
#endif
		ans = mxMessageDialog(parent,LANG(CONFIG_APTGET_FAILED,"Falló la instalación automática."))
				.Title(what).IconWarning().Check1(pref_msg,true).Check2(web_msg,true).Run();
	}
	if (ans.check2) { // si no había apt-get, o si fallo (por eso repito la pregunta del if), abrir el sitio web
		mxUT::OpenInBrowser(website);
	}
	if (ans.check1) { // si no había apt-get, o si fallo (por eso repito la pregunta del if), abrir el sitio web
		mxPreferenceWindow::ShowUp()->SetPathsPage(preferences_field);
	}
	return false;
}

void ConfigManager::TryToInstallWithAptGet (wxWindow * parent, const wxString & what, const wxString & pkgname) {
#ifdef __APPLE__
	if (pkgname=="gdb") {
		mxHelpWindow *helpw = mxHelpWindow::ShowHelp("gdb_on_mac.html");
		helpw->SetAlwaysOnTop(true); // esto parece no funcionar, por eso el reacomodo que sigue
		int w = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
		int h = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
		helpw->Move(w/2,0); helpw->SetSize(w/2,h); helpw->HideIndexTree();
		config->Init.pos_x = config->Init.pos_y = 0; config->Init.maximized=false;
		config->Init.size_x = w/2; config->Init.size_y = h;
		return;
	}
#endif
	mxMessageDialog(parent,
					LANG(CONFIG_ABOUT_TO_APTGET,"A continuación se intentará instalar el software faltante en una nueva\n"
												"terminal. Podría requerir ingresar la contraseña del administrador.\n"
												"ZinjaI continuará cuando se cierre dicha terminal.")
					).Title(what).Run();
	wxExecute(mxUT::GetCommandForRunningInTerminal(
		wxString("ZinjaI - sudo apt-get install ")+pkgname,
		wxString("sudo apt-get install ")+pkgname ),wxEXEC_SYNC);
}

wxString ConfigManager::GetTryToInstallCheckboxMessage ( ) {
#ifdef __linux__
	if (mxUT::GetOutput("apt-get --version").Len())	
		return LANG(CONFIG_APTGET_BUILD_ESSENTIAL,"Intentar instalar ahora");
#endif
	return "";
}

/**
* @brife Applies some changes to current configuration, changes that came from 
*        a complement's installation
*
* Read the other overload of ApplyPatchsFromComplements first. That one finds out
* wich files to parse, and calls this one for each one.
*
* The file can contains the following lines:
*   - To change default template for opening/creating simple programs to template xxxxx:
*      - set_default_template=xxxxx
*      - set_cpp_template=xxxxx
*      - set_c_template=xxxxx
*   - To activate a new autocompletion index xxxxx:
*      - add_to_autocomp_indexes=xxxxx
*   - To add a new templato for automatic expression improvement while debugging:
*       - add_inspection_improving_template=xxxxx
*   - To add a nue general custom tool:
*       - add_custom_tool
*       - custom_tool_xxxxx=yyyyy 
*       - custom_tool_xxxxx=yyyyy
*       - custom_tool_xxxxx=yyyyy
*       - ...
*       - Note: the first line selects a free spot for the tool, the following
*         lines will process lines "xxxxx_z=yyyyy" the same way the real config 
*         file does where z in the added "_z" is the number of the free spot.
**/

void ConfigManager::ApplyPatchsFromComplements (wxString filename) {
	wxTextFile fil(filename); if (!fil.Exists()) return; fil.Open();
	int custom_tool_index = -1;
	for ( wxString str = fil.GetFirstLine(); !fil.Eof(); str = fil.GetNextLine() ) {
		if (str.StartsWith("set_default_template=")) {
			config->Files.default_template = str.AfterFirst('=');
		} else if (str.StartsWith("set_cpp_template=")) {
			config->Files.cpp_template = str.AfterFirst('=');
		} else if (str.StartsWith("set_c_template=")) {
			config->Files.c_template = str.AfterFirst('=');
		} else if (str.StartsWith("add_to_autocomp_indexes=")) {
			config->Help.autocomp_indexes+=wxString(",")+str.AfterFirst('=');
		} else if (str.StartsWith("add_inspection_improving_template=")) {
			wxString value = str.AfterFirst('=');
			Debug.inspection_improving_template_from.Add(value.BeforeFirst('|'));
			Debug.inspection_improving_template_to.Add(value.AfterFirst('|'));
		} else if (str=="add_custom_tool") {
			custom_tool_index = custom_tools.GetFreeSpot();
		} else if (str.StartsWith("custom_tool_")) {
			str = str.Mid(wxString("custom_tool_").Len());
			wxString key = str.BeforeFirst('='), value = str.AfterFirst('=');
			if (custom_tool_index!=-1)
				custom_tools.ParseConfigLine(key+(wxString("_")<<custom_tool_index),value);
		}
	}
}

static wxString stFill(int value, unsigned int len) {
	wxString s;
	s<<value;
	while (s.Len()<len)
		s = wxString("0")+s;
	return s;
}

static wxString stDateTime2String(const wxDateTime &d) {
	return wxString()
		<<stFill(d.GetYear(),4)
		<<stFill(d.GetMonth(),2)
		<<stFill(d.GetDay(),2)
		<<stFill(d.GetHour(),2)
		<<stFill(d.GetMinute(),2)
		<<stFill(d.GetSecond(),2)
		;
}

/**
* @brife Applies some changes to current configuration, changes that came from complements
*
* A complement can add a file to zinjai/complements/config with a few instructions
* to modify zinjai's configuration. The first time zinjai runs after the complement's
* installation will apply those changes. Te mechanism to avoid applying them more than
* once is simply to compare config file date with dates from files in 
* zinjai/complements/config and process only files that are newer than config.
*
* See the other overload of ApplyPatchsFromComplements for format details
**/
void ConfigManager::ApplyPatchsFromComplements ( ) {
	wxString dir = DIR_PLUS_FILE(DIR_PLUS_FILE(config->zinjai_dir,"complements"),"config");
	wxArrayString files;
	if (!mxUT::GetFilesFromDir(files,dir,true)) return;
	for(unsigned int i=0;i<files.Count();i++) { 
		wxString fullname = DIR_PLUS_FILE(dir,files[i]);
		if (stDateTime2String(wxFileName(fullname).GetModificationTime())>Init.complements_timestamp)
			ApplyPatchsFromComplements(fullname);
	}
	Init.complements_timestamp = stDateTime2String(wxDateTime::Now());
}


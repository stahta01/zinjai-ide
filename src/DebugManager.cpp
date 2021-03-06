#include <algorithm>
#include <vector>
#include <wx/process.h>
#include <wx/stream.h>
#include <wx/msgdlg.h>
#include "DebugManager.h"
#include "mxCompiler.h"
#include "ConfigManager.h"
#include "mxMainWindow.h"
#include "mxMessageDialog.h"
#include "mxUtils.h"
#include "mxSource.h"
#include "ProjectManager.h"
#include "ids.h"
#include "mxBacktraceGrid.h"
#include "Parser.h"
#include "mxBreakList.h"
#include "mxInspectionMatrix.h"
#include "mxArgumentsDialog.h"
#include "mxApplication.h"
#include "Language.h"
#include "mxOSD.h"
#include "mxThreadGrid.h"
#include "Inspection.h"
#include "DebugPatcher.h"
#include "MenusAndToolsConfig.h"
#include "osdep.h"
#include "Cpp11.h"
#include "gdbParser.h"
#include "ZLog.h"
#include "asserts.h"
using namespace std;

//#define BACKTRACE_MACRO "define zframeaddress\nset $fi=0\nwhile $fi<$arg0\nprintf \"*zframe-%u={\",$fi\ninfo frame $fi\nprintf \"}\\n\"\nset $fi=$fi+1\nend\nend"

DebugManager *debug = nullptr;

DebuggerTalkLogger* DebuggerTalkLogger::the_logger = nullptr;

DebugManager::DebugManager() {
	backtrace_rows_count = 0;
	on_pause_action = nullptr;
	signal_handlers_state=nullptr;
	backtrace_shows_args=true;
	status = DBGST_NULL;
	current_handle = -1;
	prev_stack.clear();
	current_stack.clear();
//	inspections_count = 0;
//	backtrace_visible = false;
	threadlist_visible = false;
	waiting = debugging = running = false;
	process = nullptr;
	input = nullptr;
	output = nullptr;
	gdb_pid = 0;
	notitle_source = current_source = nullptr;
#ifndef __WIN32__
	tty_pid = 0;
	tty_process = nullptr;
#endif
	auto_step = false;
}

DebugManager::~DebugManager() {
}

bool DebugManager::Start(bool update) {
	_DBG_LOG_CALL(Open());
	if (update && project->PrepareForBuilding()) { // ver si hay que recompilar antes
		_LAMBDA_0( lmbDebugProject, { if (project) debug->Start(false); } );
		compiler->BuildOrRunProject(true,new lmbDebugProject());
		return false;
	}
	
	SetStateText(LANG(DEBUG_STATUS_STARTING,"Iniciando depuracion..."),true);
	project->Debug(); // lanzar el depurador con los parametros del proyecto
	project->SetBreakpoints(); // setear los puntos de interrupcion del proyecto
	if (!Run() && has_symbols) {
#ifdef __WIN32__
		if (wxFileName(DIR_PLUS_FILE(project->path,project->executable_name)).GetPath().Contains(' '))
			mxMessageDialog(main_window,LANG(DEBUG_ERROR_STARTING_SPACES, ""
											 "Error al iniciar el proceso. Puede intentar mover el\n"
											 "ejecutable o el proyecto a una ruta sin espacios."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconInfo().Run();
		else 
#endif
			mxMessageDialog(main_window,LANG(DEBUG_ERROR_STARTING,"Error al iniciar el proceso."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconInfo().Run();
		return false;
	}
	return true;
}

bool DebugManager::Start(mxSource *source) {
	_DBG_LOG_CALL(Open());
	if (source) {
		SetStateText(LANG(DEBUG_STATUS_STARTING,"Iniciando depuracion..."),true);
		wxString args;
		if (source->config_running.always_ask_args) {
			int res = mxArgumentsDialog(main_window,source->GetPath(true),source->exec_args,source->working_folder.GetFullPath()).ShowModal();
			if (res==0) return 0;
			if (res&AD_ARGS) {
				source->exec_args = mxArgumentsDialog::last_arguments;
				source->working_folder = mxArgumentsDialog::last_workdir;
				args = source->exec_args ;
			}
			if (res&AD_REMEMBER) {
				source->config_running.always_ask_args=false;
				if (res&AD_EMPTY) source->exec_args ="";
			}
		} else if (source->exec_args[0]!='\0')
			args = source->exec_args;	

		compiler->last_caption = source->page_text;
		compiler->last_runned = current_source = source;
		if (Start(source->working_folder.GetFullPath(),source->GetBinaryFileName().GetFullPath(),args,true,source->config_running.wait_for_key?WKEY_ALWAYS:WKEY_NEVER)) {
			int cuantos_sources = main_window->notebook_sources->GetPageCount();
			for (int i=0;i<cuantos_sources;i++) {
				mxSource *src = (mxSource*)(main_window->notebook_sources->GetPage(i));
				if (!src->sin_titulo && source!=src)
					SetBreakPoints(src,true);
			}
			SetBreakPoints(source);
			if (source->sin_titulo) notitle_source=source;
			if (!Run()) {
#ifdef __WIN32__
				if (source->GetBinaryFileName().GetFullPath().Contains(' '))
					mxMessageDialog(main_window,LANG(DEBUG_ERROR_STARTING_SPACES,""
													 "Error al iniciar el proceso. Puede intentar mover\n"
													 "el ejecutable o el proyecto a una ruta sin espacios."))
						.Title(LANG(GENERAL_ERROR,"Error")).IconInfo().Run();
				else
#endif
					mxMessageDialog(main_window,LANG(DEBUG_ERROR_STARTING,"Error al iniciar el proceso."))
						.Title(LANG(GENERAL_ERROR,"Error")).IconInfo().Run();
				return false;
			}
			return true;
		}
	}
	return false;
}

bool DebugManager::Start(wxString workdir, wxString exe, wxString args, bool show_console, int wait_for_key) {
	mxOSDGuard osd(main_window,project?LANG(OSD_STARTING_DEBUGGER,"Iniciando depuracion..."):"");
	ResetDebuggingStuff(); 
	debug_patcher->Init(exe);
#ifndef __WIN32__
	wxString tty_cmd, tty_file(DIR_PLUS_FILE(config->temp_dir,_T("tty.id")));
	if (show_console) {
		if (wxFileName::FileExists(tty_file))
			wxRemoveFile(tty_file);
		tty_cmd<<config->Files.terminal_command<<" "<<config->Files.runner_command<<_T(" -tty ")<<tty_file;
		tty_cmd.Replace("${TITLE}",LANG(GENERA_CONSOLE_CAPTION,"ZinjaI - Consola de Ejecucion")); // NO USAR ACENTOS, PUEDE ROMER EL X!!!! (me daba un segfault en la libICE al poner el � en Ejeuci�n)
		wait_for_key_policy = wait_for_key;
	//	mxUT::ParameterReplace(tty_cmd,_T("${ZINJAI_DIR}"),wxGetCwd());
		tty_process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_DEBUG);
		ZLINF2("DebugManager","Start, Launching tty process, cmd: "<<tty_cmd);
		tty_pid = wxExecute(tty_cmd,wxEXEC_ASYNC,tty_process);
		ZLINF2("DebugManager","Start, Launching tty process, pid: "<<tty_pid);
	} else {
		tty_pid=0; tty_process=nullptr;
	}
#endif
	wxString command(config->Files.debugger_command);
	command<<_T(" -quiet -nx -interpreter=mi");
	if (config->Debug.readnow) command<<_T(" --readnow");
#ifndef __WIN32__
	if (show_console) {
		gdb_pid=0;
		wxDateTime t0=wxDateTime::Now(); // algunas terminales no esperan a que lo de adentro se ejecute para devolver el control (mac por ejemplo)
		while ((tty_pid || (wxDateTime::Now()-t0).GetSeconds()<10) && !wxFileName::FileExists(tty_file))
			{ wxYield(); wxMilliSleep(100); }
		if (!tty_pid && !wxFileName::FileExists(tty_file)) {
			debugging = false;
			mxMessageDialog(main_window,LANG(DEBUG_ERROR_WITH_TERMINAL,""
											 "Ha ocurrido un error al iniciar la terminal para la ejecucion.\n"
											 "Compruebe que el campo \"Comando del terminal\" de la pesta�a\n"
											 "\"Rutas 2\" del cuadro de \"Preferencias\" sea correcto."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			main_window->SetCompilingStatus(LANG(DEBUG_STATUS_INIT_ERROR,"Error al iniciar depuracion"));
			return false;
		}
		while (true) {
			wxTextFile ftty(tty_file);
			ftty.Open();
			tty_dev = ftty.GetFirstLine();
			if (tty_dev.Len()) {
				command<<_T(" -tty=")<<tty_dev;
				ftty.Close();
				break;
			}
			ftty.Close();
			{ wxYield(); wxMilliSleep(100); }
		}
	} else
		command<<_T(" -tty=/dev/null");
#endif
//	if (args.Len())
//		command<<_T(" --args \"")<<exe<<"\" "<</*mxUT::EscapeString(*/args/*)*/;	
//	else
		command<<" \""<<exe<<"\"";	
	process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_DEBUG);
	process->Redirect();
	
#ifndef __WIN32__
	if (project && project->active_configuration->exec_method==EMETHOD_INIT) {
		command=wxString()<<"/bin/sh -c "<<mxUT::SingleQuotes(wxString()
			<<". "<<DIR_PLUS_FILE(project->path,project->active_configuration->exec_script)<<" &>/dev/null; "<<command);
	}
#endif
	
	ZLINF("DebugManager","Debug starting");
	ZLINF2("DebugManager","cmd: "<<command);
	gdb_pid = wxExecute(command,wxEXEC_ASYNC,process);
	ZLINF2("DebugManager","gdb_pid: "<<gdb_pid);
	if (gdb_pid>0) {
		input = process->GetInputStream();
		output = process->GetOutputStream();
		GDBAnswer &hello = WaitAnswer(true); // true para que no salte el watchdog, la carga de libs puede tardar bastante 
		if (hello.stream.Find("no debugging symbols found")!=wxNOT_FOUND) {
			mxMessageDialog(main_window,LANG(DEBUG_NO_SYMBOLS,""
											 "El ejecutable que se intenta depurar no contiene informacion de depuracion.\n"
											 "Compruebe que en las opciones de depuracion este activada la informacion de\n"
											 "depuracion, verifique que no este seleccionada la opcion \"stripear el ejecutable\"\n"
											 "en las opciones de enlazado, y recompile el proyecto si es necesario (Ejecucion->Limpiar\n"
											 "y luego Ejecucion->Compilar)."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			SendCommandNW(_T("-gdb-exit"));
			debugging = false; has_symbols=false;
			main_window->SetCompilingStatus(LANG(DEBUG_STATUS_INIT_ERROR,"Error al iniciar depuracion"));
			return false;
		}
		Start_ConfigureGdb();
		// configure debugger
#ifdef __WIN32__
		SendCommand(_T("-gdb-set new-console on"));
#endif
		if (args.Len()) SendCommand("set args ",args);
//		SendCommand(_T(BACKTRACE_MACRO));
		SendCommand(wxString(_T("-environment-cd "))<<mxUT::EscapeString(workdir,true));
		main_window->PrepareGuiForDebugging(gui_is_prepared=true);
		return true;
	} else  {
			mxMessageDialog(main_window,wxString(LANG(DEBUG_ERROR_STATING_GDB,"Ha ocurrido un error al ejecutar el depurador."))
#ifndef __WIN32__
							<<"\n"<<LANG(DEBUG_ERROR_STARTING_GDB_LINUX,"Si el depurador (gdb) no se encuentra instalado\n"
							"en su systema debe instalarlo con el gestor de\n"
							"paquetes que corresponda a su distribucion\n"
							"(apt-get, yum, yast, installpkg, etc.)")
#endif
				).Title("Error al iniciar depurador").IconError().Run();
			main_window->PrepareGuiForDebugging(false);
	}
	gdb_pid=0;
	debugging = false;
	main_window->SetCompilingStatus(LANG(DEBUG_STATUS_INIT_ERROR,"Error al iniciar depuracion"));
	return false;
}


/**
* Resetea los atributos utilizados durante la depuraci�n (como lista de inspecciones,
* banderas de estado, buffer para comunicaci�n con el proceso de gdb, etc).
* Es llamada por Start y LoadCoreDump.
**/

void DebugManager::ResetDebuggingStuff() {
	status=DBGST_STARTING;
#ifndef __WIN32__
	tty_pid = 0; tty_process=nullptr; tty_dev.Clear();
#endif
//	black_list.Clear(); stepping_in=false;
//	mxUT::Split(config->Debug.blacklist,black_list,true,false);
	gui_is_prepared = false;
	
	// setear en -1 todos los ids de los pts de todos interrupcion, para evitar confusiones con depuraciones anteriores
	GlobalListIterator<BreakPointInfo*> bpi=BreakPointInfo::GetGlobalIterator();
	while (bpi.IsValid()) { bpi->gdb_id=-1; bpi.Next(); }
	_DBG_LOG_CALL(Log("\n<<<NEW SESSION>>>\n"));
	m_gdb_buffer.Reset();
	debugging = true;
	child_pid = gdb_pid = 0;
	input = nullptr;
	output = nullptr;
	step_by_asm_instruction = false;
	recording_for_reverse=inverse_exec=false;
	main_window->ClearDebugLog();
	has_symbols=true;
	should_pause=false;
	debug_patcher = new DebugPatcher();
	current_thread_id = -1; current_frame_id = -1;
	if (on_pause_action) delete on_pause_action;
	on_pause_action = nullptr;
	watchpoints.clear();
}


bool DebugManager::SpecialStart(mxSource *source, const wxString &gdb_command, const wxString &status_message, bool should_continue) {
	_DBG_LOG_CALL(Open());
	mxOSDGuard osd(main_window,LANG(OSD_STARTING_DEBUGGER,"Iniciando depuracion..."));
	ResetDebuggingStuff();
	wxString exe = source?source->GetBinaryFileName().GetFullPath():DIR_PLUS_FILE(project->path,project->active_configuration->output_file);
	wxString command(config->Files.debugger_command);
	command<<_T(" -quiet -nx -interpreter=mi");
	if (config->Debug.readnow)
		command<<_T(" --readnow");
	command<<" "<<mxUT::Quotize(exe);	
	process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_DEBUG);
	process->Redirect();
	gdb_pid = wxExecute(command,wxEXEC_ASYNC,process);
	if (gdb_pid>0) {
		input = process->GetInputStream();
		output = process->GetOutputStream();
		GDBAnswer &hello = WaitAnswer();
		if (hello.stream.Find("no debugging symbols found")!=wxNOT_FOUND) {
			mxMessageDialog(main_window,LANG(DEBUG_NO_SYMBOLS,""
											 "El ejecutable que se intenta depurar no contiene informacion de depuracion.\n"
											 "Compruebe que en las opciones de depuracion este activada la informacion de\n"
											 "depuracion, verifique que no este seleccionada la opcion \"stripear el ejecutable\"\n"
											 "en las opciones de enlazado, y recompile el proyecto si es necesario (Ejecucion->Limpiar\n"
											 "y luego Ejecucion->Compilar)."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			SendCommandNW("-gdb-exit");
			debugging = false;
			return false;
		}
		// configure debugger
		Start_ConfigureGdb();
//		SendCommand(_T(BACKTRACE_MACRO));
		main_window->PrepareGuiForDebugging(gui_is_prepared=true);
		// mostrar el backtrace y marcar el punto donde corto
		GDBAnswer &ans = SendCommand(gdb_command);
		if (ans.result.StartsWith("^error,")) {
			ZLINF2("DebugManager","SpecialStart, Error on fisrt cmd: "<<gdb_command); 
			ZLINF("DebugManager","SpecialStart, Error on fisrt cmd, ans contains ^error");
			mxMessageDialog(main_window,wxString(LANG(DEBUG_SPECIAL_START_FAILED,"Ha ocurrido un error al iniciar la depuraci�n:"))+debug->GetValueFromAns(ans.result,"msg",true,true))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
			main_window->SetCompilingStatus(LANG(DEBUG_STATUS_INIT_ERROR,"Error al iniciar depuracion"));
			Stop(); return false;
		}
		while (!ans.async.Contains("*stopped")) /*ans=*/WaitAnswer(); /*WaitAnswer siempre pone el resultado en DebuggerInspection::last_answer*/
		SetStateText(status_message);
		UpdateBacktrace(true,true);
		DebuggerInspection::OnDebugPause();
//		long line;
//		main_window->backtrace_ctrl->GetCellValue(0,BG_COL_LINE).ToLong(&line);
//		wxString file = main_window->backtrace_ctrl->GetCellValue(0,BG_COL_FILE);
//		if (file.Len()) {
//			debug->MarkCurrentPoint(file,line,mxSTC_MARK_STOP);
//			debug->SelectFrame(-1,1);
//		}
		if (project) 
			project->SetBreakpoints();
		else {
			int cuantos_sources = main_window->notebook_sources->GetPageCount();
			for (int i=0;i<cuantos_sources;i++) {
				mxSource *src = (mxSource*)(main_window->notebook_sources->GetPage(i));
				if (!src->sin_titulo && source!=src)
					SetBreakPoints(src);
			}
			SetBreakPoints(source);
		}
		if (should_continue) { osd.Hide(); Continue(); }
		return true;
	}
	debugging = false;
	gdb_pid = 0;
	return false;	
}
	

/**
* Inicia el depurador cargando un volcado de memoria desde un archivo en lugar
* de ejecutar el programa. Para generar estos dumps en GNU/Linux, ejecutar
* "ulimit -c unlimited" en el shell; luego, al ejecutar un programa, si este
* revienta por un segfault, genera un archivo core*. En esta funci�n, si se
* recibe un mxSource se asume que es un programa simple, sino (source==nullptr)
* que se trata de un proyecto. Esta opci�n no est� disponible en la versi�n
* para Windows.
* @param core_file la ruta completa al archivo core
* @param source puntero al mxSource si es un programa simple, nullptr si es un proyecto
* @retval true si se cargo correctamente
* @retval false si hay error
**/

bool DebugManager::LoadCoreDump(wxString core_file, mxSource *source) {
	_DBG_LOG_CALL(Open());
	
	mxOSDGuard osd(main_window,project?LANG(OSD_LOADING_CORE_DUMP,"Cargando volcado de memoria..."):"");
	
	ResetDebuggingStuff();
	wxString exe = source?source->GetBinaryFileName().GetFullPath():DIR_PLUS_FILE(project->path,project->active_configuration->output_file);
	wxString command(config->Files.debugger_command);
	command<<_T(" -quiet -nx -interpreter=mi");
	if (config->Debug.readnow)
		command<<_T(" --readnow");
	command<<" -c "<<mxUT::Quotize(core_file)<<" "<<mxUT::Quotize(exe);	
	process = new wxProcess(main_window->GetEventHandler(),mxPROCESS_DEBUG);
	process->Redirect();
	gdb_pid = wxExecute(command,wxEXEC_ASYNC,process);
	if (gdb_pid>0) {
		input = process->GetInputStream();
		output = process->GetOutputStream();
		/*wxString hello =*/ WaitAnswer();
		/// @todo: el mensaje puede aparecer por las bibliotecas, ver como diferenciar
//		if (hello.Find(_T("no debugging symbols found"))!=wxNOT_FOUND) {
//			mxMessageDialog(main_window,LANG(DEBUG_NO_SYMBOLS,"El ejecutable que se intenta depurar no contiene informacion de depuracion.\nCompruebe que en las opciones de depuracion que este activada la informacion de depuracion,\nverifique que no este seleccionada la opcion \"stripear el ejecutable\" en las opciones de enlazado,\n y recompile el proyecto si es necesario (Ejecucion->Limpiar y luego Ejecucion->Compilar).")).Title(LANG(GENERAL_ERROR,"Error").IconError().Run();
//			SendCommandNW(_T("-gdb-exit"));
//			debugging = false;
//			return false;
//		}
		// configure debugger
		Start_ConfigureGdb();
//		SendCommand(_T(BACKTRACE_MACRO));
		main_window->PrepareGuiForDebugging(gui_is_prepared=true);
		// mostrar el backtrace y marcar el punto donde corto
		UpdateBacktrace(true,true);
		DebuggerInspection::OnDebugPause();
//		long line;
//		main_window->backtrace_ctrl->GetCellValue(0,BG_COL_LINE).ToLong(&line);
//		wxString file = main_window->backtrace_ctrl->GetCellValue(0,BG_COL_FILE);
//		if (file.Len()) {
//			debug->MarkCurrentPoint(file,line,mxSTC_MARK_STOP);
//			debug->SelectFrame(-1,0);
//		}
		return true;
	}
	gdb_pid=0;
	debugging = false;
	return false;
}

bool DebugManager::Stop(bool waitkey) {
#ifndef __WIN32__
	if (status==DBGST_WAITINGKEY) {
		ZLINF2("DebugManager","Stop, Enviando SIGKILL a tty_pid:"<<tty_pid);
		process->Kill(tty_pid,wxSIGKILL); 
		status=DBGST_STOPPING; 
		return false; 
	}
#endif
	if (status==DBGST_STOPPING) return false; else status=DBGST_STOPPING;
	if (waiting || !debugging) {
#if defined(__APPLE__) || defined(__WIN32__)
		ZLINF2("DebugManager","Stop, Enviando SIGKILL a pid:"<<gdb_pid);
		process->Kill(gdb_pid,wxSIGKILL);
		return false;
#else
		Pause();
#endif
	}
	waiting=true;
	if (gdb_pid) {
		last_command=_T("-gdb-exit\n");
#ifndef __WIN32__
#warning VER DE MOSTRAR EL CODIGO DE SALIDA QUE DA GDB (buscar exit-code)
		if (waitkey && !tty_dev.IsEmpty()) {
			wxString cmd; 
			status = DBGST_WAITINGKEY; // to be able to kill it with Stop button
			cmd 
				<< "shell " 
				<< mxUT::Quotize(config->Files.runner_command)
				<< " -lang \"" << config->Init.language_file << "\""
				<< " -debug-end <> " 
				<< tty_dev << " >&0 2>&1";
			SendCommandNW(cmd);
		}
#endif
		SendCommandNW("-exec-abort");
		SendCommandNW("-gdb-exit");
	}
	return true;
}

bool DebugManager::Run() {
	if (waiting || !debugging) return false;
	GDBAnswer &ans = SendCommand("-exec-run");
	if (ans.result.StartsWith("^running")) {
		SetStateText(LANG(DEBUG_STATUS_RUNNING,"Ejecutando..."));
		HowDoesItRuns(true);
		return true;
	} else {
		ZLWAR2("DebugManager","Run -exec-run failed, ans="<<ans.result);
		return false;
	}
}

wxString DebugManager::HowDoesItRuns(bool raise_zinjai_window) {
	wxString retval;
#ifdef __WIN32__
	static wxString sep="\\",wrong_sep="/";
#else
	static wxString sep="/",wrong_sep="\\";
#endif
	SetStateText(LANG(DEBUG_STATUS_RUNNING,"Ejecutando..."));
	MarkCurrentPoint();
	
	while (true) { // para que vuelva a este punto cuando llega a un break point que no debe detener la ejecucion
		wxString ans = WaitAnswer(true).async; retval += last_answer.full;
		wxString state_text=LANG(DEBUG_STATUS_UNKNOWN,"Estado desconocido"); 
		if (!process || status==DBGST_STOPPING) return retval;
		int st_pos = ans.Find(_T("*stopped"));
		if (st_pos==wxNOT_FOUND) {
			ZLINF2("DebugManager","HowDoesItRuns, unknown state, answer: "<<ans);
			SetStateText(state_text);
			_DBG_LOG_CALL(Log(wxString()<<"ERROR RUNNING: "<<ans));
			return retval;
		}
		ans=ans.Mid(st_pos);
		wxString how = GetValueFromAns(ans,"reason",true);
		wxString thread_id = GetValueFromAns(ans,"thread-id",true);
		if (!thread_id.ToLong(&current_thread_id)) current_thread_id=-1;
		
#ifndef __WIN32__
		if (child_pid==0) FindOutChildPid(); // see Pause
#endif
		
#define _aux_continue SendCommand(_T("-exec-continue")); waiting=true; wxYield(); waiting=false; continue;
#define _aux_repeat_step SendCommand(stepping_in?"-exec-step":"-exec-next"); waiting=true; wxYield(); waiting=false; continue;
		bool should_continue=false; // cuando se pauso solo para colocar un brekapoint y seguir, esto indica que siga sin analizar la salida... puede ser how diga signal-received (lo normal) o que se haya pausado justo por un bp de los que solo actualizan la tabla de inspecciones
		
		if (on_pause_action) {// si se pauso solo para colocar un brekapoint o algo asi, hacerlo y setear banderas para que siga ejecutando
			on_pause_action->Run(); 
			delete on_pause_action; 
			on_pause_action=nullptr;
			should_pause=false; // la pausa no la gener� el usuario, sino que era solo para colocar el brakpoint
			should_continue=true;
		}
		
		BreakPointInfo *bpi=nullptr; int mark = 0;
		if (how=="breakpoint-hit") {
			wxString sbn = GetValueFromAns(ans,"bkptno",true);
			if (sbn.Len()) {
				long bn;
				if (sbn.ToLong(&bn)) {
					bpi=BreakPointInfo::FindFromNumber(bn,true);
					if (!should_pause && bpi && bpi->action==BPA_INSPECTIONS) {
						UpdateInspections();
						_aux_continue;
					}
				}
			}
			mark = mxSTC_MARK_EXECPOINT;
			state_text=LANG(DEBUG_STATUS_BREAK,"El programa alcanzo un punto de interrupcion");
		} else if (how==_T("watchpoint-trigger") || how==_T("access-watchpoint-trigger") || how==_T("read-watchpoint-trigger")) {
			mark = mxSTC_MARK_EXECPOINT;
			state_text=LANG(DEBUG_STATUS_WATCH,"El programa se interrumpio por un WatchPoint: ");
			wxString num = GetValueFromAns(ans.AfterFirst('{'),"wpnum",true);
			state_text << num << ": " << watchpoints[num];
		} else if (how==_T("watchpoint-scope")) {
			mark = mxSTC_MARK_EXECPOINT;
			state_text=LANG(DEBUG_STATUS_WATCH_OUT,"El WatchPoint ha dejado de ser valido: ");
			wxString num = GetValueFromAns(ans.AfterFirst('{'),"wpnum",true);
			state_text << num << ": " << watchpoints[num];
		} else if (how==_T("location-reached")) {
			mark = mxSTC_MARK_EXECPOINT;
			state_text=LANG(DEBUG_STATUS_LOCATION_REACHED,"El programa alcanzo la ubicacion seleccionada");
		} else if (how==_T("function-finished")) {
			wxString retval = GetValueFromAns(ans,_T("return-value"),true);
			mark = mxSTC_MARK_EXECPOINT;
			if (retval.Len()) {
				state_text=LANG(DEBUG_STATUS_FUNCTION_ENDED_RETVAL,"Valor de retorno: ");
				state_text<<retval;
			} else 
				state_text=LANG(DEBUG_STATUS_FUNCTION_ENDED,"La funcion ha finalizado.");
		} else if (how==_T("end-stepping-range")) {
			if (auto_step) { UpdateBacktrace(true,true); _aux_repeat_step; }
			mark = mxSTC_MARK_EXECPOINT;
			state_text = LANG(DEBUG_STATUS_STEP_DONE,"Paso avanzado");
		} else if (how==_T("exited-normally") || how==_T("exited")) {
			if (how==_T("exited-normally"))
				state_text=LANG(DEBUG_STATUS_ENDED_NORMALLY,"El programa finalizo con normalidad");
			else
				state_text=wxString(LANG(DEBUG_STATUS_EXIT_WITH_CODE,"El programa finalizo con el codigo de salida "))<<GetValueFromAns(ans,_T("exit-code"),true);
		} else if (how==_T("signal-received")) {
			if (should_continue) { _aux_continue; }
			if (GetValueFromAns(ans,_T("signal-meaning"),true)==_T("Trace/breakpoint trap")) {
				mark = mxSTC_MARK_EXECPOINT;
				state_text=LANG(DEBUG_STATUS_TRAP,"El programa se interrumpio para ceder el control al depurador");
	#ifdef __WIN32__
				SendCommand(_T("-thread-select 1"));
	#endif
			} else {
				mark = mxSTC_MARK_STOP;
				state_text=wxString(LANG(DEBUG_STATUS_ABNORMAL_INTERRUPTION,"El programa se interrumpio anormalmente ( "))<<GetValueFromAns(ans,_T("signal-name"),true)<<_T(" = ")<<GetValueFromAns(ans,_T("signal-meaning"),false)<<_T(" )");
			}
		} else if (how==_T("exited-signaled") || how==_T("exited")) {
			mark = mxSTC_MARK_STOP;
			state_text=LANG(DEBUG_STATUS_TERMINAL_CLOSED,"La terminal del programa ha sido cerrada");
		} 
		else { 
//			_DBG_LOG_CALL(Log(wxString()<<"NEW REASON: "<<ans));
			if (GetValueFromAns(ans,"frame").Len()) {
				mark = mxSTC_MARK_EXECPOINT;
				state_text=LANG(DEBUG_STATUS_PAUSE_UNKNOWN_REASON,"Ejecuci�n interrumpida, causa desconocida.");
			}
		}
		if (mark) {
//			wxString fname = GetSubValueFromAns(ans,_T("frame"),_T("fullname"),true,true);
//			if (!fname.Len())
//				fname = GetSubValueFromAns(ans,_T("frame"),_T("file"),true,true);
//			fname.Replace(_T("//"),sep);
//			fname.Replace(_T("\\\\"),sep);
//			fname.Replace(wrong_sep,sep);
//			wxString line =  GetSubValueFromAns(ans,_T("frame"),_T("line"),true);
//			if (stepping_in && mark==mxSTC_MARK_EXECPOINT && black_list.Index(fname)!=wxNOT_FOUND)
//				StepIn();
//			else {
//				stepping_in=false;
				UpdateBacktrace(true,true);
				UpdateInspections();
//			}
		} else {
			BacktraceClean();
			ThreadListClean();
			running = false; // es necesario esto?
			bool debug_ends = how=="exited-normally"||how=="exited";
			if (debug_ends) raise_zinjai_window = false;
			Stop(
#ifndef __WIN32__
				debug_ends
#	ifndef __APPLE__
				&&(wait_for_key_policy==WKEY_ALWAYS||(wait_for_key_policy==WKEY_ON_ERROR&&how=="exited"))
#	endif
#endif
			);
		}
		if (bpi && bpi->action==BPA_STOP_ONCE) bpi->SetStatus(BPS_DISABLED_ONLY_ONCE);
		SetStateText(state_text); should_pause=false;
		if (bpi && bpi->annotation.Len() && current_source) {
			wxYield();
			current_source->ShowBaloon(bpi->annotation,current_source->PositionFromLine(current_source->GetCurrentLine()));
		}
		if (raise_zinjai_window && config->Debug.raise_main_window) main_window->Raise();
		return retval;
	}
}

/**
* @brief Removes a breakpoint from gdb and from ZinjaI's list
*
* If !debug->waiting it deletes the brekapoint inmediatly, but if its running
* it marks the breakpoint to be deleted with pause_* and Pause the execution.
* Next time debug see the execution paused will invoke this method again and 
* resume the execution, so user can delete breakpoint without directly pausing
* the program being debugged.
**/
bool DebugManager::DeleteBreakPoint(BreakPointInfo *_bpi) {
	if (!debugging) return false;
	if (_bpi->gdb_id==-1) { // only invalid breakpoints can be safetly deleted from zinjai without interacting with gdb (the ones that were not correctly setted in gdb (for instance, with a wrong line number))
		delete _bpi; 
		return true;
	}
	if (waiting) { // si esta ejecutando, anotar para sacar y mandar a pausar
		_DEBUG_LAMBDA_1( lmbRemoveBreakpoint, BreakPointInfo,p, { debug->DeleteBreakPoint(p); } );
		PauseFor(new lmbRemoveBreakpoint(_bpi));
		return false;
	} else {
		// decirle a gdb que lo saque
		SendCommand("-break-delete ",_bpi->gdb_id);
		// sacarlo de la memoria y las listas de zinjai
		delete _bpi;
	}
	return true;
}

int DebugManager::LiveSetBreakPoint(BreakPointInfo *_bpi) {
	if (debugging && waiting) {
		_DEBUG_LAMBDA_1( lmbAddBreakpoint, BreakPointInfo,bp, { debug->SetBreakPoint(bp); } );
		PauseFor(new lmbAddBreakpoint(_bpi));
		_bpi->SetStatus(BPS_PENDING);
		return -1;
	} else {
		return SetBreakPoint(_bpi);
	}
}

/**
* Coloca un pto de interrupci�n en gdb
*
* Le pide al depurador agregar un pto de interrupci�n. Esta funci�n se usa tanto 
* para proyectos como programas simples, y se llama desde mxSource::OnMarginClick,
* DebugManager::HowDoesItRuns y ProjectManager::SetBreakpoints y 
* DebugManager::SetBreakPoints. Si logra colocar el bp retorna su id, sino -1.
*
* Setea adem�s las propiedades adicionales si est�n definidas en el BreakPointInfo.
*
* Antes de colocar el breakpoint se fija si es una direccion valida, porque
* sino gdb lo coloca m�s adelante sin avisar.
**/
int DebugManager::SetBreakPoint(BreakPointInfo *_bpi, bool quiet) {
	if (waiting || !debugging) return 0;
	wxString adr = GetAddress(_bpi->fname,_bpi->line_number);
	if (!adr.Len()) { _bpi->SetStatus(BPS_ERROR_SETTING); if (!quiet) ShowBreakPointLocationErrorMessage(_bpi); return -1;  }
	wxString ans = SendCommand(wxString("-break-insert \"\\\"")<<_bpi->fname<<":"<<_bpi->line_number+1<<"\\\"\"").result;
	wxString num = GetSubValueFromAns(ans,"bkpt","number",true);
	if (!num.Len()) { // a veces hay que poner dos barras (//) antes del nombre del archivo en vez de una (en los .h? �por que?)
		wxString file=_bpi->fname;
		int p = file.Find('/',true);
		if (p!=wxNOT_FOUND) {
			wxString file2 = file.Mid(0,p);
			file2<<'/'<<file.Mid(p);
			ans = SendCommand(wxString("-break-insert \"")<<file2<<":"<<_bpi->line_number+1<<"\"").result;
			num = GetSubValueFromAns(ans,"bkpt","number",true);
		}
	}
	int id=-1;
	BREAK_POINT_STATUS status=BPS_SETTED;
	if (num.Len()) {
		long l; num.ToLong(&l); _bpi->gdb_id=id=l; // seteo el id aca para que los SetXXX del DebugManager que reciben solo el _bpi sepan que id usar
		// setear las opciones adicionales
		if (_bpi->ignore_count) SetBreakPointOptions(id,_bpi->ignore_count);
		if (_bpi->action==BPA_STOP_ONCE ||!_bpi->enabled) SetBreakPointEnable(_bpi);
		if (!_bpi->enabled) status=BPS_USER_DISABLED;
		if (_bpi->cond.Len()) if (!SetBreakPointOptions(id,_bpi->cond)) { status=BPS_ERROR_SETTING; ShowBreakPointConditionErrorMessage(_bpi); }
	} else { // si no se pudo colocar correctamente
		status=BPS_ERROR_SETTING; 
		if (!quiet) ShowBreakPointLocationErrorMessage(_bpi);
	}
	_bpi->SetStatus(status,id);
	return id;
}

wxString DebugManager::InspectExpression(wxString var, bool full) {
	if (waiting || !debugging) return "";
	SetFullOutput(full);
	if (var.StartsWith(">")) return GetMacroOutput(var.Mid(1));
	return GetValueFromAns( SendCommand("-data-evaluate-expression ",mxUT::EscapeString(var,true)).result,"value",true,true);
}

void DebugManager::SetBacktraceShowsArgs(bool show) {
	backtrace_shows_args=show;
	UpdateBacktrace(false,false);
}

bool DebugManager::UpdateBacktrace(bool and_threadlist, bool was_running) {
	if (and_threadlist && threadlist_visible) UpdateThreads();
	if (waiting || !debugging) return false;
	
	// averiguar las direcciones de cada frame, para saber donde esta cada inspeccion V2 
	// ya no se usan "direcciones", sino que simplemente se numeran desde el punto de entrada para "arriba"
	prev_stack = current_stack;
	if (!GetValueFromAns(SendCommand("-stack-info-depth ",BACKTRACE_SIZE).result,"depth",true).ToLong(&current_stack.depth)) current_stack.depth=0;
	current_stack.frames = current_stack.depth>0 ? SendCommand("-stack-list-frames 0 ",current_stack.depth-1).result : "";
	
	bool retval = UpdateBacktrace(current_stack,true);
	if (was_running) for(int i=0;i<backtrace_consumers.GetSize();i++) backtrace_consumers[i]->OnBacktraceUpdated(was_running);
	return retval;
}
	

bool DebugManager::UpdateBacktrace(const BTInfo &stack, bool is_current) {

	backtrace_is_current = is_current;
	
#ifdef __WIN32__
	static wxString sep="\\",wrong_sep="/";
#else
	static wxString sep="/",wrong_sep="\\";
#endif
	
	int last_stack_depth = backtrace_rows_count;
	backtrace_rows_count = stack.depth>BACKTRACE_SIZE?BACKTRACE_SIZE:stack.depth; 
	
	main_window->backtrace_ctrl->BeginBatch();
	const wxChar * chfr = stack.frames.c_str();
	// to_select* es para marcar el primer frame que tenga info de depuracion
	int i = stack.frames.Find("stack=");
	if (i==wxNOT_FOUND) {
		current_frame_id=-1;
		for (int c=0;c<stack.depth;c++) {
			main_window->backtrace_ctrl->SetCellValue(c,BG_COL_FILE,LANG(BACKTRACE_NO_INFO,"<<Imposible determinar ubicacion>>"));
			main_window->backtrace_ctrl->SetCellValue(c,BG_COL_FUNCTION,LANG(BACKTRACE_NO_INFO,"<<Imposible determinar ubicacion>>"));
		}
		main_window->backtrace_ctrl->EndBatch();
		return false;
	} else i+=7; 
	int cant_levels=0, to_select_level=-1, sll=stack.frames.Len();
	while (cant_levels<BACKTRACE_SIZE) {
		while (i<sll && chfr[i]!='{') i++;
		if (i==sll) break;
		int p=i+1;
		while (chfr[i]!='}') { i++; if (chfr[i]=='\''||chfr[i]=='\"') GdbParse_SkipString(chfr,i,sll); }
		wxString s(stack.frames.SubString(p,i-1));
#ifdef _ZINJAI_DEBUG
		if (GetValueFromAns(s,"level",true)!=(wxString()<<(cant_levels))) {
			ZLERR2("DebugManager","UpdateBacktrace, wrong frame level, cant="<<cant_levels);
			ZLERR2("DebugManager","UpdateBacktrace, wrong frame level, num="<<s);
		}
#endif
		main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_LEVEL,wxString()<<cant_levels);
		wxString func = GetValueFromAns(s,"func",true);
		if (func[0]=='?') {
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_FUNCTION,LANG(BACKTRACE_NOT_AVAILABLE,"<<informacion no disponible>>"));
			wxString from = GetValueFromAns(s,"from",true);
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_FILE,from.IsEmpty()?"":wxString("from: ")+from);
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_LINE,"");
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_ARGS,"");
		} else {
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_FUNCTION,func);
			wxString fname = GetValueFromAns(s,"fullname",true,true);
//			if (!fname.IsEmpty()&&!fname.EndsWith(".cpp")) asm("int3");
			if (!fname.Len()) fname = GetValueFromAns(s,"file",true,true);
			fname.Replace("\\\\",sep,true);
			fname.Replace("//",sep,true);
			fname.Replace(wrong_sep,sep,true);
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_FILE,fname);
			// seleccionar el frame de mas arriba que tenga info de depuracion (que en line diga algo)
			wxString line_str=GetValueFromAns(s,"line",true);
			main_window->backtrace_ctrl->SetCellValue(cant_levels,BG_COL_LINE,line_str);
			if (to_select_level==-1 && line_str.Len() && wxFileName::FileExists(fname))
				to_select_level=cant_levels;
		}
		cant_levels++;
	}
	
	// completar la columna de argumentos si es que est� visible
	if (backtrace_shows_args) {
		
		if (!is_current) { // history mode
			for (int c=0;c<cant_levels;c++) {
				main_window->backtrace_ctrl->SetCellValue(c,BG_COL_ARGS,"???");
			}
		} else {
		
			debug->SetFullOutput(false);
			wxString args ,args_list = cant_levels ? SendCommand("-stack-list-arguments 1 0 ",cant_levels-1).result : "";
			const wxChar * chag = args_list.c_str();
			i=args_list.Find("stack-args=");
			if (i==wxNOT_FOUND) {
				for (int c=0;c<stack.depth;c++)
					main_window->backtrace_ctrl->SetCellValue(c,BG_COL_ARGS,LANG(BACKTRACE_NO_ARGUMENTS,"<<Imposible determinar argumentos>>"));
				main_window->backtrace_ctrl->EndBatch();
			} else {
				bool comillas = false, cm_dtype=false; //cm_dtype indica el tipo de comillas en que estamos, inicializar en false es solo para evitar el warning
				i+=12;
				for (int c=0;c<cant_levels;c++) {
					// chag+i = frame={level="0",args={{name="...
					while (chag[i]!='[' && chag[i]!='{') {
						if (!chag[i]) break;
						i++; 
					}
					if (!chag[i]) break;
					int p=++i, arglev=0;
					// chag+i = level="0",args={{name="...
					while ((chag[i]!=']' && chag[i]!='}') || comillas || arglev>0) {
						if (comillas) {
							if (cm_dtype && chag[i]=='\"') 
								comillas=false;
							else if (!cm_dtype && chag[i]=='\'') 
								comillas=false;
							else if (chag[i]=='\\') i++;
						} else {
							if (chag[i]=='\"' || chag[i]=='\'')
								{ comillas=true; cm_dtype=(chag[i]=='\"'); }
							else if (!comillas && (chag[i]=='{' || chag[i]=='['))
								arglev++;
							else if (!comillas && (chag[i]==']' || chag[i]=='}'))
								arglev--;
							else if (chag[i]=='\\') i++;
						}
						i++;
					}
					wxString s(args_list.SubString(p,i-1));
					wxString args, sub;
					int j=0, l=s.Len();
					const wxChar * choa = s.c_str();
					// choa+j = level="0",args={{name="...
					while (j<l && choa[j]!='{' && choa[j]!='[')	j++;
					j++;
					// choa+j = {name="...
					while (j<l) {
						while (j<l && choa[j]!='{' && choa[j]!='[')
							j++;
						if (j==l)
							break;
						p=++j;
						arglev=0; comillas=false;
						while (choa[j]!='}' || comillas || arglev>0) {
							if (comillas) {
								if (cm_dtype && choa[j]=='\"') 
									comillas=false;
								else if (!cm_dtype && choa[j]=='\'') 
									comillas=false;
								else if (choa[j]=='\\') j++;
							} else {
								if (choa[j]=='\"' || choa[j]=='\'')
								{ comillas=true; cm_dtype=(choa[j]=='\"'); }
								else if (choa[j]=='{' || choa[j]=='[')
									arglev++;
								else if (choa[j]==']' || choa[j]=='}')
									arglev--;
								else if (choa[j]=='\\') j++;
							}
							j++;
						}
						sub=s.SubString(p,j-1);
						if (args.Len()) args<<", ";
						args<<GetValueFromAns(sub,"name",true)<<"="<<GetValueFromAns(sub,"value",true,true);
					}
					main_window->backtrace_ctrl->SetCellValue(c,BG_COL_ARGS,args);
				}	
			}
		}
	}
	
	// "limpiar" los renglones que sobran
	for (int c=stack.depth; c<last_stack_depth; c++) {
		for (int i=0;i<BG_COLS_COUNT;i++)
			main_window->backtrace_ctrl->SetCellValue(c,i,"");
	}
	
	// seleccionar el frame actual, o el m�s cercano que tenga info de depuraci�n
	if (to_select_level>=0) {
		main_window->backtrace_ctrl->SelectRow(to_select_level);
		SelectFrame(-1,to_select_level);
		wxString file=main_window->backtrace_ctrl->GetCellValue(to_select_level,BG_COL_FILE);
		wxString sline=main_window->backtrace_ctrl->GetCellValue(to_select_level,BG_COL_LINE);
		long line=0; if (sline.ToLong(&line)) debug->MarkCurrentPoint(file,line,is_current?(to_select_level>0?mxSTC_MARK_FUNCCALL:mxSTC_MARK_EXECPOINT):mxSTC_MARK_HISTORY);
	} else {
		main_window->backtrace_ctrl->SelectRow(0);
		current_frame_id = GetFrameID(0);
		debug->MarkCurrentPoint();
	}
	
	main_window->backtrace_ctrl->EndBatch();
	return true;
}

void DebugManager::StepIn() {
	if (waiting || !debugging) return;
	stepping_in = true;
	GDBAnswer &ans = SendCommand(step_by_asm_instruction?"-exec-step-instruction":"-exec-step");
	if (ans.result.StartsWith("^running")) HowDoesItRuns();
}

void DebugManager::StepOut() {
	if (waiting || !debugging) return;
	GDBAnswer &ans = SendCommand("-exec-finish");
	if (ans.result.StartsWith("^running")) HowDoesItRuns();
}

void DebugManager::StepOver() {
	if (waiting || !debugging) return;
	stepping_in = false;
	GDBAnswer &ans = SendCommand(step_by_asm_instruction?"-exec-next-instruction":"-exec-next");
	if (ans.result.StartsWith("^running")) HowDoesItRuns();
}


void DebugManager::Pause() {
	should_pause=true;
	if (!waiting && !debugging) return;
#ifdef __WIN32__
	if (!OSDep::winLoadDBP()) {
		mxMessageDialog(main_window,"Esta caracteristica no se encuentra presente\n"
									"en versiones de Windows previas a XP-SP2")
			.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
		return;
	}
	if (FindOutChildPid()) OSDep::winDebugBreak(child_pid);
#else
	// for some reason, since some system-level updated sending SIGINT to gdb started working weird...
	// was working only for the first time in each debug session, but being ignored (or causing other
	// random issues) in the following times (not always, I suspect on multi-threaded processes, such
	// as a wx project)... that's why next times I use the debuged process'id (child_pid) instead of 
	// the debugger's pid (pid)... but not the first time cause I need to talk with gdb once the 
	// program has started in order to find out that child_pid (see FindOutChildPid())
	ZLINF2("DebugManager","Pause, Enviando SIGINT a "<<(child_pid!=0?"child_pid: ":"pid: ")<<(child_pid!=0?child_pid:gdb_pid));
	process->Kill(child_pid!=0?child_pid:gdb_pid,wxSIGINT);
#endif
}

/**
* Si ya se conocia (child_pid!=0) retorna true sin hacer nada, sino retorna true 
* si logra determinarlo, en cuyo caso lo coloca en child_pid.
* En linux usa el comando gdb "info proc status", en windows busca un proceso 
* hijo del proceso de gdb.
**/
bool DebugManager::FindOutChildPid() {
	if (child_pid!=0) return true; // si ya se sabe cual es, no hace nada
#ifdef __WIN32__
	child_pid = OSDep::GetChildPid(gdb_pid);
#else
# ifdef __linux__
	wxString val = SendCommand("info proc status").stream;
	int pos = val.Find("Pid:"); 
	if (pos==wxNOT_FOUND) return false;
	pos+=4; while (val[pos]==' '||val[pos]=='\t') pos++;
	child_pid=0; 
	while (val[pos]>='0'&&val[pos]<='9') { 
		child_pid*=10; child_pid+=val[pos]-'0'; pos++;
	}
# endif
#endif
	return child_pid!=0;
}

void DebugManager::Continue() {
	if (waiting || !debugging || status==DBGST_STOPPING || status==DBGST_WAITINGKEY) return;
	
#if defined(__WIN32__) || defined(__unix__)
	if (config->Debug.return_focus_on_continue && FindOutChildPid()) {
		// intentar darle el foco a alguna ventana de la aplicacion, o a la terminal si no hay ventana
# ifdef __WIN32__
		OSDep::SetFocus(child_pid);
# else
		if (!OSDep::SetFocus(child_pid)) OSDep::SetFocus(tty_pid);
# endif
	}
#endif
	
	MarkCurrentPoint();
	GDBAnswer &ans = SendCommand("-exec-continue");
	if (ans.result.Mid(1,7)=="running") HowDoesItRuns(true);
}

void DebugManager::ReadGDBOutput() {
	if (input->IsOk() && input->CanRead()) {
		int n, watchdog=0;
		do {
			static char buffer[256];
			n = input->Read(buffer,255).LastRead();
			m_gdb_buffer.Read(buffer,n);
		} while (++watchdog!=1000 && n==255 && input->CanRead());
	}
}

DebugManager::GDBAnswer &DebugManager::WaitAnswer(bool set_reset_running) {
	
	boolFlagGuard running_guard(set_reset_running?&running:nullptr);
	
	struct WatchDog {
		bool running;
		int lines;
		wxDateTime t0;
		WatchDog(bool r) : running(r), lines(0) { if (!running) t0=wxDateTime::Now(); }
		bool NewLine() { if (!running) { if ((++lines)%1000==0) return Check(); } return false; }
		bool Check() {
			EXPECT(!running);
			if ((wxDateTime::Now()-t0).GetSeconds()>=10) {
				if (mxMessageDialog(main_window,"Alguna operaci�n en el depurador est� tomando demasiado tiempo, desea interrumpirla?.")
					.Title("UPS!").IconWarning().ButtonsYesNo().Run().yes ) 
				{
					_DBG_LOG_CALL(Log(wxString()<<"\n<<<TIME OUT>>>\n"));
					return true;
				} else {
					t0 = wxDateTime::Now();
				}
			}
			return false;
		}
	};
	
	boolFlagGuard waiting_guard(waiting,true);
	last_answer.Clear();
	WatchDog watchdog(running);
	while(process || m_gdb_buffer.HasData()) {
		
		// try to get some output from gdb
		if (process) ReadGDBOutput();
		else { ZLWAR("DebugManager","WaitAnswer, process==nullptr"); }
		
		GDBAnsBuffer::LineType type;
		static wxString line;
		while ( (type=m_gdb_buffer.GetNextLine(line))!=GDBAnsBuffer::NONE ) {
			last_answer.full += line; last_answer.full +="\n";
			_DBG_LOG_CALL(Log(wxString()<<"\n<<< "<<line));
			switch(type) {
				case GDBAnsBuffer::LOG_STREAM:
					if (line.StartsWith("&\"Error in re-setting breakpoint ")) {
						long bn=-1;	
						if (line.Mid(33).BeforeFirst(':').ToLong(&bn)) {
							BreakPointInfo *bpi = BreakPointInfo::FindFromNumber(bn,true);
							if (bpi) {
								bpi->SetStatus(BPS_ERROR_SETTING);
								ShowBreakPointLocationErrorMessage(bpi);
							}
						}
					}
					main_window->AddToDebugLog(line);
				break;
				
				case GDBAnsBuffer::TARGET_STREAM:
				case GDBAnsBuffer::CONSOLE_STREAM:
					last_answer.stream += mxUT::UnEscapeString(line.Mid(2,line.Len()-3),false);
				break;
				
				case GDBAnsBuffer::NOTIFY_ASYNC:
					main_window->AddToDebugLog(line);
				break;
				case GDBAnsBuffer::EXEC_ASYNC:
				case GDBAnsBuffer::STATUS_ASYNC:
					last_answer.async += line;
				break;
				
				case GDBAnsBuffer::RESULT:
					last_answer.result += line;
				break;
				
				case GDBAnsBuffer::PROMPT:
					return last_answer; // normal return path
				
				default:
//					last_answer.cli_output += line;
					;
			}
			if (watchdog.NewLine()) { // too many lines
				InterruptOperation();
				_DBG_LOG_CALL(Log(wxString()<<"\n<<<TIME OUT 1>>>\n"));
				return last_answer.Clear(false);
			}
		}
		
		if (watchdog.NewLine()) { // a line too long
			InterruptOperation();
			_DBG_LOG_CALL(Log(wxString()<<"\n<<<TIME OUT 2>>>\n"));
			return last_answer.Clear(false);
		}
		
		// wait for gdb to generate more output
		while (process && input->IsOk() && !input->CanRead()) {
			if (running) {
				g_application->Yield(true);
				wxMilliSleep(50);
			} else {
				wxMilliSleep(10);
				if (watchdog.Check()) { // long time no answer
					InterruptOperation();
					_DBG_LOG_CALL(Log(wxString()<<"\n<<<TIME OUT 3>>>\n"));
					return last_answer.Clear(false);
				}
			}
		}
		
	}
	// this is not the happy path, something went wrong
	_DBG_LOG_CALL(Log(wxString()<<"\n<<<TRUNCATED OUTPUT>>>\n"));
	last_answer.full += "<<<TRUNCATED OUTPUT>>>";
	last_answer.is_ok = false;
	return last_answer;
}

void DebugManager::SendCommandNW(wxString command) {
	_DBG_LOG_CALL(Log(wxString()<<"\n>>> "<<command));
	if (!process) return;
	last_command = command;
	output->Write(command.c_str(),command.Len());
	output->Write("\n",1);
}

DebugManager::GDBAnswer &DebugManager::SendCommand(wxString command) {
	waiting = true;
	_DBG_LOG_CALL(Log(wxString()<<"\n>>> "<<command));
	if (!process) return last_answer.Clear(false);
	last_command = command;
	output->Write(command.c_str(),command.Len());
	output->Write("\n",1);
	return WaitAnswer();
}

DebugManager::GDBAnswer &DebugManager::SendCommand(wxString command, int i) {
	waiting = true;
	_DBG_LOG_CALL(Log(wxString()<<"\n>>> "<<command<<i));
	if (!process) return last_answer.Clear(false);
	command<<i<<"\n";
	last_command = command;
	output->Write(command.c_str(),command.Len());
	return WaitAnswer();
}

DebugManager::GDBAnswer &DebugManager::SendCommand(wxString cmd1, wxString cmd2) {
	waiting = true;
	_DBG_LOG_CALL(Log(wxString()<<"\n>>> "<<cmd1<<cmd2));
	if (!process) return last_answer.Clear(false);
	cmd1<<cmd2<<"\n";
	last_command = cmd1;
	output->Write(cmd1.c_str(),cmd1.Len());
	return WaitAnswer();
}

/// @brief Sets all breakpoints from an untitled or out of project mxSource
void DebugManager::SetBreakPoints(mxSource *source, bool quiet) {
	if (waiting || !debugging) return;
	const LocalList<BreakPointInfo*> &breakpoints=source->m_extras->GetBreakpoints();
	for(int i=0;i<breakpoints.GetSize();i++) { 
		breakpoints[i]->UpdateLineNumber();
		debug->SetBreakPoint(breakpoints[i],quiet);
	}
}

/**
* @brief Marca el punto actual del c�digo donde se encuentra el depurador
*
* Marca el punto donde se encuentra en el frame actual, borrando la marca anterior
* si es que hab�a. Si se invoca sin argumentos borra la marca anterior y nada mas.
* La marca es la flecha en el margen, verde para ejecucion normal y en el frame
* interior, amarillo para otro frame, rojo cuando se detuvo por un problema
* irrecuperable, como un segfault.
*
* @param cfile path completo del archivo
* @param cline numero de line en Base 1
* @param cmark tipo de marcador: -1 (ninguna), mxSTC_MARK_EXECPOINT (verde), mxSTC_MARK_FUNCCALL (amarillo), mxSTC_MARK_STOP (rojo)
**/
bool DebugManager::MarkCurrentPoint(wxString cfile, int cline, int cmark) {
	if (cmark!=-1) {
		if (current_source) {
			if (current_handle!=-1) 
				current_source->MarkerDeleteHandle(current_handle);
			if ( (current_source->GetFullPath()) !=cfile ) {
				current_source = main_window->IsOpen(cfile);
				if (!current_source) {
					if (notitle_source && notitle_source->temp_filename==cfile)
						current_source = notitle_source;
					else
						current_source = main_window->OpenFile(cfile);
				}
			}
		} else {
			if (notitle_source && notitle_source->temp_filename==cfile)
				current_source = notitle_source;
			else {
				current_source = main_window->IsOpen(cfile);
				if (!current_source) 
					current_source = main_window->OpenFile(cfile);
			}
		}
		if (current_source==EXTERNAL_SOURCE) current_source=nullptr;
		if (current_source) {
			int x=main_window->notebook_sources->GetPageIndex(current_source);
			if (x!=main_window->notebook_sources->GetSelection())
				main_window->notebook_sources->SetSelection(x);
			current_handle = current_source->MarkerAdd(cline-1,cmark);
			current_source->MarkError(cline-1);
		}
	} else {
		if (current_source) {
			current_source->MarkerDeleteHandle(current_handle);
			current_handle=-1;
		}
	}
	return current_source;
}

/**
* @brief Parsea la salida de un comando enviado a gdb para extraer un campo
*
* @param ans        cadena con el resultado arrejado por gdb en su interfaz mi, usualmente obtenida mediante SendCommand
* @param key        nombre del campo cuyo valor se quiere extraer (por ejemplo, si la cadena dice 'name="booga"' y se le pasa "name" se obtiene "booga")
* @param crop       indica si hay que suprimir las comillas que rodean al valor
* @param fix_slash  si el valor es texto puede contener secuencias de escape, esto indica si hay que reemplazarlas por sus valores reales
**/
wxString DebugManager::GetValueFromAns(wxString ans, wxString key, bool crop, bool fix_slash) {
	if (ans.Len()<key.Len()+2)
		return "";
	unsigned int sc=0, t = ans.Len()-key.Len()-2;
	key<<_T("=");
	char stack[25];
//	bool not_ignore=true;
	bool is_the_first = (ans.Left(key.Len())==key) ;
	for (unsigned int i=0;i<t;i++) {
		char c=ans[i];
		if (sc && !is_the_first) {
			if (stack[sc]=='[' && c==']')
				sc--;
			else if (stack[sc]=='(' && c==')')
				sc--;
			else if (stack[sc]=='{' && c=='}')
				sc--;
			else if (stack[sc]=='\"' && c=='\"')
				sc--;
			else if (stack[sc]=='\'' && c=='\'')
				sc--;
			else if (stack[sc]!='\"' && stack[sc]!='\'' /*&& not_ignore*/) {
				if (c=='[' || c=='\"' || c=='{' || c=='(')
					stack[++sc]=c;
//			} else if (stack[sc]=='\"' && c=='\\') {
//				not_ignore=false;
//			} else 
//				not_ignore=true;
			} else if (c=='\\') {
				i++; 
				if (ans[i]=='n') 
					ans[i]='\n';
				continue;
			}
		}  else {
			if (c=='[' || c=='\"' || c=='\'' || c=='{')
				stack[++sc]=c;
			else if ( (c==',' && ans.Mid(i+1,key.Len())==key) || is_the_first ) {
				if (is_the_first)
					i--;
				i+=key.Len()+1;
				sc=0;
				int p1=i;
				c=ans[i];
				while ( i<ans.Len() && !( sc==0 && (c==',' || c=='}' || c=='\r' || c=='\n') ) ) {
					if (sc) {
						if (stack[sc]=='[' && c==']')
							sc--;
						else if (stack[sc]=='(' && c==')')
							sc--;
						else if (stack[sc]=='{' && c=='}')
							sc--;
						else if (stack[sc]=='\"' && c=='\"') {
							sc--;
						}
						else if (stack[sc]=='\'' && c=='\'')
							sc--;
						else if (stack[sc]!='\"' && stack[sc]!='\'' /*&& not_ignore*/) {
							if (c=='[' || c=='\"' || c=='{' || c=='(')
								stack[++sc]=c;
//						} else if (stack[sc]=='\"' && c=='\\')
//							not_ignore=false;
//						else 
//							not_ignore=true;
						} else if (c=='\\') {
							i++; 
							if (ans[i]=='n') 
								ans[i]='\n';
						}
					} else if (c=='[' || c=='\"' || c=='\'' || c=='{') {
						stack[++sc]=c;
					}
					c=ans[++i];
				}
				if (!fix_slash)
					return ans.Mid(p1+(crop?1:0),i-p1-(crop?2:0));
				wxString ret = ans.Mid(p1+(crop?1:0),i-p1-(crop?2:0));
				
				int l=ret.Len(),i=0,d=0;
				while (i<l) {
					if (ret[i]=='\\') { 
						if (ret[i+1]=='n') ret[i+1]='\n';
						if (ret[i+1]=='n') ret[i+1]='\t';
						else if (ret[i+1]<='9' && ret[i+1]>='0' && i+3<l && ret[i+2]<='9' && ret[i+2]>='0' && ret[i+3]<='9' && ret[i+3]>='0')
							{ ret[i+3]=(ret[i+1]-'0')*8*8+(ret[i+2]-'0')*8+(ret[i+3]-'0'); i+=2; }
						i++; d++; ret[i-d] = ret[i];
					}
					if (d) ret[i-d]=ret[i];
					i++;
				}
				return ret.Mid(0,l-d);
			}
		}
	}
	return "";
}

wxString DebugManager::GetSubValueFromAns(wxString ans, wxString key1, wxString key2, bool crop, bool fix_slash) {
	wxString value = GetValueFromAns(ans,key1);
	if (value.Len()) {
		value[0]=value[value.Len()-1]=',';
		return GetValueFromAns(value,key2,crop,fix_slash);
	}
	return "";
}

void DebugManager::SetStateText(wxString text, bool refresh) {
	main_window->SetCompilingStatus(text);
	menu_data->toolbar_status_text->SetLabel(wxString(LANG(DEBUG_STATUS_PREFIX,"  Depuracion: "))+text);
	if (refresh) wxYield();
}


void DebugManager::BacktraceClean() {
	if (current_stack.depth>BACKTRACE_SIZE) current_stack.depth=BACKTRACE_SIZE;
	for(int c=0;c<current_stack.depth;c++) {
		for (int i=0;i<BG_COLS_COUNT;i++)
			main_window->backtrace_ctrl->SetCellValue(c,i,"");
	}
	current_stack.depth = 0;
}

/**
* @brief Busca la direcci�n de memoria donde empiezan las instrucciones de una linea particular del codigo fuente
*
* Si gdb no reconoce la ubicacion devuelve una cadena vacia. Esta funcion sirve 
* entre otras cosas para saber si es una ubicaci�n v�lida, por ejemplo para
* verificar antes de colocar los puntos de interrupci�n.
*
* @param fname ruta del archivo, con cualquier barra (si es windows corrige)
* @param line n�mero de linea en base 0
**/
wxString DebugManager::GetAddress(wxString fname, int line) {
	if (waiting || !debugging) return "";
#ifdef __WIN32__
	for (unsigned int i=0;i<fname.Len();i++) // corregir las barras en windows para que no sean caracter de escape
		if (fname[i]=='\\') 
			fname[i]='/';
#endif
	wxString ans = SendCommand(wxString("info line \"")<<fname<<":"<<line+1<<"\"").stream;
	int r=ans.Find("starts at");
	if (r!=wxNOT_FOUND) {
		ans=ans.Mid(r);
		r=ans.Find("0x");
		if (r!=wxNOT_FOUND)
			return ans.Mid(r).BeforeFirst(' ');
	}
	return "";
}

/**
* @brief Hace un salto sin miramientos a alguna linea de c�digo
*
* @param fname ruta del archivo, con cualquier barra (si es windows corrige)
* @param line n�mero de linea en base 0
**/
bool DebugManager::Jump(wxString fname, int line) {
	if (waiting || !debugging) return false;
	boolFlagGuard running_guard(running); // para que se ponia en true aca? se puede sacar?
	wxString adr = GetAddress(fname,line);
	if (adr.Len()) {
		GDBAnswer &ans=SendCommand("-gdb-set $pc=",adr);
		if (ans.result.StartsWith("^done")) {
			MarkCurrentPoint(fname,line+1,mxSTC_MARK_EXECPOINT);
			UpdateBacktrace(false,true);
			return true;
		}
	}
	return false;
}

/**
* @brief Busca la direcci�n de memoria donde empiezan las instrucciones de una linea particular del codigo fuente
*
* Si gdb no reconoce la ubicacion devuelve una cadena vacia. Esta funcion sirve 
* entre otras cosas para saber si es una ubicaci�n v�lida, por ejemplo para
* verificar antes de colocar los puntos de interrupci�n.
*
* @param fname ruta del archivo, con cualquier barra (si es windows corrige)
* @param line n�mero de linea en base 0
*
* @return true si la direccion dada en fname y line es valida y el depurador efectivamente
*         se puso a ejecutar, false si la direccion no era valida o hubo algun error
**/
bool DebugManager::RunUntil(wxString fname, int line) {
	if (waiting || !debugging) return false;
	wxString adr = GetAddress(fname,line);
	if (adr.Len()) {
		GDBAnswer &ans = SendCommand("advance *",adr); // aca estaba exec-until pero until solo funciona en un mismo frame, advance se los salta sin problemas
		if (ans.result.StartsWith("^error"))
		HowDoesItRuns(); /// @todo: guardar adr para comparar en HowDoesItRuns y saber si realmente llego o no a ese punto (siempre dice que si, aunque termine el programa sin pasar por ahi)
		running = false;
		return true;
	}
	return false;
}

bool DebugManager::Return(wxString what) {
	if (waiting || !debugging) return false;
	GDBAnswer &ans= SendCommand(wxString("-exec-return ")<<what);
	if (ans.result.SubString(1,4)!="done") 
		return false;
	
	wxString fname = GetSubValueFromAns(ans.result,"frame","fullname",true);
	if (!fname.Len())
		fname = GetSubValueFromAns(ans.result,"frame","file",true);
	wxString line =  GetSubValueFromAns(ans.result,"frame","line",true);
	long fline = -1;
	line.ToLong(&fline);
	MarkCurrentPoint(fname,fline,mxSTC_MARK_EXECPOINT);
	UpdateBacktrace(false,true);
	UpdateInspections();
	return true;
}

void DebugManager::ProcessKilled() {
	ZLINF("DebugManager","ProcessKilled");
	ReadGDBOutput(); // this event can be triggered within a wxYield called inside WaitAnswer, so will get the remaining output before deleting the process
	delete debug_patcher;
	_DBG_LOG_CALL(Close());
	MarkCurrentPoint();
	notitle_source = nullptr;
	debugging=false;
#ifndef __WIN32__
	if (tty_process) {
		ZLINF2("DebugManager","ProcessKilled, enviando SIGKILL a tty_pid: "<<tty_pid);
		tty_process->Kill(tty_pid,wxSIGKILL);
	}
#endif
	delete process;
	process=nullptr;
	gdb_pid=0;
	running = debugging = waiting = false;
	status=DBGST_NULL;
	wxCommandEvent evt;
	if (gui_is_prepared) main_window->PrepareGuiForDebugging(false);
	DebuggerInspection::OnDebugStop();
	for(int i=0;i<backtrace_consumers.GetSize();i++) backtrace_consumers[i]->OnDebugStop();
}

#ifndef __WIN32__
void DebugManager::TtyProcessKilled() {
	ZLINF("DebugManager","TtyProcessKilled");
	if (gdb_pid && debugging && status!=DBGST_STOPPING) Stop();
	delete tty_process;
	tty_process = nullptr;
	tty_pid = 0;
}
#endif

void DebugManager::UpdateInspections() {
	DebuggerInspection::OnDebugPause();
}

wxString DebugManager::GetNextItem(wxString &ans, int &from) {
	char stack[50];
	int st_size=0;
	bool comillas = false;
	int p1=from,p=from, l=ans.Len();
	const wxChar * ch = ans.c_str();
	while ( !(( st_size==0 && !comillas && (ch[p]==',' || ch[p]==']')) || p>=l ) ) {
		switch (ch[p]) {
		case '\\':
			p++;
			break;
		case '{':
			stack[st_size++]='{';
			break;
		case '[':		
			stack[st_size++]='[';
			break;
		case ']':
			if (st_size && stack[st_size-1]=='[')
				st_size--;
			break;
		case '}':
			if (st_size &&  stack[st_size-1]=='{')
				st_size--;
			break;
		case '\"':
			comillas=!comillas;
			break;
		}
		p++;
	}
	from = p+1;
	return ans.SubString(p1,p-1);
}

bool DebugManager::SelectFrame(long frame_id, long frame_level) {
	if (frame_level==-1) frame_level = GetFrameLevel(frame_id); 
	else if (frame_id==-1) frame_id = GetFrameID(frame_level); 
	GDBAnswer &ans = SendCommand("-stack-select-frame ",frame_level);
	if (ans.result.StartsWith("^done")) { current_frame_id = frame_id; return true; }
	else return false;
}

bool DebugManager::DoThat(wxString what) {
	wxMessageBox(SendCommand(what).full,what,wxOK,main_window);
	return true;
}


/**
* @brief Agrega los breakpoints y watchpoints a la tabla de puntos de interrupcion
**/
void DebugManager::PopulateBreakpointsList(mxBreakList *break_list, bool also_watchpoints) {
	wxGrid *grid=break_list->grid;
	if (debug->running) return;
	grid->ClearGrid();
	wxString ans = SendCommand("-break-list").result;
	int p=ans.Find("body=[")+6;
	wxString item=GetNextItem(ans,p);
	while (item.Mid(0,5)=="bkpt=") {
		item=item.Mid(6); 
		wxString type=GetValueFromAns(item,"type",true);
		wxString catch_type=GetValueFromAns(item,"catch-type",true);
		if (catch_type.IsEmpty() && type=="breakpoint") {
			long id=-1; GetValueFromAns(item,"number",true).ToLong(&id); 
			BreakPointInfo *bpi=BreakPointInfo::FindFromNumber(id,true);
			int cont=break_list->AppendRow(bpi?bpi->zinjai_id:-1);
			grid->SetCellValue(cont,BL_COL_TYPE,"bkpt");
			grid->SetCellValue(cont,BL_COL_HIT,GetValueFromAns(item,"times",true));
			if (GetValueFromAns(item,"enabled",true)=="y") {
				if (GetValueFromAns(item,"disp",true)=="dis")
					grid->SetCellValue(cont,BL_COL_ENABLE,"once");
				else
					grid->SetCellValue(cont,BL_COL_ENABLE,"enabled");
			} else
				grid->SetCellValue(cont,BL_COL_ENABLE,"diabled");
			wxString fname = GetValueFromAns(item,"fullname",true);
			if (!fname.Len()) fname = GetValueFromAns(item,"file",true);
			grid->SetCellValue(cont,BL_COL_WHY,fname + ": line " +GetValueFromAns(item,"line",true));
			grid->SetCellValue(cont,BL_COL_COND,GetValueFromAns(item,"cond",true));
		} else if (also_watchpoints) {
			if(type.Contains("watchpoint") || !catch_type.IsEmpty()) {
				int cont=break_list->AppendRow(-1);
				if (!catch_type.IsEmpty()) {
					grid->SetCellValue(cont,BL_COL_TYPE,"catch");
				} else {
					if (type.Mid(0,3)=="rea") {
						grid->SetCellValue(cont,BL_COL_TYPE,"w(l)");
					} else if (type.Mid(0,3)=="acc") {
						grid->SetCellValue(cont,BL_COL_TYPE,"w(e)");
					} else /*if (type.Mid(0,3)==_("acc"))*/ {
						grid->SetCellValue(cont,BL_COL_TYPE,"w(l/e)");
					}
				}
				grid->SetCellValue(cont,BL_COL_HIT,GetValueFromAns(item,"times",true));
				grid->SetCellValue(cont,BL_COL_ENABLE,GetValueFromAns(item,"enabled",true)=="y"?"enabled":"disabled");
				grid->SetCellValue(cont,BL_COL_WHY,
								   /*mxUT::UnEscapeString(*/GetValueFromAns(item,"number",true/*)*/) + ": "
														+GetValueFromAns(item,"what",true) );
			}
		}
		item=GetNextItem(ans,p);
	}
}

/// @brief Define the number of time the breakpoint should be ignored before actually stopping the execution
void DebugManager::SetBreakPointOptions(int num, int ignore_count) {
	wxString cmd("-break-after ");
	cmd<<num<<" "<<ignore_count;
	SendCommand(cmd);
}

/// @brief Define the condition for a conditional breakpoint and returns true if the condition was correctly setted
bool DebugManager::SetBreakPointOptions(int num, wxString condition) {
	wxString cmd("-break-condition ");
	cmd<<num<<" "<<mxUT::EscapeString(condition);
	GDBAnswer &ans = SendCommand(cmd);
	return ans.result.StartsWith("^done");
}

void DebugManager::SetBreakPointEnable(BreakPointInfo *_bpi) {
	wxString cmd("-break-");
	if (_bpi->enabled) {
		cmd<<(_bpi->action==BPA_STOP_ONCE?"enable once ":"enable ");
	} else {
		cmd<<"disable ";
	}
	cmd<<_bpi->gdb_id;
	/*wxString ans = */SendCommand(cmd);
}


void DebugManager::LiveSetBreakPointEnable(BreakPointInfo *_bpi) {
	if (debugging && waiting) {
		_DEBUG_LAMBDA_1( lmbEnableBreakpoint, BreakPointInfo,p, { debug->SetBreakPointEnable(p); } );
		PauseFor(new lmbEnableBreakpoint(_bpi));
	} else {
		SetBreakPointEnable(_bpi);
	}
}

// //**
//* @brief find a BreakPointInfo that matches a gdb_id or zinjai_id
//*
//* If use_gdb finds a breakpoint that matches gdb_id, else 
//* finds a breakpoint that matches zinjai_id. If there's no match
//* returns nullptr.
//*
//* This method searchs first in opened files (main_window->notebook_sources)
//* starting by the current source, then searchs in project's files if there
//* is a project.
//**/
//BreakPointInfo *DebugManager::FindBreakInfoFromNumber(int _id, bool use_gdb_id) {
//	int sc = main_window->notebook_sources->GetPageCount();
//	if (sc) {
//		// buscar primero en el fuente acutal (siempre deberia estar ahi)
//		mxSource *source = (mxSource*)(main_window->notebook_sources->GetPage(main_window->notebook_sources->GetSelection()));
//		BreakPointInfo *bpi= *source->breaklist;
//		while (bpi && ((use_gdb_id&&bpi->gdb_id!=_id)||(!use_gdb_id&&bpi->zinjai_id!=_id))) bpi=bpi->Next();
//		if (bpi) return bpi;
//		// si no lo encontro buscar en el resto de los fuentes abiertos
//		for (int i=0;i<sc;i++) {
//			source = (mxSource*)(main_window->notebook_sources->GetPage(i));
//			bpi = *source->breaklist;
//			while (bpi && bpi->gdb_id!=_id) bpi=bpi->Next();
//			if (bpi) return bpi;
//		}
//	}
//	if (project) {
//		for(int i=0;i<2;i++) { 
//			file_item *file =i==0?project->first_source:project->first_header;
//			ML_ITERATE(file) {
//				BreakPointInfo *bpi = file->breaklist;
//				while (bpi && bpi->gdb_id!=_id)
//					bpi = bpi->Next();
//				if (bpi) return bpi;
//				file = file->next;
//			}
//		}
//	}
//	return nullptr;
// }

int DebugManager::GetBreakHitCount(int num) {
	long l=0;
	GDBAnswer &ans = SendCommand("-break-info ",num);
	int p = ans.result.Find("times=");
	if (p==wxNOT_FOUND) return -1;
	GetValueFromAns(ans.result.Mid(p),"times",true).ToLong(&l);
	return l;
}

bool DebugManager::SaveCoreDump(wxString core_file) {
	if (!debugging || waiting) return false;
	SendCommand(wxString(_T("gcore "))<<core_file);
	return true;
}

wxString DebugManager::GetMacroOutput(wxString cmd, bool keep_endl) {
	GDBAnswer &ans = SendCommand(cmd), ret;
	if (ans.result.StartsWith("^done")) {
		wxString stream = ans.stream;
		if (!keep_endl) mxUT::RemoveCharInplace(stream,'\n');
		int d=0, l=stream.Len();
		for(int p=0; p<l;++p) {
			if(stream[p]=='\''||stream[p]=='\"') {
				stream[p-d] = stream[p];
				char c = stream[p++]; 
				while (p<l && stream[p]!=c) { 
					stream[p-d]=stream[p];
					if(stream[p++]=='\\') {
						stream[p-d]=stream[p];
						++p; 
					}
				}
				stream[p-d]=stream[p];
			} else if(stream[p]=='$') {
				int p0 = p;
				while (p<l && stream[p]!=' ') p++; // skip the $number
				if (p<l) p++; // skip the space
				while (p<l && stream[p]!=' ') p++; // skip the equal sign
				// then cames another space
				d+=p-p0+1;
			} else {
				stream[p-d]=stream[p];
			}
		}
		if (d) stream=stream.Mid(0,l-d);
		
		return stream;
	} else  
		return ans.result;
}

bool DebugManager::EnableInverseExec() {
	if (recording_for_reverse) {
		if (inverse_exec) ToggleInverseExec();
		/*wxString ans = */SendCommand("record stop");
		recording_for_reverse=false;
	} else {
		GDBAnswer &ans=SendCommand("record");
		if (ans.result.StartsWith("^done")) {
			recording_for_reverse=true;
			return true;
		} else 
			mxMessageDialog(main_window,LANG(DEBUG_ERROR_REVERSE,""
											 "Ha ocurrido un error al intentar activar esta caracteristica.\n"
											 "Para utilizarla debe instalar gdb version 7.0 o superior.\n"
											 "Adem�s, no todas las plataformas soportan este tipo de ejecucion."))
				.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
	}
	return false;
}

bool DebugManager::ToggleInverseExec() {
	if (!debugging || waiting) return false;
	if (recording_for_reverse) {
		GDBAnswer &ans = inverse_exec ? SendCommand("-gdb-set exec-direction forward"):SendCommand("-gdb-set exec-direction reverse");
		if (ans.result.StartsWith("^done")) inverse_exec=!inverse_exec;
	} else {
		mxMessageDialog(main_window,LANG(DEBUG_REVERSE_DISABLED,""
										 "Solo se puede retroceder la ejecucion hasta el punto en donde la\n"
										 "ejecucion hacia atras fue habilitada. Actualmente esta caracteristica\n"
										 "no esta habilitada. Utilice el comando \"Habilitar Ejecucion Hacia Atras\"\n"
										 "del menu de Depuracion para habilitarla."))
			.Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
	}
	return inverse_exec;
}

void DebugManager::UnregisterSource(mxSource *src) {
	if (current_source==src) current_source=nullptr;
	if (notitle_source==src) notitle_source=nullptr;
}

void DebugManager::UpdateThreads() {
	wxString ans = SendCommand("-thread-list-ids").result;
	if (ans.Contains("number-of-threads=\"0\"")) {
		main_window->threadlist_ctrl->SetData(0,"-",LANG(THREADS_NO_THREADS,"<<No hay hilos>>"),"--","--");
		main_window->threadlist_ctrl->SetNumber(1);
	} else {
		wxString cur=ans.Mid(ans.Find("current-thread-id=")+16).BeforeFirst('\"');
		ans=GetValueFromAns(ans,"thread-ids");
		int c=0;
		wxString id,det,file,line,func;
		int i=ans.Find("thread-id=");
		while (i!=wxNOT_FOUND) {
			ans=ans.Mid(i+11);
			id=ans.BeforeFirst('\"');
			det = SendCommand("-thread-info ",id).result;
			i = det.Find("threads=");
			func.Clear();
			if (i!=wxNOT_FOUND) {
				det=GetValueFromAns(det.Mid(i+10),"frame");
				if (det.Len()) {
					det=det.Mid(1);
					func=GetValueFromAns(det,"func");
					file=GetValueFromAns(det,"file");
					if (!file.Len()) file=GetValueFromAns(det,"fullname");
					line=GetValueFromAns(det,"line");
				}
			}
			if (!func.Len()) {
				func=LANG(THREADS_NO_INFO,"<<Informacion no disponible>>");
				file.Clear(); line.Clear();
			}
			main_window->threadlist_ctrl->SetData(c++,id,func,file,line);
			if (id==cur) main_window->threadlist_ctrl->SelectRow(c-1);
			i=ans.Find("thread-id=");
		}
		main_window->threadlist_ctrl->SetNumber(c);
	}
}

void DebugManager::ThreadListClean() {
	main_window->threadlist_ctrl->SetData(0,"","","","");
	main_window->threadlist_ctrl->SetNumber(1);
}

bool DebugManager::SelectThread(long thread_id) {
	GDBAnswer &ans = SendCommand("-thread-select ",thread_id);
	if (!ans.result.StartsWith("^done")) return false;
	current_thread_id = thread_id; 
	current_frame_id = GetFrameID(0);
	return true;
}

void DebugManager::SetFullOutput (bool on, bool force) {
	static bool is_full=false;
	if (!force&&is_full==on) return;
	if (on) SendCommand("set print elements 0");
	else SendCommand("set print elements 100"); /// @todo: put this number in preferences
	is_full=on;
}

void DebugManager::ShowBreakPointLocationErrorMessage (BreakPointInfo *_bpi) {
	static bool show_breakpoint_error=true;
	if (!show_breakpoint_error) return;
	mxMessageDialog::mdAns res = mxMessageDialog(main_window,
		wxString(LANG(DEBUG_BAD_BREAKPOINT_WARNING,"El depurador no pudo colocar un punto de interrupcion en:"))<<
		"\n"<<_bpi->fname<<": "<<_bpi->line_number+1<<"\n"<<
		LANG(DEBUG_BAD_BREAKPOINT_WARNING_LOCATION,"Las posibles causas son:\n"
			"* Fue colocado en un archivo que no se compila en el proyecto/programa.\n"
			"* Fue colocado en una linea que no genera codigo ejecutable (ej: comentario).\n"
			"* Informaci�n de depuraci�n desactualizada o inexistente. Intente recompilar\n"
			"   completamente el programa/proyecto, utilizando el item Limpiar del menu Ejecucion\n"
			"   antes de depurar.\n"
			"* Espacios o acentos en las rutas de los archivos fuente. Si sus directorios contienen\n"
			"   espacios o acentos en sus nombres pruebe renombrarlos o mover el proyecto."))
			.Title(LANG(GENERAL_WARNING,"Aviso")).IconWarning()
			.Check1("No volver a mostrar este mensaje",false).Run();
	if (res.check1) show_breakpoint_error=false;
}

void DebugManager::ShowBreakPointConditionErrorMessage (BreakPointInfo *_bpi) {
	static bool show_breakpoint_error=true;
	if (!show_breakpoint_error) return;
	mxMessageDialog::mdAns res = mxMessageDialog(main_window,
		wxString(LANG(DEBUG_BAD_BREAKPOINT_WARNING,"El depurador no pudo colocar un punto de interrupcion en:"))<<
		"\n"<<_bpi->fname<<": "<<_bpi->line_number+1<<"\n"<<
		LANG(DEBUG_BAD_BREAKPOINT_WARNING_CONDITION,"La condici�n ingresada no es v�lida."))
		.Title(LANG(GENERAL_WARNING,"Aviso")).IconWarning().Check1("No volver a mostrar este mensaje",false).Run();
	if (res.check1) show_breakpoint_error=false;
}

void DebugManager::SendSignal (const wxString & signame) {
	if (waiting || !debugging || status==DBGST_STOPPING || status==DBGST_WAITINGKEY) return;
	MarkCurrentPoint();
	GDBAnswer &ans = SendCommand("signal ",signame);
	if (ans.result.StartsWith("^running")) {
		HowDoesItRuns(true);
	}
}

bool DebugManager::GetSignals(vector<SignalHandlingInfo> & v) {
	wxString info; v.clear();
	if (debugging) {
		if (waiting) {
			mxMessageDialog(main_window,"Debe pausar o detener la ejecuci�n para modificar el comportamiento ante se�ales.")
				.Title(LANG(GENERAL_ERROR,"Error")).IconInfo().Run(); 
			return false;
		} else {
			GDBAnswer &ans = SendCommand("info signals");
			if (!ans.result.StartsWith("^done")) return false;
			info = ans.full;
		}
	} else {
		info = mxUT::GetOutput("gdb --interpreter=mi --quiet --batch -ex \"info signal\"",true);
	}
	while (info.Contains('\n')) {
		wxString line=info.BeforeFirst('\n');
		info=info.AfterFirst('\n');
		// para saber si esta linea es una se�al o no, vemos si empieza con ~"XXX, con XXX may�sculas 
		// otras lineas son por ej la cabecera de la tabla (~"Signal...), lineas en blanco /~"\n"), o de ayuda (~"Use...)
		if (! (line.Len()>4 && line[0]=='~' && line[1]=='\"' && (line[2]>='A'&&line[2]<='Z') && (line[3]>='A'&&line[3]<='Z') && (line[4]>='A'&&line[4]<='Z') ) ) continue;
		line = mxUT::UnEscapeString(line.Mid(1),true);
		SignalHandlingInfo si;
		// la primer palabra es el nombre
		int i=0,i0=0, l=line.Len();
		while (i<l && (line[i]!=' '&&line[i]!='\t')) i++;
		si.name=line.Mid(i0,i-i0);
		// la segunda es un Yes/No que corresponde a Stop
		while (i<l && (line[i]==' '||line[i]=='\t')) i++;
		i0=i;
		while (i<l && (line[i]!=' '&&line[i]!='\t')) i++;
		si.stop=line.Mid(i0,i-i0)=="Yes";
		// la segunda es un Yes/No que corresponde a Print
		while (i<l && (line[i]==' '||line[i]=='\t')) i++;
		i0=i;
		while (i<l && (line[i]!=' '&&line[i]!='\t')) i++;
		si.print=line.Mid(i0,i-i0)=="Yes";
		// la segunda es un Yes/No que corresponde a Pass
		while (i<l && (line[i]==' '||line[i]=='\t')) i++;
		i0=i;
		while (i<l && (line[i]!=' '&&line[i]!='\t')) i++;
		si.pass=line.Mid(i0,i-i0)=="Yes";
		// lo que queda es la descripcion
		while (i<l && (line[i]==' '||line[i]=='\t')) i++;
		si.description=line.Mid(i);
		if (si.description.EndsWith("\n")) si.description.RemoveLast();
		v.push_back(si);
	}
	if (!signal_handlers_state) {
		signal_handlers_state = new vector<SignalHandlingInfo>[2];
		signal_handlers_state[0]=signal_handlers_state[1]=v;
	}
	return true;
}

bool DebugManager::SetSignalHandling (SignalHandlingInfo & si, int i) {
	if (debugging) {
		if (waiting) return false;
		wxString cmd("handle "); cmd<<si.name;
		cmd<<" "<<(si.print?"print":"noprint");
		cmd<<" "<<(si.stop?"stop":"nostop");
		cmd<<" "<<(si.pass?"pass":"nopass");
		GDBAnswer &ans = SendCommand(cmd);
		if (!ans.result.StartsWith("^done")) return false;
	}
	if (i!=-1 && signal_handlers_state) signal_handlers_state[1][i]=si;
	return true;
}

void DebugManager::Start_ConfigureGdb ( ) {
	// averiguar la version de gdb
	gdb_version = 0;
	wxString ver = SendCommand("-gdb-version").stream;
	unsigned int i=0; 
	bool have_main_version=false;
	while (i<ver.Len() && !have_main_version) {
		if (i+2<ver.Len() && ver[i]==' ' && (ver[i+1]>='0'&&ver[i+1]<='9') && ver[i+2]=='.') { // 1-digit main version
			gdb_version=(ver[i+1]-'0')*1000; have_main_version=true; i+=2;
		} else if (i+3<ver.Len() && ver[i]!=' ' && (ver[i+1]>='0'&&ver[i+1]<='9') && (ver[i+2]>='0'&&ver[i+2]<='9') && ver[i+3]=='.') { // 2-digits main version
			gdb_version=(ver[i+1]-'0')*10000+(ver[i+2]-'0')*1000; have_main_version=true; i+=3;
		}
		if (have_main_version) {
			if (i+1<ver.Len() && (ver[i+1]>='0'&&ver[i+1]<='9')) {
				if (i+2<ver.Len() && (ver[i+2]>='0'&&ver[i+2]<='9')) // 2-digits sub version
					gdb_version+=(ver[i+1]-'0')*10+(ver[i+2]-'0');
				else // 1-digit sub version
					gdb_version+=(ver[i+1]-'0');
			}	
		}
		i++;
	}

#ifdef __WIN32__
	if (config->Debug.no_debug_heap) SendCommand("set environment _NO_DEBUG_HEAP 1");
#endif
	// configurar comportamiento ante se�ales
	if (signal_handlers_state) {
		for(unsigned int i=0;i<signal_handlers_state[0].size();i++) { 
			if (signal_handlers_state[0][i]!=signal_handlers_state[1][i])
				debug->SetSignalHandling(signal_handlers_state[1][i]);
		}
	}
	// otras configuraciones varias
	if (config->Debug.catch_throw) SendCommand("catch throw");
	if (!config->Debug.auto_solibs) SendCommand("set auto-solib-add off");
//	SendCommand("set print addr off"); // necesito las direcciones para los helpers de los arreglos
	SendCommand(_T("set print repeats 0"));
#ifdef __APPLE__
	SendCommand("set startup-with-shell off");
#endif
	SetFullOutput(false,true);
	SetBlacklist();
	
	// macros para gdb generales
	if (wxFileName(config->Debug.macros_file).FileExists())
		SendCommand("source ",mxUT::Quotize(config->Debug.macros_file));
	else if (wxFileName(config->GetZinjaiSamplesPath(config->Debug.macros_file)).FileExists())
		SendCommand("source ",mxUT::Quotize(config->GetZinjaiSamplesPath(config->Debug.macros_file)));
	// macros para gdb del proyecto
	if (project && project->macros_file.Len() && wxFileName(DIR_PLUS_FILE(project->path,project->macros_file)).FileExists())
		SendCommand("source ",mxUT::Quotize(DIR_PLUS_FILE(project->path,project->macros_file)));
	
	// reiniciar sistema de inspecciones
	DebuggerInspection::OnDebugStart();
	for(int i=0;i<backtrace_consumers.GetSize();i++) backtrace_consumers[i]->OnDebugStart();
}

void DebugManager::Initialize() {
	debug = new DebugManager();
}

void DebugManager::TemporaryScopeChange::ChangeIfNeeded(DebuggerInspection *di) {
	if (!di->IsFrameless()) ChangeTo(di->GetFrameID(),di->GetThreadID());
}

bool DebugManager::PauseFor (OnPauseAction * action) {
	if (!debugging) { delete action; return false; } // si no estamos depurando, no hacer nada (no deberia pasar)
	if (!waiting) { action->Run(); delete action; return true; } // si esta en pausa ejecuta en el momento (no deberia pasar)
	if (on_pause_action) return false; // por ahora no se puede encolar mas de una accion
	on_pause_action = action; // encolar la accion
	Pause(); // pausar para que se ejecute
	return true;
}

void DebugManager::InvalidatePauseEvent(void *ptr) {
	if (!on_pause_action) return;
	if (!on_pause_action->Invalidate(ptr)) return;
	delete on_pause_action; on_pause_action=nullptr;
}

/// @retval el numero id en gdb si lo agrego, "" si no pudo
wxString DebugManager::AddWatchPoint (const wxString &expression, bool read, bool write) {
	if (expression.IsEmpty()) return "";
	GDBAnswer &ans = SendCommand("-break-watch ",mxUT::EscapeString(expression));
	if (!ans.result.StartsWith("^done")) return "";
	wxString num = GetSubValueFromAns(ans.result,"wpt","number",true);
	watchpoints[num] = expression;
	return num;
}

bool DebugManager::DeleteWatchPoint (const wxString & num) {
	GDBAnswer &ans = SendCommand("-break-delete ",num);
	return ans.result.StartsWith("^done");
}

void DebugManager::SetBlacklist (bool clear_first) {
	if (!CanTalkToGDB()) return;
	if (clear_first) SendCommand("skip delete");
	if (config->Debug.use_blacklist) {
		for(unsigned int i=0;i<config->Debug.blacklist.GetCount();i++)
			SendCommand("skip ",config->Debug.blacklist[i]);
	}
}

void DebugManager::Patch ( ) {
	if (CanTalkToGDB()) {
		debug_patcher->Patch();
	} else {
		_DEBUG_LAMBDA_0( lmbPatch, { debug->GetPatcher()->Patch(); } );
		PauseFor(new lmbPatch());
	}
}

bool DebugManager::ToggleAutoStep () {
	return (auto_step = !auto_step);
}

myBTEventHandler::myBTEventHandler ( ) {
	debug->backtrace_consumers.Add(this); 
	registered=true;
}

void myBTEventHandler::UnRegister ( ) {
	if (!registered) return;
	int pos = debug->backtrace_consumers.Find(this); 
	if (pos!=debug->backtrace_consumers.NotFound())
		debug->backtrace_consumers.Remove(pos);
	registered=false;
}

myBTEventHandler::~myBTEventHandler ( ) {
	UnRegister();
}

wxString DebugManager::GetCurrentLocation ( ) {
	return (current_source&&current_handle!=-1) ? ( wxString() 
		<< current_source->source_filename.GetFullName() 
		<< " : " 
		<< current_source->MarkerLineFromHandle(current_handle)
							 ) : wxString(LANG(BACKTRACE_NOT_AVAILABLE,"<<informacion no disponible>>"));
}

bool DebugManager::SetFakeBacktrace (const BTInfo & stack) {
	return UpdateBacktrace(stack,false);
}

bool DebugManager::CurrentBacktraceIsReal ( ) {
	return backtrace_is_current;
}

const DebugManager::BTInfo & DebugManager::GetCurrentStackRawData ( ) {
	return current_stack;
}

void DebugManager::SetStepMode (bool asm_mode_on) {
	step_by_asm_instruction = asm_mode_on;
}

bool DebugManager::IsAsmStepModeOn() {
	return step_by_asm_instruction;
}

/// sends Ctrl+C, used to interrupt a command taking too long or generating too much output
bool DebugManager::InterruptOperation ( ) {
	if (process && gdb_pid && process->Exists(gdb_pid)) {
#ifdef __WIN32__
		OSDep::winDebugBreak(child_pid);
#else
		process->Kill(gdb_pid,wxSIGINT);
#endif
		return true;
	}
	return false;
}


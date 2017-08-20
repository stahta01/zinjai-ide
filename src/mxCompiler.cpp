#include <wx/txtstrm.h>
#include "ids.h"
#include "Language.h"
#include "mxMainWindow.h"
#include "mxCompiler.h"
#include "mxAUI.h"
#include "Parser.h"
#include "mxMessageDialog.h"
#include "ProjectManager.h"
#include "ConfigManager.h"
#include "DebugManager.h"
#include "DebugPatcher.h"
#include "mxSource.h"
#include "mxUtils.h"
#include "Toolchain.h"
#include "CodeHelper.h"
#include "mxExternCompilerOutput.h"
#include "CompilerErrorsManager.h"
#include "compiler_strings.h"
#include "EnvVars.h"


#ifdef _ZINJAI_DEBUG
#include<iostream>
#include <cstdlib>
using namespace std;
#endif

wxMutex mxMutexCompiler;

mxCompiler *compiler=nullptr;

static bool EnsureCompilerNotRunning() {
	compile_and_run_struct_single *compile_and_run=compiler->compile_and_run_single;
	while (compile_and_run && !compile_and_run->compiling && !compile_and_run->linking) 
		compile_and_run=compile_and_run->next;
	if (compile_and_run) return false;
	return true;
}

compile_and_run_struct_single::compile_and_run_struct_single(const compile_and_run_struct_single *o) {
	*this=*o; 
	on_end=nullptr; // para evitar doble-delete, igual nunca deberia tener un on_end porque esto solo se usa para fuentes de un proyecto
#ifdef _ZINJAI_DEBUG
	cerr<<"compile_and_run: *"<<mname<<endl;
	count++;
#endif
	mxMutexCompiler.Lock();
	prev=nullptr;
	next=compiler->compile_and_run_single;
	if (next) next->prev=this;
	compiler->compile_and_run_single=this;
	mxMutexCompiler.Unlock();
}

compile_and_run_struct_single::compile_and_run_struct_single(const char *name) {
	error_line_flag=CAR_LL_NULL;
#ifdef _ZINJAI_DEBUG
	mname=name;
	cerr<<"compile_and_run: +"<<name<<endl;
	count++;
#endif
	on_end=nullptr;
	killed=compiling=linking=false;
	output_type=MXC_NULL;
	process=nullptr;
	pid=0;
	valgrind_cmd=compiler->valgrind_cmd;
	mxMutexCompiler.Lock();
	prev=nullptr;
	next=compiler->compile_and_run_single;
	if (next) next->prev=this;
	compiler->compile_and_run_single=this;
	mxMutexCompiler.Unlock();
}

compile_and_run_struct_single::~compile_and_run_struct_single() {
	delete on_end;
	mxMutexCompiler.Lock();
	if (next) next->prev=prev;
	if (prev)
		prev->next=next;
	else
		compiler->compile_and_run_single=next;
	
#ifdef _ZINJAI_DEBUG
	cerr<<"compile_and_run: -"<<mname<<endl;
	count--;
#endif
	mxMutexCompiler.Unlock();
}

wxString compile_and_run_struct_single::GetInfo() {
	wxString info;
	info<<pid<<" ";
	if (compiling) info<<"c";
	if (linking) info<<"l";
#ifdef _ZINJAI_DEBUG
	info<<" "<<mname;
	info<<" "<<step_label;
#endif
	return info;
}

#ifdef _ZINJAI_DEBUG 
int compile_and_run_struct_single::count=0;
#endif

mxCompiler::mxCompiler(/*wxTreeCtrl *atree, wxTreeItemId s, wxTreeItemId e, wxTreeItemId w, wxTreeItemId a*/) {
//	tree=atree; all=a; errors=e; warnings=w; state=s;
	last_runned=nullptr;
	last_compiled=nullptr;
	compile_and_run_single=nullptr;
}

/**
* @param on_end En cualquier caso se hacer cargo de esta accion. Si hay que 
*               compilar se la transfiere al proyecto, si está todo listo
*               la ejecuta y le hace luego el delete.
**/
void mxCompiler::BuildOrRunProject(bool prepared, GenericAction *on_end) {
	RaiiDeletePtr<GenericAction> ga_del(on_end);
	if (project->GetWxfbActivated() && project->GetWxfbConfiguration()->working) return;
	main_window->extern_compiler_output->Clear();
	main_window->SetCompilingStatus(LANG(GENERAL_PREPARING_BUILDING,"Preparando compilación..."),true);
DEBUG_INFO("wxYield:in  mxCompiler::BuildOrRunProject");
	wxYield();
DEBUG_INFO("wxYield:out mxCompiler::BuildOrRunProject");
	if (prepared || project->PrepareForBuilding(nullptr)) { // si hay que compilar/enlazar
		if (!EnsureCompilerNotRunning()) return;
		fms_move( fms_delete(project->post_compile_action), on_end );
		wxString current;
		compile_and_run_struct_single *compile_and_run=new compile_and_run_struct_single("BuildOrRunProject 1");
		project->compile_startup_time = time(nullptr);
		errors_manager->Reset(project);
		for (unsigned int i=0;i<project->warnings.GetCount();i++)
			errors_manager->AddZinjaiError(false,project->warnings[i]);
		compile_and_run->compiling=true;
		compile_and_run->pid=project->CompileNext(compile_and_run,current); // mandar a compilar el primero
		if (compile_and_run->pid) { // si se puso a compilar algo
			if (current_toolchain.IsExtern()) main_window->m_aui->Show(PaneId::Compiler,true);
			main_window->StartExecutionStuff(compile_and_run,current);
		} else {
			main_window->SetStatusText(LANG(MAINW_COMPILATION_INTERRUPTED,"Compilación interrumpida."));
			delete compile_and_run;
		}
	} else {
		if (!project->active_configuration->dont_generate_exe) compiler->CheckForExecutablePermision(project->GetExePath());
		main_window->SetCompilingStatus(LANG(MAINW_BINARY_ALREADY_UPDATED,"El binario ya esta actualizado"),true);
		if (on_end) on_end->Run(); 
	}	
}

static inline bool ErrorLineIsChild(const wxString &error_line) {
	// si el mensaje de error empieza con 3 espacios, es porque en realidad sigue del mensaje anterior
	int p=2,l=error_line.Len();
	while (p<l && error_line[p]!=':') p++; // saltear el nombre del archivo
	if (p==l) return false; else p++;
	while (p<l && error_line[p]!=':') p++; // saltear el numero de linea
	if (p==l) return false; else p++;
	while (p<l && error_line[p]!=':') p++; // saltear el la columna
	if (p==l) return false; else p++;
	if (p+2<l && error_line[p]==' '&&error_line[p+1]==' '&&error_line[p+2]==' ') return true; // puede venir directo el mensaje
	if (p+5<l && error_line[p+1]=='n'&&error_line[p+2]=='o'&&error_line[p+3]=='t'&&error_line[p+5]==':') return true; // o la palabra note
	while (p<l && error_line[p]!=':') p++; // o la palabra error/warning/
	if (p==l) return false; else p++;
	if (p+2<l && error_line[p]==' '&&error_line[p+1]==' '&&error_line[p+2]==' ') return true; // puede venir directo el mensaje
	return false;
}


CAR_ERROR_LINE mxCompiler::ParseSomeErrorsOneLine(compile_and_run_struct_single *compile_and_run, const wxString &error_line) {
	
	// if it starts or continues an "In included file from ....., \n from ...., \n from .....:" list, ignore
	if (compile_and_run->error_line_flag==CAR_LL_IN_INCLUDED_FILE || error_line.Contains(EN_COMPOUT_IN_FILE_INCLUDED_FROM) || error_line.Contains(ES_COMPOUT_IN_FILE_INCLUDED_FROM) ) {
		if (error_line.Last()==',') compile_and_run->error_line_flag=CAR_LL_IN_INCLUDED_FILE;
		else compile_and_run->error_line_flag=CAR_LL_NULL;
		return CAR_EL_TYPE_IGNORE;
	}
	compile_and_run->error_line_flag=CAR_LL_NULL;
	
	// if there was an error within the compiler stop
	if (error_line==EN_COMPOUT_COMPILATION_TERMINATED || error_line==ES_COMPOUT_COMPILATION_TERMINATED) return CAR_EL_TYPE_ERROR;
	
	// if it starts or continue a list of template instantiation chain: "In instantiation of ....: \n required from ...\n required from ..."
	if (error_line.Contains(EN_COMPOUT_IN_INSTANTIATION_OF) || error_line.Contains(ES_COMPOUT_IN_INSTANTIATION_OF)
		|| error_line.Contains(EN_COMPOUT_REQUIRED_FROM) || error_line.Contains(ES_COMPOUT_REQUIRED_FROM)) 
	{
		if (error_line.Contains(EN_COMPOUT_REQUIRED_FROM_HERE) || error_line.Contains(ES_COMPOUT_REQUIRED_FROM_HERE))
			return CAR_EL_TYPE_CHILD_NEXT|CAR_EL_FLAG_NESTED|CAR_EL_FLAG_SWAP|CAR_EL_FLAG_KEEP_LOC|CAR_EL_FLAG_USE_PREV;
		else
			return CAR_EL_TYPE_CHILD_NEXT|CAR_EL_FLAG_NESTED;
	}
	
	// esto va antes de testear si es error o warning, porque ambos tests (por ejemplo, este y el de si es error) pueden dar verdaderos, y en ese caso debe primar este
	if (ErrorLineIsChild(error_line)) {
		if (error_line.Contains(EN_COMPOUT_IN_EXPANSION_OF_MACRO)) 
			return CAR_EL_TYPE_CHILD_LAST|CAR_EL_FLAG_SWAP;
		else
			return CAR_EL_TYPE_CHILD_LAST;
	}
	
	// errores "fatales" (como cuando no encuentra los archivos que tiene que compilar)
	if (error_line.Contains(EN_COMPOUT_FATAL_ERROR) || error_line.Contains(ES_COMPOUT_FATAL_ERROR)) {
		return CAR_EL_TYPE_ERROR;
	}
	
	if (error_line.Contains(EN_COMPOUT_ERROR) || error_line.Contains(ES_COMPOUT_ERROR)) {
		// el "within this contex" avisa el punto dentro de una plantilla, para un error previo que se marco en la llamada a la plantilla, por eso va como hijo
		if (error_line.Contains(EN_COMPOUT_WITHIN_THIS_CONTEXT) || error_line.Contains(ES_COMPOUT_WITHIN_THIS_CONTEXT))
			return CAR_EL_TYPE_CHILD_LAST;
		if (error_line.Contains(EN_COMPOUT_AT_THIS_POINT_IN_FILE) || error_line.Contains(EN_COMPOUT_AT_THIS_POINT_IN_FILE)) // estos errores aparecian en gcc 4.4, ahora cambiaron y ya no los veo
			return CAR_EL_TYPE_CHILD_LAST;
		else return CAR_EL_TYPE_ERROR;
	}
	
	// warnings del compilador
	if (error_line.Contains(EN_COMPOUT_WARNING) || error_line.Contains(ES_COMPOUT_WARNING)) {
		return CAR_EL_TYPE_WARNING;
	}
	
	// warnings del linker
	if (error_line.Contains(EN_COMPOUT_LINKER_WARNING) || error_line.Contains(ES_COMPOUT_LINKER_WARNING)) {
		return CAR_EL_TYPE_WARNING;
	}
	
//	// notas (detectado antes en ErrorLineIsChild(error_line))
//	if (error_line.Contains(EN_COMPOUT_NOTE) || error_line.Contains(ES_COMPOUT_NOTE)) {
//		return CAR_EL_CHILD_LAST;
//	}
	
	// error inside functions has a line before saying "In function 'foo()':"
	if (error_line.Last()==':') return CAR_EL_TYPE_IGNORE;
	
	// anything else? probably linker error
	return CAR_EL_TYPE_ERROR;
	
//	compile_and_run->parsing_errors_was_ok=false;
//	return CAR_EL_UNKNOWN;
}

void mxCompiler::ParseSomeErrors(compile_and_run_struct_single *compile_and_run) {
	if (compile_and_run->output_type==MXC_EXTERN) { ParseSomeExternErrors(compile_and_run); return; }
	
	wxProcess *process = compile_and_run->process;
	wxTextInputStream input(*(process->GetErrorStream()));	
	while ( process->IsErrorAvailable() ) {
		
		wxString error_line=input.ReadLine();
		
		if (compile_and_run->output_type==MXC_EXTRA || error_line.Len()==0) {
			errors_manager->AddExtraOutput(compile_and_run->m_cem_state,error_line);
			continue;
		}
		
		// reemplazar templates para que sea más legible
		
		// averiguar si es error, warning, o parte de un error/warning anterior/siguiente
		CAR_ERROR_LINE action = ParseSomeErrorsOneLine(compile_and_run,error_line);
		
//		// no mostrar taaantos errores/warnings	
#warning TODO: reestablecer el limite
//		if (action==CAR_EL_ERROR) {
//			num_errors++; 
//			if (num_errors>config->Init.max_errors) {
//				compile_and_run->process->Kill(compile_and_run->pid,wxSIGKILL); // matar el proceso, por si entra en loop infinito, podría colgar zinjai
//				compile_and_run->last_error_item_IsOk=false;
//				continue;
//			}
//			compile_and_run->last_error_item_IsOk=false;
//		} else if (action==CAR_EL_WARNING) { 
//			num_warnings++; 
//			if (num_warnings>config->Init.max_errors) {
//				compile_and_run->last_error_item_IsOk=false;
//				continue;
//			}
//		} else if (action==CAR_EL_CHILD_LAST||action==CAR_EL_CHILD_SWAP) {
//			if (!compile_and_run->last_error_item_IsOk && (num_warnings>config->Init.max_errors||num_errors>config->Init.max_errors)) {
//				compile_and_run->process->Kill(compile_and_run->pid,wxSIGKILL); // matar el proceso, por si entra en loop infinito, podría colgar zinjai
//				continue;
//			}
//		}
		
		if (action&(CAR_EL_TYPE_ERROR|CAR_EL_TYPE_WARNING)) { // nuevo error o warning
			errors_manager->AddError(compile_and_run->m_cem_state,action&CAR_EL_TYPE_ERROR,error_line);
		} else if (action&CAR_EL_TYPE_CHILD_LAST) { // continua el último error
			errors_manager->AddNoteForLastOne(compile_and_run->m_cem_state,error_line,action);
		} else if (action&CAR_EL_TYPE_CHILD_NEXT) { // parte (hijos) del siguiente error
			errors_manager->AddNoteForNextOne(compile_and_run->m_cem_state,error_line,action);
		}
			
	}
}

void mxCompiler::ParseSomeExternErrors(compile_and_run_struct_single *compile_and_run) {
	wxProcess *process = compile_and_run->process;
	static wxString error_line, nice_error_line;
	wxTextInputStream input1(*(process->GetInputStream()));	
	while ( process->IsInputAvailable() ) {
		error_line=input1.ReadLine();
		if (error_line.Len()==0) continue;
		errors_manager->AddExtraOutput(compile_and_run->m_cem_state,error_line);
		main_window->extern_compiler_output->AddLine("< ",error_line);
	}
	wxTextInputStream input2(*(process->GetErrorStream()));	
	while ( process->IsErrorAvailable() ) {
		error_line=input2.ReadLine();
		if (error_line.Len()==0) continue;
		errors_manager->AddExtraOutput(compile_and_run->m_cem_state,error_line);
		main_window->extern_compiler_output->AddLine("!! ",error_line);
	}
}

void mxCompiler::ParseCompilerOutput(compile_and_run_struct_single *compile_and_run, bool success) {
	
	ParseSomeErrors(compile_and_run); // poner los errores/warnings/etc en el arbol
	bool cem_ok = errors_manager->FinishStep(compile_and_run->m_cem_state);
	
	if (!compile_and_run_single->killed && !cem_ok) {
		mxMessageDialog(main_window,LANG(MAINW_COMPILER_OUTPUT_PARSING_ERROR,""
				 "ZinjaI ha intentado reacomodar la salida del compilador de forma incorrecta.\n"
				 "Puede que algun error no se muestre correctamente en el arbol. Para ver la salida\n"
				 "completa haga click con el boton derecho del raton en cualquier elemento del arbol\n"
				 "y seleccione \"abrir ultima salida\". Para contribuir al desarrollo de ZinjaI puede\n"
				 "enviar esta salida, o el codigo que la ocasiono a zaskar_84@yahoo.com.ar"
			 )).Title(LANG(GENERAL_ERROR,"Error")).IconError().Run();
	}
	
	delete compile_and_run->process; compile_and_run->process=nullptr; // liberar el proceso
	
	// ver como sigue
	if (success) { // si el proceso termino correctamente (no hay errores/problemas)
		if (project && (compile_and_run->compiling||compile_and_run->linking)) { // si es proyecto
			
//			compile_and_run->compiling=false; // para que se pone en falso?
			
			wxString current;
			if ((project->compile_was_ok||!config->Init.stop_compiling_on_error) && project->CompileNext(compile_and_run,current)) { // si se puso a compilar algo
				main_window->StartExecutionStuff(compile_and_run,current);
			} else {
				if (compiler->IsCompiling()) { // si queda otro en paralelo
					delete compile_and_run; return;
				}
				if (project->compile_was_ok) { // si esta todo listo, informar el resultado
					if (compile_and_run->linking && !project->active_configuration->dont_generate_exe) CheckForExecutablePermision(project->GetExePath());
					main_window->SetStatusProgress(0);
					time_t elapsed_time = time(nullptr)-project->compile_startup_time;
					if (elapsed_time>5) {
						if (elapsed_time/60==0)
							main_window->SetCompilingStatus(LANG1(MAINW_COMPILING_DONE_SECONDS,"Compilación finalizada ( tiempo transcurrido: <{1}> segundos ).",wxString()<<elapsed_time));
						if (elapsed_time/60==1)
							main_window->SetCompilingStatus(LANG1(MAINW_COMPILING_DONE_ONE_MINUTE,"Compilación finalizada ( tiempo transcurrido: un minuto y <{1}> segundos ).",wxString()<<(elapsed_time%60)));
						else
							main_window->SetCompilingStatus(LANG2(MAINW_COMPILING_DONE_MINUTES_AND_SECONDS,"Compilación finalizada ( tiempo transcurrido: <{1}> minutos y <{2}> segundos ).",wxString()<<(elapsed_time/60),wxString()<<(elapsed_time%60)));
					} else {
						main_window->SetCompilingStatus(LANG(MAINW_COMPILING_DONE,"Compilación finalizada."));
					}
					errors_manager->CompilationFinished();
//					main_window->SetCompilingStatus(LANG(MAINW_COMPILING_DONE,"Compilación Finalizada"));
					// ejecutar o depurar
					GenericAction *on_end = fms_move(compile_and_run->on_end);
					valgrind_cmd=compile_and_run->valgrind_cmd;
					delete compile_and_run; compile_and_run=nullptr;
					if (on_end) { on_end->Run(); delete on_end; }
					valgrind_cmd="";
				} else {
					compile_and_run->compiling=false;
					main_window->SetCompilingStatus(LANG(MAINW_COMPILATION_INTERRUPTED,"Compilación interrumpida!"),true);
					main_window->SetStatusProgress(0);
					main_window->m_aui->Show(PaneId::Compiler,true);
					delete compile_and_run; compile_and_run=nullptr;
				}
			}
		} else { // si era un ejercicio simple, ver si hay que ejecutar
			CheckForExecutablePermision(last_compiled->GetBinaryFileName().GetFullPath());
			main_window->SetStatusProgress(0);
			main_window->SetCompilingStatus(LANG(MAINW_COMPILING_DONE,"Compilación Finalizada"),true);
			errors_manager->CompilationFinished();
			GenericAction *on_end = fms_move(compile_and_run->on_end);
			valgrind_cmd=compile_and_run->valgrind_cmd;
			delete compile_and_run; compile_and_run=nullptr;
			if (on_end) { on_end->Run(); delete on_end; }
			valgrind_cmd="";
		}
	} else { // si fallo la compilacion
		// informar y no seguir
		if (!project || project->compile_was_ok) {
			main_window->SetStatusProgress(0);
			main_window->SetCompilingStatus(LANG(MAINW_COMPILATION_INTERRUPTED,"Compilación interrumpida!"),true);
			compile_and_run->compiling=false;
//			wxBell();
			if (compile_and_run->output_type==MXC_EXTRA)
				errors_manager->AddZinjaiError(true,wxString(LANG(MAINW_COMPILATION_CUSTOM_STEP_ERROR,"Error al ejecutar paso de compilación personalizado: "))<<compile_and_run->step_label);
			errors_manager->CompilationFinished();
			main_window->SetFocusToSourceAfterEvents();
			if (!project) g_code_helper->TryToSuggestTemplateSolutionForLinkingErrors(compile_and_run->m_cem_state.full_output,compile_and_run->on_end);
		}
		if (project) {
			project->compile_was_ok=false;
			if (!config->Init.stop_compiling_on_error) {
				wxString current;
				 // mandar a compilar el que sigue
				if (project->CompileNext(compile_and_run,current)) { // si se puso a compilar algo
					main_window->StartExecutionStuff(compile_and_run,current);
					return; // avoid final delete
				} else {
					main_window->SetStatusProgress(0);
					main_window->SetStatusText(LANG(MAINW_COMPILATION_INTERRUPTED,"Compilación interrumpida!"));
				}
			}
		}
		delete compile_and_run;
	}
}

/**
* @param on_end Si puede ponerse a compilar, le pasa esta accion al proceso de 
*               compilacion, sino la destruye, pero en cualquier caso ya no le
*               pertenece a quien invoque a esta funcion.
**/
void mxCompiler::CompileSource (mxSource *source, GenericAction *on_end) {
	RaiiDeletePtr<GenericAction> oe_del(on_end);
	if (!EnsureCompilerNotRunning()) return;
	compile_and_run_struct_single *compile_and_run=new compile_and_run_struct_single("CompileSource");
	fms_move(compile_and_run->on_end,on_end);
	compile_and_run->output_type=MXC_GCC;
	parser->ParseSource(source,true);
	last_compiled=source;
	wxString z_opts(wxString(" "));
	bool cpp = source->IsCppOrJustC();
	if (config->Debug.format.Len()) z_opts<<config->Debug.format<<" ";
	z_opts<<(cpp?current_toolchain.cpp_compiling_options:current_toolchain.c_compiling_options)<<" "; // forced compiler arguments
	z_opts<<(cpp?current_toolchain.cpp_linker_options:current_toolchain.c_linker_options)<<" "; // forced linker arguments
	z_opts<<current_toolchain.GetExtraCompilingArguments(cpp);
	z_opts<<" -g "; // always include debugging information
	if (!source->sin_titulo) { // avoid not recognizing files without extension
		eFileType ftype = mxUT::GetFileType(source->source_filename.GetFullName(),false);
		if (ftype!=FT_HEADER&&ftype!=FT_SOURCE) z_opts<<(source->IsCppOrJustC()?"-x c++ ":"-x c "); 
	}
	// prepare command line
	wxString comp_opts = source->GetCompilerOptions();
	wxString output_file = source->GetBinaryFileName().GetFullPath();
	if (debug->IsDebugging()) debug->GetPatcher()->AlterOutputFileName(output_file);
	wxString command = wxString(cpp?current_toolchain.cpp_compiler:current_toolchain.c_compiler)
					   +z_opts+" "+mxUT::Quotize(source->GetFullPath())+" "+comp_opts+" -o "+mxUT::Quotize(output_file);
	
	// lanzar la ejecucion
	compile_and_run->process=new wxProcess(main_window->GetEventHandler(),mxPROCESS_COMPILE);
	compile_and_run->process->Redirect();
	
	errors_manager->Reset(false);
	
	EnvVars::SetMode(EnvVars::COMPILING);
	compile_and_run->m_cem_state = errors_manager->InitCompiling(command);
	compile_and_run->pid = mxUT::Execute(source->GetPath(false),command,wxEXEC_ASYNC,compile_and_run->process);
	compile_and_run->compiling=true;
	main_window->StartExecutionStuff(compile_and_run,LANG(MAINW_COMPILING_DOTS,"Compilando..."));
}

int mxCompiler::NumCompilers() {
	int n=0;
	mxMutexCompiler.Lock();
	compile_and_run_struct_single *item = compile_and_run_single;
	while (item) {
		if (item->compiling && item->pid!=0) n++;
		item=item->next;
	}
	mxMutexCompiler.Unlock();
	return n;
}
bool mxCompiler::IsCompiling() {
	mxMutexCompiler.Lock();
	compile_and_run_struct_single *item = compile_and_run_single;
	while (item) {
		if (item->compiling && item->pid!=0) {
			mxMutexCompiler.Unlock();
			return true;
		}
		item=item->next;
	}
	mxMutexCompiler.Unlock();
	return false;
}

wxString mxCompiler::GetCompilingStatusText() {
	wxString text=LANG(PROJMNGR_COMPILING,"Compilando");
	char sep=' ',com='\"';
	compile_and_run_struct_single *item = compile_and_run_single;
	while (item) {
		if (item->compiling && item->pid!=0) {
			text<<sep<<com<<item->step_label<<com;
			sep=',';
		}
		item=item->next;
	}
	text<<_T("\"...");
	return text;
}

bool mxCompiler::CheckForExecutablePermision(wxString file) {
#ifndef __WIN32__
	if (!wxFileName::IsFileExecutable(file)) {
		system((string("chmod a+x ")+mxUT::Quotize(file).c_str()).c_str());
		if (!wxFileName::IsFileExecutable(file)) {
			errors_manager->AddZinjaiError(true,LANG(MAINW_WARNING_NO_EXCUTABLE_PERMISSION,"El binario no tiene permisos de ejecución."));
			return false;
		} else {
			errors_manager->AddZinjaiError(false,LANG(MAINW_WARNING_EXCUTABLE_PERMISSION_CHANGED,"Se cambiaron los permisos del binario (chmod a+x)."));
		}
	}
#endif
	return true;
}


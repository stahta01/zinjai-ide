#ifndef MXCOMPILER_H
#define MXCOMPILER_H
#include <wx/treectrl.h>
#include <wx/string.h>
#include "Cpp11.h"
#include "CompilerErrorsManager.h"

class wxProcess;
class mxSource;
class wxTimer;
class GenericAction;

enum MXC_OUTPUT {
	MXC_GCC, ///< output should be parsed and added to compiler tree (regular output)
	MXC_SIMIL_GCC, ///< output should be added to compiler tree, but not parsed (for gcc-like compilers)
	MXC_EXTRA, ///< output should not be parsed, it's a project's extra compiling step
	MXC_EXTERN, ///< output should be redirected to main_window->extern_compiler_output (it's from an extern toolchain)
	MXC_NULL ///< undefined, should not be used
};

enum CAR_LAST_LINE { // information about last error line that may condicion current one
	CAR_LL_NULL, //
	CAR_LL_IN_INCLUDED_FILE
};

enum CAR_ERROR_LINE { // information about last error line that may condicion current one
	CAR_EL_UNKNOWN,
	CAR_EL_ERROR,
	CAR_EL_WARNING,
	CAR_EL_CHILD_LAST,
	CAR_EL_CHILD_NEXT,
	CAR_EL_CHILD_SWAP,
	CAR_EL_IGNORE
};

//! Información acerca de una compilación en proceso (puede ser realmente una compilación, o un paso adicional, o hasta una ejecución... es decir, cualquier proceso relacionado a la construcción y/o ejecución)
struct compile_and_run_struct_single {
	GenericAction *on_end; ///< what to do after this process if it runs ok
	bool parsing_errors_was_ok; ///< indica si hubo problemas al analizar/reacomodar la salida del compilador
	bool killed; ///< indica si fue interrumpido adrede, para usar en OnProcessTerminate
	bool compiling; ///< indica si esta compilando/enlazando o ejecutando
	wxString step_label; ///< nombre del paso especial que se esta ejecutando (para los extra step, usar con ouput_type=MXC_EXTRA)
	MXC_OUTPUT output_type; ///< indica el tipo de salida
	bool linking; ///< indica si esta compilando o enlazando (cuando compiling=true, parece que solo se usa para llamar a mxCompiler::CheckForExecutablePermision)
	wxProcess *process; ///< puntero al proceso en ejecucion
	long int pid; ///< process id del proceso en ejecucion, 0 si no se pudo lanzar
	CAR_LAST_LINE error_line_flag; ///< flag for mxCompiler::ParseSomeErrorsOneLine
	CompilerErrorsManager::CEMState m_cem_state;
	wxString valgrind_cmd; ///< prefijo para la ejecucion con la llamada a valgrind
	compile_and_run_struct_single(const char *name);
	compile_and_run_struct_single(const compile_and_run_struct_single *o); ///< para cuando compila en paralelo en un proceso, crea el segundo hilo a partir de un primero duplicando algunas propiedades
	~compile_and_run_struct_single();
	compile_and_run_struct_single *next, *prev;
#ifdef _ZINJAI_DEBUG
	const char* mname;
	static int count;
#endif
	wxString GetInfo();
};

//! Agrupa las funciones relacionadas al lanzamiento y gestion de los procesos de compilacion
class mxCompiler {
public:
	mxCompiler();
	bool IsCompiling();
	int NumCompilers();
	wxString GetCompilingStatusText();
	
	void CompileSource (mxSource *source, GenericAction *on_end=nullptr);
	void BuildOrRunProject(bool prepared, GenericAction *on_end=nullptr);
	void ParseSomeExternErrors(compile_and_run_struct_single *compile_and_run);
	CAR_ERROR_LINE ParseSomeErrorsOneLine(compile_and_run_struct_single *compile_and_run, const wxString &error_line);
	void SetWarningsAndErrorsNumbersOnTree();
	void ParseSomeErrors(compile_and_run_struct_single *compile_and_run);
	void ParseCompilerOutput(compile_and_run_struct_single *compile_and_run, bool success);
	compile_and_run_struct_single *compile_and_run_single; // lista simplemente doblemente enlazada, sin nodo ficticio ???
	
	// compile_and_run_struct_common
	mxSource *last_compiled; ///< ultimo fuente compilado
	mxSource *last_runned; ///< ultimo fuente ejecutado
	wxString last_caption; ///< titulo de la pestaña del ultimo fuente ejecutado
	wxTimer *timer; ///< timer que actualiza el arbol de compilacion mientras compila
	wxString valgrind_cmd; ///< indica si la proxima ejecución se hace con valgrind
	bool CheckForExecutablePermision(wxString file); ///< en linux, verifica que tenga permisos de ejecucion
};

extern mxCompiler *compiler;

#endif


#include "EnvVars.h"
#include "ProjectManager.h"

#warning buscar otros setenv y ver si los puedo poner aca, y dejar el zinjai_dir fijo para el graphviz de algoritmos

#define UNDEF_VAR "<{[UNDEF]}>" // special value for storing in modified_vars for previously unsetted vars
#include "mxUtils.h"


static HashStringString modified_vars; // env vars to be resetted after compiling/running/debugging with their respectives original values

static void store_and_set(const wxString &name, wxString value, bool add=false) {
	wxString old_value; if (!wxGetEnv(name,&old_value)) old_value=UNDEF_VAR;
	if (modified_vars.find(old_value)==modified_vars.end()) modified_vars[name]=old_value;
	if (add) value = old_value+value; else value.Replace(wxString("${")+name+"}",old_value);
	wxSetEnv(name,value);
}

void EnvVars::Reset() {
	for( HashStringString::iterator it=modified_vars.begin(); it!=modified_vars.end(); ++it) {
		if (it->second==UNDEF_VAR) wxUnsetEnv(it->first);
		else wxSetEnv(it->first,it->second);
	}
	modified_vars.clear();
}


void EnvVars::SetMode (EnvVars::mode_t mode) {
	
	Reset(); if (mode==NONE) return;
	
	if (project) {
		project_configuration *pconf = project->active_configuration;
		
		// for execution scripts
		store_and_set("Z_PROJECT_PATH",pconf->working_folder);
		store_and_set("Z_PROJECT_BIN",project->executable_name);
		store_and_set("Z_TEMP_DIR",project->GetTempFolder());
		store_and_set("Z_ARGS",pconf->args);
		
		if (mode==RUNNING||mode==DEBUGGING) {
			
			// update PATH/LD_LIBRARY_PATH to find the project generated dynamic libs
			wxString ldlp_name = _if_win32("PATH","LD_LIBRARY_PATH"), 
					 ldlp_sep = _if_win32(";",":"), ldlp_value;
			wxGetEnv(ldlp_name,&ldlp_value);
			bool has_libs = false;
			for(JavaVectorIterator<project_library> lib(pconf->libs_to_build); lib.IsValid(); lib.Next()) {
				if (!lib->is_static) {
					has_libs = true;
					if (ldlp_value.Len()) ldlp_value << ldlp_sep;
					ldlp_value << mxUT::Quotize(DIR_PLUS_FILE(project->path,lib->path));
				}
			}
			if (has_libs) store_and_set(ldlp_name,ldlp_value);
			
			// add user defined environmental variables
			if (pconf->env_vars.Len()) {
				wxArrayString array;
				mxUT::Split(pconf->env_vars,array,false,false);
				for(unsigned int i=0;i<array.GetCount();i++) {  
					if (!array[i].Contains("=")) continue;
					wxString name = array[i].BeforeFirst('='), value = array[i].AfterFirst('='); 
					mxUT::ParameterReplace(value,"${MINGW_DIR}",current_toolchain.mingw_dir,false);
					mxUT::ParameterReplace(value,"${TEMP_DIR}",project->GetTempFolder(),false);
					mxUT::ParameterReplace(value,"${PROJECT_PATH}",project->path,false);
					bool add = name.Last()=='+'; 
					if (add) name.RemoveLast();
					store_and_set(name,value,add);
				}
			}
			
		}
	}
	
	if (mode==DEBUGGING||mode==RUNNING) {
		store_and_set("RUNNING_FROM_ZINJAI","1");
		store_and_set("DEBUGGING_FROM_ZINJAI",mode==DEBUGGING?"1":"0");
	}
	
}


static void restore_env_var(const char *varname) {
	wxString value, zvname=wxString("ZINJAI_EV_")+varname;
	if (!wxGetEnv(zvname,&value)) return; else wxUnsetEnv(zvname);
	if (value=="ZINJAI_UNSET") wxUnsetEnv(varname);
	else wxSetEnv(varname,value.c_str());
}

void EnvVars::Init (const wxString &zinjai_dir) {
#ifdef __linux__
	// env vars changed by ZinjaI's own wrapper (../src_extras/launcher.cpp) to fix menus and wx-encoding problems)
	restore_env_var("UBUNTU_MENUPROXY");
	restore_env_var("LIBOVERLAY_SCROLLBAR");
#endif
	wxSetEnv("ZINJAI_DIR",zinjai_dir);
}


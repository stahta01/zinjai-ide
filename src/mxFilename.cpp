#include "mxFilename.h"
#include <wx/filename.h>

static char path_sep = wxFileName::GetPathSeparator();

/** 
* Concatena una ruta y un nombre de archivo. Si este es relativo, agregando la 
* barra si es necesario. Si el "archivo" era una ruta absoluta, la devuelve sin cambios.
* Esta función se creó para encapsular las diferencias entre Windows y GNU/Linux,
* particularmente, el caracter de separación, y la presencia/ausencia de la unidad.
* La macro DIR_PLUS_FILE encapsula esta llamada utilizando el objeto utils.
* @param dir El directorio base
* @param fil El archivo. Puede ser solo un nombre, la parte final de una ruta, o una ruta completa
* @return Un wxString con la ruta resultante completa
**/
wxString mxFilename::JoinDirAndFile(wxString dir, wxString fil) {
	if (dir.Len()==0 || (fil.Len()>1 && (fil[0]=='\\' || fil[0]=='/' || fil[1]==':')))
		return fil;
	else if (dir.Last()==path_sep)
		return dir+fil;
	else
		return dir+path_sep+fil;
}

/** 
* Convierte un path absoluto en relativo, siempre y cuando no deba subir más
* de 2 niveles desde el path de referencia
**/
wxString mxFilename::Relativize(wxString name, wxString path) {
	if (path.Last()=='\\' || path.Last()=='/') path.RemoveLast();
	bool rr=false;
	if (rr) name.RemoveLast();
	wxFileName fname(name);
	if (fname==path) return ".";
	fname.MakeRelativeTo(path);
	wxString rname = fname.GetFullPath();
	if (rname.StartsWith(_T("../../..")) || rname.StartsWith(_T("..\\..\\..")))
		return name;
	else 
		return rname;
}

#ifdef __WIN32__
#	define _is_path_char(str,p) (str[p]=='\\'||str[p]=='/'||p==1&&str[p]==':')
#else
#	define _is_path_char(str,p) (str[p]=='/')
#endif

wxString mxFilename::Normalize (wxString path) {
	int len = path.Len();
	int p = 0, p0 = 0, delta=0;
	do {
		char c = path[p];
		if (p==len || _is_path_char(path,p)) {
			if (p0==p-2 && path[p-1]=='.' && path[p-2]=='.') {
				int pa = p0-delta-2;
				while(pa>0 && !_is_path_char(path,pa)) --pa;
				if (pa>0) {
					delta=p-pa;
				}
			} else if (p0==p-1 && path[p-1]=='.') {
				delta+=2;
			}
			p0=p+1;
		}
		if (delta) path[p-delta]=c;
		++p;
	} while(p<=len);
	if (delta) path.erase(len-delta);
	return path;
}

wxString mxFilename::GetFileName (const wxString &fullpath, bool with_extension) {
	int i = fullpath.Len()-1, pdot=-1;
	while (i>=0 && !_is_path_char(fullpath,i)) {
		if (!with_extension && pdot==-1 && fullpath[i]=='.') pdot=i;
		--i;
	}
	return fullpath.Mid(i+1,pdot==-1?wxSTRING_MAXLEN:(pdot-i-1));
}

wxString mxFilename::GetPath (const wxString &fullpath) {
	int i = fullpath.Len()-1;
	while (i>=0 && !_is_path_char(fullpath,i)) --i;
	return fullpath.Mid(0,i+1);
	
}


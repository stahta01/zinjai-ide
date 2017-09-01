#ifndef MXFILENAME_H
#define MXFILENAME_H

#include <wx/string.h>

#define DIR_PLUS_FILE mxFilename::JoinDirAndFile
#define DIR_PLUS_FILE_2(a,b,c) mxFilename::JoinDirAndFile(mxFilename::JoinDirAndFile(a,b),c)

class mxFilename {
public:
	/** @brief Concatena una ruta y un nombre de archivo **/
	static wxString JoinDirAndFile(wxString dir, wxString fil);
	
	/** @brief Convierte un path absoluto en relativo **/
	static wxString Relativize(wxString fname, wxString path);
	
	/** @brief Contrae los ".." de los paths **/
	static wxString Normalize(wxString path);
	/** @brief Extrae solo el nombre del archivo, sin la ruta **/
	static wxString GetFileName(const wxString &fullpath, bool with_extension=true);
	/** @brief Extrae solo la ruta, sin el nombre del archivo **/
	static wxString GetPath(const wxString &fullpath, bool or_dot=false);
	/** @brief Si el path termina en '/' o '\', pero no es el raiz, lo remueve**/
	static wxString RemoveTrailingSlash(const wxString &fullpath);
};

#endif


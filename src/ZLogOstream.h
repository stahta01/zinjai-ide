#ifndef ZLOGOSTREAM_H
#define ZLOGOSTREAM_H
#include "ZLog.h"
#include "Cpp11.h"
#include <iostream>

class ZLogOstream : public ZLog {
protected:
	std::ostream *m_ost;
public:
	ZLogOstream(const char *name) : ZLog(name), m_ost(nullptr) { }
	void DoLog(Level lvl, const char *grp, const wxString &str) {
		wxString msg = ZLog::GetString(lvl,grp,str);
		if (!msg.IsEmpty()) (*m_ost) << msg << endl;
	}
};

class ZLogFile : public ZLogOstream {
	std::ofstream m_file;
public:
	ZLogFile(wxString &fname) : ZLogOstream("ZLogFile") { 
		m_ost = &m_file;
		m_file.open(fname.c_str());
		if (!m_file.is_open()) 
			ZLWAR2("ZLogFile","No se pudo abrir: "<<fname);
	}
};

class ZLogCerr : public ZLogOstream {
public:
	ZLogCerr() : ZLogOstream("ZLogCerr") { 
		m_ost = &std::cerr;
	}
};


#endif


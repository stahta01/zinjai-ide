#ifndef ZLOG_H
#define ZLOG_H
#include <wx/string.h>
#include <fstream>
#include <string>

#define	ZLERR(g,x) ZLog::Print(ZLog::Error,  g,x)
#define	ZLWAR(g,x) ZLog::Print(ZLog::Warning,g,x)
#define	ZLINF(g,x) ZLog::Print(ZLog::Info,   g,x)
#define	ZLDBG(g,x) ZLog::Print(ZLog::Debug,  g,x)
#define	ZLERR2(g,x) ZLog::Print(ZLog::Error,  g,wxString()<<x)
#define	ZLWAR2(g,x) ZLog::Print(ZLog::Warning,g,wxString()<<x)
#define	ZLINF2(g,x) ZLog::Print(ZLog::Info,   g,wxString()<<x)
#define	ZLDBG2(g,x) ZLog::Print(ZLog::Debug,  g,wxString()<<x)

class ZLog {
public:
	ZLog(const char *name);
	~ZLog();
	enum Level { Error, Warning, Info, Debug };
	static void Print(Level l, const char *group, const wxString &x) {
		if (s_first) s_first->LogAndForward(l,group,wxString()<<x);
	}
	static void Print(Level l, const char *group, const char *x) {
		if (s_first) s_first->LogAndForward(l,group,wxString()<<x);
	}
	
protected:
	wxString GetString(Level lvl, const char *grp, const wxString &str) {
		switch(lvl) {
		case ZLog::Error:   return wxString() << '+' << grp << ": " << str;
		case ZLog::Warning: return wxString() << '!' << grp << ": " << str;
		case ZLog::Info:    return wxString() << '=' << grp << ": " << str;
#ifdef _ZINJAI_DEBUG
		case ZLog::Debug:   return wxString() << '>' << grp << ": " << str;
#else
		case ZLog::Debug:   return "";
#endif
		default:   return wxString() << "(INVALID LOG LEVEL)" << grp << ": " << str;
		}
	}
private:
	static ZLog *s_first;
	ZLog *m_prev,*m_next;
	void LogAndForward(Level lvl, const char *grp, const wxString &str) {
		DoLog(lvl,grp,str); if (m_next) m_next->LogAndForward(lvl,grp,str);
	}
	virtual void DoLog(Level l, const char *group, const wxString &str) = 0;
	const char* m_name;
};

#endif


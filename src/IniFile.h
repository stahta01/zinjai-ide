#ifndef INIFILE_H
#define INIFILE_H
#include <wx/textfile.h>
#include "mxUtils.h"
#include "asserts.h"

class IniFileReader {
public:
	IniFileReader(wxString path) : m_wxtf(path), m_is_ok(m_wxtf.Exists()&&m_wxtf.Open()) {
		if (m_is_ok) {
			m_nextline = m_wxtf.GetFirstLine();
			if (NextLineIsEmptyOrComment()) ReadNextLine();
		}
	}
	class Pair {
	public:
		const wxString &Key() const { return key; }
//		int KeyLastNum() const { 
//			int p = key.Len(); long l;
//			while ( (--p)>=0 && key[p]>='0' && key<='9');
//			EXPECT_OR(key.Mid(p+1).ToLong(&l),return -1);
//			return l;
//		}
		const wxString &AsString() const { return value; }
		wxString AsMultiLineText() const { return mxUT::Line2Text(value); }
		int AsInt() const { 
			long l;
			EXPECT_OR(value.ToLong(&l),return 0);
			return l; 
		}
		double AsDouble() const {
			double d;
			EXPECT_OR(value.ToDouble(&d),return 0);
			return d; 
		}
		char AsChar() const { 
			EXPECT_OR(value.Len()==1,return '\0');
			return value[0]; 
		}
		bool AsBool() const { 
			return mxUT::IsTrue(value); 
		}
		bool IsOk() const {
			return !key.IsEmpty();
		}
	private:
		friend class IniFileReader;
		wxString key, value;
		bool SetInvalidState() { 
			key.Clear(); value.Clear(); return false; 
		}
		bool SetFromLine(const wxString &line) {
			int p = line.Find('=');
			EXPECT_OR(p!=0&&p!=wxNOT_FOUND,return SetInvalidState());
			key = line.Mid(0,p); value = line.Mid(p+1); return true;
		}
	};
	wxString GetNextSection() {
		while (m_is_ok && !NextLineIsSection()) ReadNextLine();
		if (!m_is_ok) return "";
		wxString name = m_nextline.Mid(1,m_nextline.Len()-2);
		ReadNextLine();
		return name;
	}
	Pair GetNextPair() {
		if (!m_is_ok || NextLineIsSection()) m_aux_pair.SetInvalidState();
		else { m_aux_pair.SetFromLine(m_nextline); ReadNextLine(); }
		return m_aux_pair;
	}
	bool IsOk() const { 
		return m_is_ok;
	}
private:
	wxTextFile m_wxtf;
	bool m_is_ok;
	wxString m_nextline;
	Pair m_aux_pair;
	bool NextLineIsEmptyOrComment() const {
		return m_nextline.IsEmpty() || m_nextline[0]=='#';
	}
	bool NextLineIsSection() const {
		return m_nextline.Len()>2 && m_nextline[0]=='[' && m_nextline[m_nextline.Len()-1]==']';
	}
	bool ReadNextLine() {
		do {
			if (!m_is_ok || m_wxtf.Eof()) return (m_is_ok=false);
			m_nextline = m_wxtf.GetNextLine();
		} while (NextLineIsEmptyOrComment());
		return true;
	}
};

#endif


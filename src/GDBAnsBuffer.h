#ifndef GDBANSBUFFER_H
#define GDBANSBUFFER_H
#include <wx/string.h>
#include <cstdlib>

class GDBAnsBuffer {
public:
	enum LineType { 
		NONE,
		EMPTY, ///< ""
		PROMPT, ///< "(gdb) "
		RESULT, ///< "^..."
		EXEC_ASYNC, ///< "*..."
		STATUS_ASYNC, ///< "+..."
		NOTIFY_ASYNC, ///< "=..."
		CONSOLE_STREAM, ///< "~..." "...", is a c-string (quoted and scaped)
		TARGET_STREAM, ///< "@...", is a c-string (quoted and scaped)
		LOG_STREAM, ///< "&...", is a c-string (quoted and scaped)
		OTHER ///< "..."
	};
	GDBAnsBuffer() 
		: m_buf((wxChar*)std::malloc(1024*sizeof(wxChar))), m_capacity(1024), 
		m_len(0), m_done(0), m_unprocessed_lines(0) 
	{ 
		
	}
	~GDBAnsBuffer() { 
		std::free(m_buf); 
	}
	void Read(const wxChar *data, int len) {
		if (!len) return;
		EnsureSpaceForAdding(len);
		for(int i=0;i<len;i++) { 
			if (data[i]=='\r') continue;
			if (data[i]=='\n') ++m_unprocessed_lines;
			m_buf[m_len++] = data[i];
		}
	}
	bool HasData() const { return m_unprocessed_lines!=0; }
	LineType GetNextLine(wxString &line) { 
		if (m_unprocessed_lines==0) { return NONE; }
		// find next '\n'... the line will go from m_buf to that '\n'
		int endl_pos = m_done; 
		while (m_buf[endl_pos]!='\n') ++endl_pos;
		// make the c-string end at that pos so we can copy that to a wxstring
		m_buf[endl_pos] = '\0';
		line = m_buf+m_done;
		// mark that portion of the buffer as already processed
		m_done = endl_pos+1; --m_unprocessed_lines;
		// guess type
		if (line.IsEmpty()) return EMPTY;
		switch (line[0]) {
			case '^': return RESULT;
			case '*': return EXEC_ASYNC;
			case '+': return STATUS_ASYNC;
			case '=': return NOTIFY_ASYNC;
			case '~': return CONSOLE_STREAM;
			case '@': return TARGET_STREAM;
			case '&': return LOG_STREAM;
			default: 
				return line=="(gdb) " ? PROMPT : OTHER;
		}
	}
	void Reset() {
		m_unprocessed_lines = 0; m_done = m_len = 0;
	}
	int GetLen() const { return m_len-m_done; }
private:
	wxChar *m_buf;
	int m_capacity; ///< total buffer size
	int m_len; ///< used part of the buffer [0;m_len)
	int m_done; ///< already processed part of the used part of the buffer [m_len;m_done), can be discarded
	int m_unprocessed_lines; ///< cuantas lineas de texto hay listas para procesar en el buffer
	void EnsureSpaceForAdding(int new_len) {
		// "done" is what is aleready processed, from 0 to m_done
		// "data" is what remains to be processed, from "m_done" to m_len
		// "next" is what is going to be appended, from m_len to m_len+len
		if (m_len+new_len+1<m_capacity) return; // ok, enough space for done, current and next
		int data_len = m_len-m_done;
		if (data_len+new_len+1<m_capacity) { // enough space for current and next, just reset m_done to 0
			for(int id=0, is=m_done; is<m_len; ++is,++id) m_buf[id] = m_buf[is];
			m_len = data_len; m_done = 0;
		} else { // not enough space anyway, a new larger buffer is required
			// exponentially grow capacity
			int new_capacity = m_capacity*2; 
			while (new_capacity<data_len+new_len+1) new_capacity*=2;
			// create the new buffer, copy the usefull data, and free the old one
			wxChar *new_buf = (wxChar*)std::malloc(new_capacity*sizeof(wxChar));
			for(int id=0, is=m_done; is<m_len; ++is,++id) new_buf[id] = m_buf[is];
			std::free(m_buf); m_buf = new_buf; m_capacity = new_capacity; 
			m_len = data_len; m_done = 0;
		}
	}
	
};

#endif

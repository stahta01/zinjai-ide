#include "ZLog.h"
#include "Cpp11.h"

ZLog *ZLog::s_first = nullptr;

ZLog::ZLog (const char *name) : m_prev(nullptr), m_next(s_first), m_name(name) {
	ZLINF2("ZLog","Initializing logger "<<m_name);
	if (s_first) s_first->m_prev = this;
	s_first = this;
}

ZLog::~ZLog ( ) {
	if (m_next) m_next->m_prev = m_prev;
	if (m_prev) m_prev->m_next = m_next;
	else s_first = m_next;
	ZLINF2("ZLog","Deleting logger "<<m_name);
}





#include "mxMiniMapPanel.h"
#include "mxMiniSource.h"
#include "mxSource.h"

mxMiniMapPanel::mxMiniMapPanel(wxWindow *parent) 
	: wxPanel(parent), m_current_map(nullptr)
{
	m_sizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(m_sizer);
}
void mxMiniMapPanel::SetCurrentSource (mxSource * src)  {
	if (m_current_map) {
		m_sizer->Remove(m_current_map);
		m_current_map->Hide();
	}
	if (src) {
		m_current_src = src;
		m_current_map = src->GetMinimap(this);
		m_sizer->Add(m_current_map,wxSizerFlags().Proportion(1).Expand());
		m_current_map->Show();
		Layout();
	} else {
		m_current_map = nullptr;
		m_current_src = nullptr;
	}
}

void mxMiniMapPanel::UnregisterSource (mxSource * src) {
	if (m_current_src == src) SetCurrentSource(nullptr);
}


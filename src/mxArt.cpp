#include "mxArt.h"
#include "mxUtils.h"

#include <wx/dcmemory.h>
#include <wx/settings.h>
#include <wx/button.h>
#include <wx/msgdlg.h>
#include "ConfigManager.h"
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <iostream>
#include <wx/wfstream.h>
#include <wx/image.h>
using namespace std;
	
mxArt *bitmaps = nullptr;

mxArt::mxArt(wxString img_dir) {
	
	files.source = &(GetBitmap("trees/ap_cpp.png"));
	files.header = &(GetBitmap("trees/ap_header.png"));
	files.other = &(GetBitmap("trees/ap_other.png"));
	files.blank = &(GetBitmap("trees/ap_blank.png"));
	
	parser.icon02_define =&(GetBitmap("trees/as_define.png"));
	parser.icon03_func = &(GetBitmap("trees/as_func.png"));
	parser.icon04_class = &(GetBitmap("trees/as_class.png"));
	parser.icon05_att_unk = &(GetBitmap("trees/as_att_unk.png"));
	parser.icon06_att_pri = &(GetBitmap("trees/as_att_pri.png"));
	parser.icon07_att_pro = &(GetBitmap("trees/as_att_pro.png"));
	parser.icon08_att_pub = &(GetBitmap("trees/as_att_pub.png"));
	parser.icon09_mem_unk = &(GetBitmap("trees/as_met_unk.png"));
	parser.icon10_mem_pri = &(GetBitmap("trees/as_met_pri.png"));
	parser.icon11_mem_pro = &(GetBitmap("trees/as_met_pro.png"));
	parser.icon12_mem_pub = &(GetBitmap("trees/as_met_pub.png"));
	parser.icon13_none = &(GetBitmap("trees/as_none.png"));
	parser.icon14_global_var = &(GetBitmap("trees/as_global.png"));
	parser.icon15_res_word = &(GetBitmap("trees/as_res_word.png"));
	parser.icon16_preproc = &(GetBitmap("trees/as_preproc.png"));
	parser.icon17_doxygen = &(GetBitmap("trees/as_doxygen.png"));
	parser.icon18_typedef = &(GetBitmap("trees/as_typedef.png"));
	parser.icon19_enum_const = &(GetBitmap("trees/as_enum_const.png"));
	parser.icon20_argument = &(GetBitmap("trees/as_arg.png"));
	parser.icon21_local = &(GetBitmap("trees/as_local.png"));

	icons.error = &(GetBitmap("dialogs/icon_error.png"));
	icons.info = &(GetBitmap("dialogs/icon_info.png"));
	icons.warning = &(GetBitmap("dialogs/icon_warning.png"));
	icons.question = &(GetBitmap("dialogs/icon_question.png"));
	
	buttons.ok = &(GetBitmap("dialogs/button_ok.png"));
	buttons.cancel = &(GetBitmap("dialogs/button_cancel.png"));
	buttons.help = &(GetBitmap("dialogs/button_help.png"));
	buttons.find = &(GetBitmap("dialogs/button_find.png"));
	buttons.replace = &(GetBitmap("dialogs/button_replace.png"));
	buttons.next = &(GetBitmap("dialogs/button_next.png"));
	buttons.prev = &(GetBitmap("dialogs/button_prev.png"));
	buttons.stop = &(GetBitmap("dialogs/button_stop.png"));
	
}


mxArt::~mxArt() {
	
	delete parser.icon02_define;
	delete parser.icon03_func;
	delete parser.icon04_class;
	delete parser.icon05_att_unk;
	delete parser.icon06_att_pri;
	delete parser.icon07_att_pro;
	delete parser.icon08_att_pub;
	delete parser.icon09_mem_unk;
	delete parser.icon10_mem_pri;
	delete parser.icon11_mem_pro;
	delete parser.icon12_mem_pub;
	delete parser.icon13_none;
	delete parser.icon14_global_var;
	delete parser.icon15_res_word;
	delete parser.icon16_preproc;
	delete parser.icon17_doxygen;
	delete parser.icon18_typedef;
	delete parser.icon19_enum_const;
	delete parser.icon20_argument;
	
	delete files.blank;
	delete files.source;
	delete files.header;
	delete files.other;

	delete buttons.ok;
	delete buttons.cancel;
	delete buttons.help;
	delete buttons.replace;
	delete buttons.find;
	delete buttons.next;
	delete buttons.prev;
	delete buttons.stop;
	
	delete icons.info;
	delete icons.error;
	delete icons.warning;
	delete icons.question;
}

wxString mxArt::GetSkinImagePath(const wxString &fname, bool replace_if_missing) {
	static wxString default_path=DIR_PLUS_FILE(config->zinjai_dir,"imgs");
	static wxString skin_path=DIR_PLUS_FILE(config->zinjai_dir,config->Files.skin_dir);
	static bool is_default = default_path==skin_path;
	if (is_default) {
#ifdef _ZINJAI_DEBUG
		if (!wxFileName::FileExists(DIR_PLUS_FILE(default_path,fname)))
			cerr<<"MISSING IMAGE: "<<DIR_PLUS_FILE(default_path,fname)<<endl;
#endif
		return DIR_PLUS_FILE(default_path,fname);
	} else {
		wxString fskin = DIR_PLUS_FILE(skin_path,fname);
		if (!replace_if_missing || wxFileName::FileExists(fskin)) {
#ifdef _ZINJAI_DEBUG
			if (!wxFileName::FileExists(fskin))
				cerr<<"MISSING IMAGE: "<<fskin<<endl;
#endif
			return fskin;
		} else {
#ifdef _ZINJAI_DEBUG
			if (!wxFileName::FileExists(DIR_PLUS_FILE(default_path,fname)))
				cerr<<"MISSING IMAGE: "<<DIR_PLUS_FILE(default_path,fname)<<endl;
#endif
			return DIR_PLUS_FILE(default_path,fname);
		}
	}
}

void mxArt::Initialize ( ) {
	bitmaps = new mxArt(config->Files.skin_dir);
}


bool mxArt::HasBitmap (const wxString &fname, bool is_optional) {
	static wxString last_fname;
	static bool last_is_optional;
	if (fname==last_fname && is_optional==last_is_optional) return last_bmp;
	
	static wxString skin_path=DIR_PLUS_FILE(config->zinjai_dir,config->Files.skin_dir);
	static wxString default_path=DIR_PLUS_FILE(config->zinjai_dir,"imgs");
	last_bmp = auxHasBitmap(DIR_PLUS_FILE(skin_path,fname));
	if (!last_bmp && !is_optional)
		last_bmp = auxHasBitmap(DIR_PLUS_FILE(default_path,fname));
	return last_bmp;
	
}	
	
const wxBitmap *mxArt::auxHasBitmap (const wxFileName &fullpath) {
	wxString path = fullpath.GetPath();
	BitmapPack *&pack = packs[path];
	if (!pack) pack=LoadPack(path);
	wxBitmap *&bmp = pack->bitmaps[fullpath.GetFullPath()];
	if (!bmp && !pack->pack_exists) {
		if (fullpath.FileExists())
			bmp = new wxBitmap(fullpath.GetFullPath(),wxBITMAP_TYPE_PNG);
	}
	return bmp;
}

const wxBitmap & mxArt::GetBitmap(const wxString &fname, bool is_optional) {
	HasBitmap(fname,is_optional); 
#ifdef _ZINJAI_DEBUG
	if (!last_bmp)
		cerr<<"MISSING IMAGE: "<<fname<<endl;
#endif
	return *last_bmp;
}

mxArt::BitmapPack *mxArt::LoadPack (const wxString & path) {
	BitmapPack *pack = new BitmapPack();
#ifdef _USE_PACKED_BITMAPS
	wxTextFile fil(DIR_PLUS_FILE(path,"bitmaps.idx"));
	if (fil.Exists()) {
		wxFFileInputStream fpack(DIR_PLUS_FILE(path,"bitmaps.pack"));
		fil.Open();
		pack->pack_exists=true;
		for ( wxString str = fil.GetFirstLine(); !fil.Eof(); str = fil.GetNextLine() ) {
			pack->bitmaps[DIR_PLUS_FILE(path,str)]=new wxBitmap(wxImage(fpack,wxBITMAP_TYPE_PNG));
		}
	}
#endif
	return pack;
}


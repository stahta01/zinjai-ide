AP="ap_c ap_cpp ap_header ap_zpr ap_other ap_blank ap_blacklist ap_wxfb ap_folder"
AS="as_arg as_att_pri as_att_pro as_att_pub as_att_unk as_class as_define as_doxygen as_enum_const as_folder as_func as_global as_local as_met_pri as_met_pro as_met_pub as_met_unk as_none as_preproc as_res_word as_typedef"
CO="co_project_warning co_error co_warning co_info co_out co_err_info co_folder"
for a in $AP $AS $CO; do ./export_one.sh trees.svg ../trees $a 16 16; done
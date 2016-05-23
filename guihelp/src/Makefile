HOEDOWN = hoedown/hoedown --tables --fenced-code --autolink --strikethrough --disable-intra-emphasis --quote
MDS = $(shell ls *.md)
HTMLS = $(subst md,html,$(addprefix ../,$(MDS)))
all: $(HTMLS)
	
	
../%.html: %.md
	@echo $< ' -> ' $@
	@echo '<HTML><HEAD><TITLE>ZinjaI - '$(shell head $< -n1 | sed 's/#//g;s/*//g')'</TITLE></HEAD><BODY>' > $@
	@$(HOEDOWN) $< >> $@
	@echo '</BODY></HTML>' >> $@

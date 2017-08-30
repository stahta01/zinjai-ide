PREFS="new_file new_project open import"
for a in $PREFS; do ./export_one.sh welcome.svg ../welcome $a 24 24; done

./export_one.sh welcome.svg ../welcome tip 44 50

./export_one.sh welcome.svg ../welcome title 85 28
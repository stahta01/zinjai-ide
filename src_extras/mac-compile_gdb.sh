cd ~
export LOGFILE=$(pwd)/gdb_build.log
date > "$LOGFILE"
mkdir -p .zinjai
cd .zinjai
export GDBVER=8.0


echo "CURRENT SHELL: $SHELL" > "$LOGFILE"

function zsdo { 
	echo "LA SIGUIENTE OPERACION PUEDE REQUERIR AUTENTICACION / THE FOLLOWING OPERATION MAY REQUIERE AUTHENTICATION" 
	echo "RUNNING: \"sudo" "$@""\""
        echo ">>> sudo" "$@" >> "$LOGFILE"
	sudo "$@" 2>> "$LOGFILE"
	echo ""; echo "" >> "$LOGFILE"
}

function zqdo { 
        echo ">>> $1" >> "$LOGFILE"
	"$@" 2>> "$LOGFILE"
	echo "" >> "$LOGFILE"
}

function zdo { 
	echo "$2... / $3..." 
        echo ">>> $4" >> "$LOGFILE"
	$4 2>> "$LOGFILE" || { echo "ERROR $2"; exit $1; }
	echo ""; echo "" >> "$LOGFILE"
}

function zdo_nr { 
	echo "$2... / $3..." 
        echo ">>> $4" >> "$LOGFILE"
	$4 || { echo "ERROR $2"; exit $1; }
	echo ""; echo "" >> "$LOGFILE"
}

function zdo_ni { 
        echo ">>> $4" >> "$LOGFILE"
	$4 2>> "$LOGFILE" || { echo "ERROR $2"; exit $1; }
	echo ""; echo "" >> "$LOGFILE"
}

if ! [ "$GDBVER" = "$(cat gdb.version)" ]; then
	zdo_nr 1 "DOWNLOADING GDB" "DESCARGANDO GDB" "curl http://ftp.gnu.org/gnu/gdb/gdb-$GDBVER.tar.gz -o gdb-$GDBVER.tar.gz"
	zdo 2 "UNCOMPRESSIONG SOURCES" "DESCOMPRIMIENDO FUENTES" "tar -xzf gdb-$GDBVER.tar.gz"
	zdo_ni 3 "ENTERING SRC DIRECTORY" "INGRESANDO AL DIRECTORIO DE GDB" "cd gdb-$GDBVER"
	zdo 4 "CONFIGURING GDB" "CONFIGURANDO COMPILACION" "./configure"
	zdo 5 "BUILDING GDB" "COMPILANDO GDB" "make"
	zdo 6 "INSTALLING GDB" "INSTALLANDO GDB" "cp ./gdb/gdb ../gdb.bin"
	zdo_ni 7 "GOING BACK TO ZINJAI'S DIRECTORY" "VOLVIENDO AL DIRECTORIO DE ZINJAI" "cd .."
	echo $GDBVER > gdb.version
	zdo 8 "CLEANING TEMPORARY FILES" "BORRANDO ARCHIVOS TEMPORALES" "rm -rf gdb-$GDBVER.tar.gz gdb-$GDBVER"
else
	echo "GDB ALREADY PRESENT: $(cat gdb.version)" >> "$LOGFILE"
	echo ""    
fi
echo -e \\033[2J\\033[H
echo "BUILDING COMPLETE / COMPILACION FINALIZADA" 
echo " "
echo " "
echo "PRESS RETURN WHEN THE CERTIFICATE IS ON THE DESKTOP"
echo "PRESIONE ENTER CUANDO EL CERTIFICADO ESTE EN EL ESCRITORIO" 
read x
echo ""
zsdo security add-trust -d -r trustRoot -p basic -p codeSign -k /Library/Keychains/System.keychain "$HOME/Desktop/zinjai-gdb.cer"
zqdo rm -f "$HOME/Desktop/zinjai-gdb.cer"

echo " "
echo "PRESS RETURN WHEN THE CERTIFICATE IS IN LOGIN AGAIN"
echo "PRESIONE ENTER CUANDO EL CERTIFICADO ESTE NUEVAMENTE EN INICIO DE SESION" 
read x
echo ""
zdo 10 "APLYING CERTIFICATE TO GDB" "APLICANDO EL CERTIFICADO A GDB" "codesign -s zinjai-gdb -f gdb.bin"
echo ""
echo ""
echo "TRYING TO RESTART TASKGATED SERVICE / INTENTANDO REINICIAR EL SERVICIO TASKGATED "
echo ""
zsdo killall taskgated
echo ""
echo ""
echo -e \\033[2J\\033[H
echo "PROCESS FINISHED. YOU CAN CLOSE THIS WINDOW NOW."
echo "PROCESO FINALIZADO. YA PUEDE CERRAR ESTA VENTANA."
echo ""

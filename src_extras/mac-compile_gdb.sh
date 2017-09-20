cd ~
export LOGFILE=$(pwd)/gdb_build.log
date > "$LOGFILE"
mkdir -p .zinjai
cd .zinjai
export GDBVER=8.0


function zsdo { 
	echo "LA SIGUIENTE OPERACION PUEDE REQUERIR AUTENTICACION / THE FOLLOWING OPERATION COULD REQUIERE AUTHENTICATION" 
	echo "RUNNING: \"sudo $1\""
        echo ">>> sudo $1" >> "$LOGFILE"
	sudo $1 2>> "$LOGFILE"
	echo ""; echo "" >> "$LOGFILE"
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
fi
echo " "
echo "PRESIONE ENTER CUANDO EL CERTIFICADO ESTE EN EL ESCRITORIO" 
echo "PRESS RETURN WHEN THE CERTIFICATE IS ON THE DESKTOP"
echo ""
zsdo "security add-trust -d -r trustRoot -p basic -p codeSign -k /Library/Keychains/System.keychain ~/Desktop/zinjai.cer"
rm -f ~/Desktop/zinjai.cer
read x
echo "PRESIONE ENTER CUANDO EL CERTIFICADO ESTE LISTO" 
echo "PRESS RETURN WHEN THE CERTIFICATE IS READY"
read x
zdo 10 "APLYING CERTIFICATE TO GDB" "APLICANDO EL CERTIFICADO A GDB" "codesign -s zinjai-gdb -f gdb.bin"
echo ""
echo "INTENTANDO REINICIAR EL SISTEMA DE LLAVES / TRYING TO RESTART KEYCHAINS SYSTEM"
zsdo "killall taskgated"
echo ""
echo "YOU CAN CLOSE THIS WINDOW NOW / YA PUEDE CERRAR ESTA VENTANA"

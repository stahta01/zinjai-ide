### Diálogo *Opciones de Compilación y Ejecución de Proyecto*

Este diálogo permite gestionar los perfiles de compilación y ejecución de un proyecto. Se accede al mismo a travéz del comando [Opciones](menu_ejecucion.html#opciones) del [menú Ejecución](menu_ejecucion.html). Si necesita configurar otras opciones no relacionadas a la compilación y ejecución (como tabulado, nombre, indices de autocompletado, archivo de macros de depuración, etc.), utilice el diálogo de [Configuración de Proyecto](project_general_config.html).

En cada proyecto se pueden definir uno o más perfiles (por ejemplo, uno para depuración/debug y otro para produccion/release, o uno por plataforma, etc.). Cada perfil define aspectos relativos a la compilación, el enlazado y la ejecución del proyecto. Es decir, cuando se compila o ejecuta el proyecto, los pasos necesarios para dicha compilación, las opciones que reciben el compilador y el enlazador, y la forma en que este resultado es ejecutado dependen del perfil activo en ese momento.


<A name="administracion"></A>
#### Administración perfiles

Para administrar los diferentes perfiles debe utilizar los botones y la lista desplegable de la parte superior de la ventana: 

![](project_config_header.png)

*  **Añadir un nueva perfil**: para agregar un nuevo perfil debe utilizar el botón *Nuevo*. Cuando presione dicho botón se le pedirá un nombre para el nuevo perfil, a continuación se le permitirá elegir otro perfil preexistente desde el cual copiar la configuración inicial, y finalmente se creará el mismo añadiendose a la lista desplegable.

*  **Utilizar/activar un perfil**: para utilizar un perfil debe seleccionarlo y hacer click sobre el botón *Utilizar*. Nótese que el echo de seleccionar un perfil de la lista desplegable no lo convierte en el perfil activo para las siguientes compilaciones/ejecuciones, sino que solamente muestra su configuración en las pestañas para permitirle modificarlo.

*  **Renombrar un perfil existente**: para cambiarle el nombre a una configuración seleccione la misma en la lista desplegable y presione *Renombrar*.

*  **Modificar un perfil existente**: para modificar el contenido de una configuración existente debe seleccionarla en la lista desplegable, modificar los que desee.

*  **Eliminar un perfil**: para eliminar una configuración seleccione la misma en la lista desplegable y presione *Eliminar*.

*Nota:* Existe una forma más rápida de cambiar el perfil activo sin utilizar este cuadro de diálogo, que consiste en hacer click derecho sobre el botón de la barra de herramientas de la ventana principal que corresponde a este diálogo. Esta acción despliega un menú contextual con la lista de perfiles disponibles.


A continuación se detallan las opciones que se pueden definir dentro de cada perfil:


<A name="general"></A>
#### Pestaña General

Define opciones generales y de ejecución del proyecto.

![](project_config_general.png)

*  **Ubicacion del ejecutable**: nombre o ubicación y nombre del ejecutable. Esta ruta puede ser absoluta, relativa al directorio del proyecto. Puede utilizar además la variable `${TEMP_DIR}` para referirse al directorio definido para temporales en la pestaña *Compilación*.

*  **Mecanismo de ejecución**: permite elegir cómo se ejecutará el proyecto, para realizar tareas de inicialización antes de lanzar el ejecutable, o lanzarlo de formas especiales:

    *  Regular: se lanza el ejecutable definido en el campo *Ubicación del ejecutable* utilizando los argumentos definidos en el campo *Argumentos para la ejecución*.

    *  Con Inicialización: primero se ejecuta un script (con `/bin/sh` en GNU/Linux, o el intérprete de comandos de Windows en Windows), y luego se procede como el caso Regular (se lanza el ejecutable configurado en el proyecto).

    *  Mediante wrapper: Se antepone al comando de ejecución regular (ejecutable + argumentos) el comando que se ingrese en el campo de texto *comando wrapper*. Este comando recibirá como argumentos al ejecutable del proyecto y los argumentos regulares para el mismo, y deberá encargarse de lanzar la ejecución (esto permite utilizar herramientas como `time` (de GNU/Linux) para instrumentar la ejecución).

    *  Solo Script: en este caso ZinjaI nunca ejecuta el ejecutable generado, sino que solo lanza un script, y el script debe ser quien realice la ejecución. El script puede utilizar algunas de las configuraciones definidas en el perfil, ya que las recibe como variables de entorno. En particular la variable `Z_PROJECT_PATH` contendrá la carpeta del proyecto, la variable `Z_PROJECT_BIN` contendrá la ubicación del ejecutable, la variable `Z_ARGS` contendrá los argumentos para la ejecución, y la variable `Z_TEMP_DIR` contedrá la ruta del directorio de temporales. El directorio de trabajo del script será el configurado en el campo *Directorio de trabajo*, y recibirá los argumentos definidos en *Argumentos para la ejecución*.


*  **Script para ejecución/comando wrapper**: indica el script (ruta completa o relativa a la carpeta del proyecto) del script que se utiliza cuando el mencanismo de ejecución seleccionado es *Con Inicialización* o *Solo Script*; o el comando que se utiliza a modo de wrapper para lanzar la ejecución cuando el mecanismo seleccionado es *Mediante wrapper*.

*  **Directorio de trabajo**: directorio en cual se ejecutará el programa (o script), ruta absoluta o relativa al directorio del proyecto.

*  **Siempre pedir argumentos al ejecutar**: si esta opción está activada *ZinjaI* solicitará los argumantos que debe pasarle al ejecutable antes de cada ejecución. El cuadro de diálogo mediante el cual se solicitan los argumentos permite además modificar el directorio de trabajo y guarda un historial de los argumentos utilizados en las diferentes ejecuciones.

*  **Argumentos para la ejecución**: argumentos que debe recibir el ejecutable (o script) del proyecto para su ejecución.

*  **Esperar una tecla luego de la ejecución**: permite definir si *ZinjaI* esperará a que se presione *Enter* luego de ejecutar su programa para evitar que la ventana del mismo se cierre inmediatamente sin poder ver los resultados. La opción *En caso de error* utiliza el valor de retorno del programa y solo espera si este es distinto de `0`. Esta configuración no aplica para los proyectos que no muestren consola (ver ítem *Es un proyecto de Consola* de la pestaña [Enlazado](#enlazado) de este mismo cuadro de diálogo), ni para las ejecuciones en modo depuración.

*  **Variables de entorno**: Permite modificar variables de entorno antes de ejecutar el proyecto. Colocando `NOMBRE=VALOR` se le asigna `VALOR` a la variable `NOMBRE`. Colocando `NOMBRE+=VALOR` el `VALOR` se le agrega al contenido previo de la variable `NOMBRE`. Puede colocar una lista de asignaciones separadas por espacios o comas, y puede utilizar comillas o caracteres de escape para introducir espacios, comillas, barras, etc, en `VALOR`. Al ejecutar un proyecto, el mismo hereda el ambiente (conjunto de variables de entorno de ZinjaI). El ambiente de ZinjaI se corresponde al heredado desde el sistema operativo, con el agregado en la variable `PATH` de la carpeta que contiene los ejecutables del compilador en el caso de Windows, y las carpetas de bibliotecas dinámicas que el proyecto genere en las variables `PATH` o `LD_LIBRARY_PATH` (según el sistema operativo). Las variables aquí definidas se agregan o reemplazan en este ambiente previo.

	
<A name="compilacion"></A>
#### Pestaña Compilacion

Define los parametros que se utilizan para la compilación.

![](project_config_compiling.png)

*  **Parametros extra para la compilacion**: aquí el usuario puede establecer libremente parámetros adicionales que *ZinjaI* utilizará para la compilación. Vea [Ayuda del compilador utilizado](gcc_index.html).

*  **Macros a definir**: permite ingresar una lista de macros de preprocesador (equivalentes a las definidas con #define en el código fuente) que serán utilizadas para la compilación de los fuentes (con el parámetro -D en gcc). El formato de ingreso es una lista separada por comas o espacios con los nombres y opcionalmente pegado el signo igual y el valor para dicha macro.

*  **Directorios adicionales para buscar cabeceras**: permite definir uno o más directorios en los que el compilador debe buscar los archivos de cabecera incluidos en los fuentes. Los directorios de la lista pueden estar separados por coma, punto y coma o espacios y pueden ser absolutos o relativos al directorio del proyecto. Si desea agregar una ruta que contenga espacios puede encerrarla entre comillas dobles. Equivale a la utilización del parámetro "-I".

*  **Estandar**: Permite elegir la versión del lenguaje C o C++ a utilizar (para especificar a gcc/g++ con el argumento -std=...). Elegir un estándar garantiza que el compilador cumple al menos con sus reglas, pero puede incluir extensiones. La casilla **estricto** permite desactivar las extensiones y limitarlo solo al estándar seleccionado (generando un error al querer utilizar alguna extensión, equivale al parámetro `-pedantic`). Algunos estándares recientes podrían no estar disponibles (o tener soporte parcial) si utiliza una versión de *gcc* desactualizada (`c++11` y `gnu++11` no están presentes en versiones de *gcc* menores a `4.7`, por lo que *ZinjaI* los reemplazará por `c++0x` y `gnu++0x`; de igual forma reemplazará `c++14` y `gnu++14` por `c++1y` y `gnu++1y` en versiones de *gcc* menores a `4.9`). Si se elige la opción predeterminada, *ZinjaI* no indicará a *gcc* qué versión utilizar.

*  **Nivel de advertencias**: define la cantidad y el tipo de advertencias/avisos que el compilador arrojará durante la compilación. Los niveles *Ninguna*, *Todas* y *Extra* equivalen a los parámetros de compilación `-w`, `-Wall` y `-Wall -Wextra` en *gcc* respectivamente. El nivel predeterminado no necesita ningún parámetro, ya que es el nivel predeterminado del compilador. Si además activa la opción *como errores*, cada warning será tratado como un error de compilación, por lo que detendrá el proceso (equivalente al parámetro `-Werror`).

*  **Información de depuracion**: determnia si el compilador incluye o no información de depuración en el ejecutable. Esta información es la que le permite al depurador reconocer qué linea de código se estaba ejecutando en determinado momento, la ubicación de las variables para evaluar sus expresiones, etc. Equivale a los parámetros `-g0`, `-g1`, `-g2` y `-g3`. El nivel más comunmente utilizado para la depuración es el 2.

*  **Nivel de optimizacion**: determina las optimizaciones que el compilador realiza durante la compilación. Estas optimizaciones permiten que el programa corra más rápidamente u ocupe menos espacio en disco. Equivale a los parámetros `-O0`, `-O1`, `-O2`, `-O3`, `-Os`, `-Og` y `-Ofast` (si la versión de *gcc* es menor a `4.8` se utiliza `-O0` en lugar de `-Og`). Los niveles 2 y 3 son los más utilizados comunmente para producción (release). Nota: estas optimizaciones alteran a veces el flujo de control interno del programa, o eliminan determinadas instrucciones, por lo que no debería utilizarse cuando se pretende depurar el programa, ya que lo observado en el depurador puede no condecirse exactamente con lo escrito en el código fuente.

*  **Directorio para archivos temporales e intermedios**: ubicación donde *ZinjaI* guardará los archivos intermedios de la compilación (objetos, archivos de extensión .o, uno por cada fuente compilable del proyecto). El último directorio esta ruta puede no existir, en cuyo caso será creado durante la primer compilación. Esta ruta puede ser absoluta o relativa al directorio del proyecto.


<A name="enlazado"></A>
#### Pestaña Enlazado

Define los parametros que se utilizan para el enlazado.

![](project_config_linking.png)

*  **Parametros extra para el enlazado**: aquí el usuario puede establecer libremente parámetros adicionales que *ZinjaI* utilizará para el enlazado. Vea [Ayuda del compilador utilizado](gcc_index.html)</A>.

*  **Directorios adicionales para buscar librerias**: permite definir uno o más directorios en los que el enlazador debe buscar los archivos de objetos de las librerías externas. Los directorios de la lista pueden estar separados por coma, punto y coma o espacios y pueden ser absolutos o relativos al directorio del proyecto. Si desea agregar una ruta que contenga espacios puede encerrarla entre comillas dobles. Equivale a la utilización del parámetro "-L".

*  **Bibliotecas a enlazar**: define una lista de librerias externas a enlazar con los objetos del proyecto para crear el ejecutable. Los nombres de las librerías puede estar separados por coma, punto y coma, o espacios. Si desea incluir un nombre que contenga espacios puede encerrarlo entre comillas dobles ("nombre de archivo"). Equivale a la utilización del parámetro "-l".

*  **Información para depuración**: Determina qué hacer con la información de depuración que contengan los objetos que se enlazan en el ejecutable (tanto los compilados por el proyecto, como los provenientes de biblitoecas externas). Si elige *Mantener en el binario*, la información de depuración contenida en los archivos objeto a enlazar se incluirá sin modificaciones en el ejecutable. Si elige *Eliminar el binario", la información se removerá durante el enlazado, por lo que el ejeuctable final no contendrá información de depuración alguna (se utiliza el argumento `-s` para el enlazado). Si elige *extraer a un archivo separado*, primero se enlazará el ejecutable como en el caso *Mantener en el binario*, pero luego se utilizarán las herramientas `objcopy` y `strip` para copiar la información de depuración del ejecutable a un nuevo archivo (con idéntico nombre, pero extensión `.dbg`) y luego eliminarla del ejecutable  respectivamente. El ejecutable resultante no contendrá información de depuración (similar a la opción *Eliminar del binario*) pero sí un enlace al archivo `.dbg` generado (para que el depurador `gdb` la reconozca y utilice automáticamente).

*  **Es un programa de consola**: esta opción sólo tiene sentido en sistemas *Windows*, y determina si el programa generado utilza o no una consola. Si está desactivado se utiliza el parámetro `-mwindows`; lo cual permite ocultar la terminal (ventana de texto negra) si el programa crea su propia ventana mediante una bilbioteca gráfica. En sistemas *GNU/Linux* esta opción determina si al ejecutar el proyecto, *ZinjaI* muestra o no la ventana de la terminal; sin embargo esto no modifica al ejecutable, ya que la distinción entre ejecutables de consolas y ejecutables de ventanas no aplica en GNU/Linux.

*  **Reenlazar obligatoriamente en la proxima compilacion/ejecucion**: si se selecciona esta opción el *ZinjaI* reenlazará el ejecutable antes de la próxima ejecución aunque no se detecten cambios en sus fuente u objetos. Luego de reenlazar el proyecto, esta opción de desactiva automáticamente. Puede ser útil cuando un cambio en alguno de los parámetros de enlazado lo amerite.

*  **Icono del ejecutable**: ruta completa o relativa a la carpeta de proyecto del archivo que se utilizará como ícono del ejecutable al compilar en *Microsoft Windows*. Este icono se compila mediante un archivo de recursos generado automáticamente en la carpeta de archivos temporales del proyecto.

*  **Archivo manifest.xml**: ruta completa o relativa a la carpeta de proyecto del archivo que se utilizará como manifest para en *Microsoft Windows*. Este archivo se compila mediante un archivo de recursos generado automáticamente en la carpeta de archivos temporales del proyecto.


<A name="secuencia"></A>
#### Pestaña Secuencia

![](project_config_sequence.png)

Permite utilizar un [toolchain alternativo](toolchains.html), o alterar el proceso de construcción del proyecto insertando pasos adicionales. El proceso detallado, junto con las reglas que determinan cuando ejecutar estos pasos, se explica en la sección [Secuencia de contrucción de proyectos](project_building_sequence.html). Estos pasos se utilizan, por ejemplo, para insertar llamadas a parsers de bibioteca específicas, compilación de recursos adicionales, etc. Cuando agregue o modifique un paso personalizado, lo hará mediante el [Diálogo Agregar/Editar Paso de Compilación Personalizado](compile_extra_steps.html). Consulte este enlace para encontrar una descripción más detallada de las propiedades de cada paso.


<A name="biblioteca"></A>
#### Pestaña Biblioteca

![](project_config_libraries.png)

Permite definir bibliotecas que serán generadas a partir de un subconjunto de fuentes del proyectos. Estas bibliotecas serán además enlazadas con el ejecutable final. Para generar una biblioteca, debe agregarla en esta lista y definir sus propiedades, y qué fuentes la componen, mediante el [Diálogo Generar Biblioteca](lib_to_build.html). Para comprender detalladamente la forma de compilación de las bibliotecas y cómo se integran en el proyecto consulte la sección [Secuencia de contrucción de proyectos](project_building_sequence.html).
Por último, si el objetivo de su proyecto es sólamente construir una o varias bibliotecas, pero no un ejecutable, puede tildar la casilla "Generar solo bibliotecas". Esto evita que *ZinjaI* intente enlazar un ejecutable final.

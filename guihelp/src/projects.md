### Sobre el manejo de Proyectos en ZinjaI...

Si bien *ZinjaI* se desarrolló inicialmente con la simplicidad como uno de los requerimientos más importantes, prevee también su utilización para el desarrollo de proyectos de mayor complejidad. Prueba de ello es el hecho de que el mismo *ZinjaI* es utilizado para su propio desarrollo (cada versión se utiliza para desarrollar la siguiente). El manejo de proyectos surge como una forma de dar al usuario mayor potencia y flexibilidad, sin perder la posibilidad de realizar un ejercicio o programa simple rápidamente. Por esto, los programas simples se compilan en una sola linea sin necesidad de configurar absolutamente nada ni de incluirlos en ningun proyecto (solo abrir/escribir y compilar), mientras que la creación de un proyecto requiere algunos pasos adicionales, pero presenta las siguientes ventajas:

<A name="config"/>
#### Configuración de Proyecto

La configuración del proyecto es mucho más extensa y especializada que la configuración de un programa simple. Esta incluye diferentes opciones generales, opciones relacionadas a la integración con herramientas externas, y opciones relacionadas a la compilación, el enlazado y la ejecución del mismo. Dichas opciones se encuentran distribuidas entre diferentes cuadros de diálogo, siendo los principales el de [Opciones Generales del Proyecto](project_general_config.html) y el de [Opciones de Compilación y Ejecución del Proyecto](project_config.html).

Respecto a las opciones de compilación, enlazado y ejecución, en un proyecto se pueden especificar más de una configuración. Se denomina "perfil" a cada configuración. Esto permite definir, como usualmente se hace en muchos otros IDEs, un perfil para utilizar durante el desarrollo (modo *Debug*) y otro diferente para generar el ejecutable final (modo *Release*). Sin embargo, la cantidad de perfiles es arbitraria, por lo que no debe limitarse solo a dos, pudiendo así armar configuraciones según diferentes criterios (Release/Debug, Sistema Operativo, Versión de las bibliotecas externas, etc).


<A name="portability"/>
#### Portabilidad del Proyecto

La portabilidad de un proyecto es una de los aspectos más cuidados en el manejo interno del mismo. Por portabilidad se refiere a la posibilidad de simplemente copiar la carpeta del mismo y abrirlo en otra PC (con igual o diferente sistema operativo, actualmente *Win32* y *GNU/Linux*) y poder editarlo y compilarlo con la menor cantidad de cambios posibles, o en muchos casos sin ningún tipo de modificación. Además de poder generar diferentes configuraciones para diferentes sistemas operativos, *ZinjaI* realiza internamente otras adaptaciones requeridas tales como:

*  la capacidad de detectar y reemplazar automáticamente el caracter de separación en las rutas de archivos
*  la capacidad de tomar indistantemente los caracteres de fin de linea utilizados por uno u otro sistema operativo
*  la inclusión de configuraciones para ambas plataformas en los proyecto plantilla incluidos
*  las consideraciones especiales en la generación de *Makefiles* (ver [Diálogo Generar Makefile](makefile.html))
*  las consideraciones necesarias al construir los comandos de compilación y ejecución
*  la elección de un compilador, depurador y demás herramientas necesarias disponibles gratuitamente para un gran número de arquitecturas (gcc,gdb,etc)
*  la relativización de todos los paths de archivos pertenecientes al proyecto (todas las rutas se almacenan de forma relativa a la carpeta del proyecto siempre que se pueda)
*  la utilización en su desarrollo de un lenguaje C++ estandar y librerías portables de gran aceptación (como *wxWidgets*)
*  etc.


<A name="templates"/>
#### Utilización de Plantillas de Proyecto

*ZinjaI* permite crear un nuevo proyecto a partir de una plantilla. Cualquier subdirectorio que se encuentre en el directorio de plantillas (ver <A href="preferences.html">Diálogo de Preferencias</A>) será tomado como plantilla de proyecto. Crear un proyecto a partir de una plantilla implica copiar el contenido de la carpeta del proyecto plantilla a la carpeta del nuevo proyecto. Este contenido puede incluir archivos fuentes, cabeceras u otros incluidos en el proyecto, todas las configuraciones del mismo, etc. Para crear una nueva plantilla, simplemente cree un nuevo proyecto y copie el directorio correspondiente al directorio de plantillas (puede que deba reiniciar *ZinjaI* luego). Se incluyen alguna plantillas de ejemplo (proyecto que utilice las librerías *wxWidgets*, y proyecto que utilice *OpenGL*).


#### Temas relacionados

*  [Secuencia de compilación de un proyecto](project_building_sequence.html)
*  [Toolchains alternativos](toolchains.html)
*  [Creación de bibliotecas](libs_generation.html)
*  [Herencia de archivos entres proyectos](project_inheritance.html)

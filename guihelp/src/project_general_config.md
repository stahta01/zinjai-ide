### Diálogo *Configuración de Proyecto*

Este diálogo presenta opciones generales de configuración del proyecto, y da acceso rápido a varias configuraciones relacionadas a herramientas externas. Para acceder al mismo se utiliza la opción [Configuración de proyecto](menu_archivo.html#configuracion_proyecto) del menú [Configuración de proyecto](menu_archivo.html).


<A name="general"></A>
#### General

En esta pestaña encontrará configuraciones generales propias de este cuadro de diálogo (es decir, que solo se pueden definir desde aquí):

*  **Nombre del proyecto** Puede introducir aquí cualquier cadena de texto. Este nombre será el que se muestre en la barra de título de *ZinjaI* mientras el proyecto esté abierto.

*  **Tabulado** Aquí se define el tipo de tabulado que se utiliza al indentar el código fuente, y eventualmente la representación de los caracteres TAB. Los valores iniciales se heredan al crear un proyecto de los generales definidos en el [Cuadro de Preferencias](preferences.html#estilo) de *ZinjaI*, pero  luego permanecen asociados al proyecto y se utilizan cuando elmismo está abierto reemplazando a los generales para que el proyecto reciba un tratamiento uniforma aunque se lo edite con diferentes instalaciones y configuraciones de *ZinjaI*.

*  **Extensiones preferidas** Aquí se definen las extensiones por defecto para archivos fuente y cabecera. Estas extensiones se utilizarán al crear archivos o clases desde la opción [Nuevo](menu_archivo.html#nuevo) del [Menú Archivo](menu_archivo.html).

*  **Heredar archivos de** Puede ingresar aquí la ruta (preferentemente relativa) a uno o más archivos .zpr (de otros proyectos *ZinjaI*). Su proyecto actual heredará (incluirá en su propio arbol de proyecto) todos los archivos (fuentes, cabeceras y otros) definidos en dichos proyectos. La lista de archivos heredados no es fija, sino que se actualiza (se releen los .zprs) cada vez que se abre el proyecto. Más sobre la [Herencia de Proyectos](project_inheritance.html)...

*  **Índices de autocompletado adicionales** Aquí se pueden definir nombres de archivos de índices de autocompletado que en caso de no estar activados en las [preferencias generales](preferences.html#asistencias) (y de estar instalados en el sistema) se activarán automáticamente al abrir el proyecto. Utilice esta opción para activar índices de bibliotecas no estándar que utilice en su proyecto.

*  **Archivo de definición de autocódigos** Puede especificar aquí un archivo de [autocódigos](autocode.html) para reemplazar al definido en las [preferencias generales](preferences.html#asistencias) mientras se trabaja con el proyecto.

*  **Archivo de macros para gdb** Puede especificar aquí un archivo de comandos gdb para reemplazar al definido en las [preferencias generales](preferences.html#depuracion) mientras se trabaja con el proyecto. Al iniciar el depurador, *ZinjaI* ejecuta en gdb este archivo antes de inciar la ejecución del programa a depurar. Notar que si bien el nombre del campo sugiere que el archivo debería utilizarse para definir macros gdb, en realidad el archivo puede contener cualquier comando gdb válido que no implique iniciar la ejecución.

*  **Mejora de inspecciones según tipo** Puede definir aquí plantillas adicionales a las definidas en las [preferencias generales](preferences.html#depuracion) para la mejora en la visualización de inspecciones. Puede aprovechar este cuadro para agregar mejoras propias de estructuras de datos específicas de su proyecto, o de bibliotecas no estándar que su proyecto utilice.

<A name="mas"></A>
#### Más

Esta pestaña ofrece accesos a otros cuadros de diálogo configuración del proyecto más específicos. Los primeros relacionados a los perfiles de compilación y sus opciones (accesibles desde el [menú Ejecución](menu_ejecucion.html) y/o desde el menú contextual del [Árbol de Proyecto](panel_project_tree.html)), los siguientes relacionados a herramientas externas (accesibles desde los diferentes submenúes del [menú Herramientas](menu_herramientas.html)).


<A name="info"></A>
#### Info

Esta pestaña no permite variar ninguna configuración, sino que da acceso a algunas herramientas (ya presentes en el [menú Herramientas](menu_herramientas.html) que presentan información resumida/estadística del proyecto y sus archivos.

### Panel *Árbol de Proyecto* ó *Árbol de Archivos*

El panel de *Árbol de Proyecto*, o también llamado *Árbol de Archivos* muestra las archivos asociados al proyecto actual (en caso de haber un proyecto cargado), o todos los archivos abiertos (en caso de trabajar en modo programas simples, sin proyecto).

![](project_tree.png)


#### Categorías
Las categorías que pueden aparecer en el árbol son:

*  **Fuentes** Archivos "compilables". Es decir, archivos que generarán un objeto o binario. Usualmente estarán aquí todos los fuentes de C y C++. Se determina que un archivo es *fuente* por su extensión. La extensión válida para C es: **.c**, y las extensiones válidas para C++ son: **.cpp**, **.c++**, **.cc** y **.cxx**.
*  **Cabeceras** Archivos de cabecera (a incluir en fuentes con la directiva "#include"). Los archivos en esta categoría serán parseados para detectar símbolos (clases, funciones, etc para el autcompletado) y tendrán el coloreado de sintaxis habilitada, pero nunca serán compilados directamente (sino a través de un #include en un fuente). Las extensiones válidas como cabeceras C/C++ son: **.h**, **.hh**, **.hpp**, **.hxx** y **.h++**.
*  **Otros** Aquí estarán todos los demás archivos abiertos/del proyecto (los que no caigan en ninguna de las categorías anteriores). Estos archivos no participarán del árbol de símbolos ni tendrán el coloreado de sintaxis C/C++ habilitado por defecto. Sin embargo, para algunos formatos conocidos (como Makefiles, o scripts de bash .sh), se habilitará un coloreado simple alternativo. Para los demás archivos, el coloreado C/C++ se puede habilitar desde el [menú Ver](menu_ver#colorear_sintaxis.html).
*  **Lista Negra** Esta categoría puede aparecer solo cuando se trabaja en modo proyecto y se utiliza el mecanismo de [Herencia de Proyectos](project_inheritance.html). A esta categoría se mueven los archivos del proyecto padre que se quiere evitar heredar en el proyecto hijo (es decir que constituye una lista de excepciones).

En modo programa simple la categoría de un archivo se define por su extensión y no es posible modificarla. En modo proyecto, se puede mover un archivo de una categoría a otra utilizando el menú contextual.

En modo proyecto, además, los items correspondientes a archivos heredados de otros .zprs que no hayan sido reasignados al proyecto actual como propios se identificarán por estar presentados con un color de texto semi-transparente (en general, si el fondo del árbol es blanco, se verán en gris en lugar de negro).



#### Menú contextual

Cuando se trabaja sobre un proyecto, el menú contextual del árbol (click derecho sobre un archivo, o sobre una categoría) es más completo y permite por ejemplo:

*  Mover los archivos de una categoría a otra
*  Definir cuales ignorar para el autocompletado/árbol de símbolos
*  Definir cuales abrir en modo solo-lectura
*  Definir un conjunto de opciones de compilación diferentes al resto (a las establecidas en el cuadro de [Opciones de Compilación y Ejecución del Proyecto](project_config.html), para más detalle ver [Opciones de Compilación por fuente](by_src_comp_args.html)).
*  Recompilar solo un fuente en particular; o moverlo a la primera posición en el orden de compilación (para que sea el primero a compilar cada vez que se recompila todo el proyecto).
*  Renombrar el archivo.
*  Quitarlo del proyecto (y opcionalmente eliminarlo del disco).
*  Ver sus propiedades, o abrir el explorador de archivos (tanto el panel de *ZinjaI* como el del sistema) en la carpeta del mismo.
*  Modificar la visualización del árbol para que muestre solo los nombres de archivos, o todas las rutas relativas a la carpeta del proyecto.
*  Abrir todos los archivos de una determinada categoría.
*  Acceder a cuadros de diálogo para buscar archivos dentro del proyecto, o para añadir uno o más (resultados de una búsqueda en el disco) archivos al proyecto (para esto hacer click derecho sobre alguna de las categorías).

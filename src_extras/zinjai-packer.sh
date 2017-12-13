# script for compiling and packing zinjai in linux for distribution
# call with "prepare" to init the directory structure, with "update <dir>" to secure-copy
# sources and other required files from the development branch located on <dir>, and with
# "pack" to compile and compress the result into "zinjai-lxx-yyyymmdd.tgz"

if [ "$1" = "prepare" ]; then

  mkdir -p zinjai
  mkdir -p zinjai/samples
  mkdir -p zinjai/toolchains
  mkdir -p zinjai/templates
  mkdir -p zinjai/autocomp
  mkdir -p zinjai/colours
  mkdir -p zinjai/guihelp
#   mkdir -p zinjai/cppreference
  mkdir -p zinjai/third
  mkdir -p zinjai/third/gprof2dot
  mkdir -p zinjai/third/lizard
  mkdir -p zinjai/parser
  mkdir -p zinjai/parser/common
  mkdir -p zinjai/parser/cpp
  mkdir -p zinjai/parser/cpp/cpplib
  mkdir -p zinjai/parser/hyper
  mkdir -p zinjai/parser/misc
  mkdir -p zinjai/lang
  mkdir -p zinjai/lang/tools
  mkdir -p zinjai/lang/tools/mxLangTool
  mkdir -p zinjai/imgs
  mkdir -p zinjai/imgs/icons
  mkdir -p zinjai/imgs/dialogs
  mkdir -p zinjai/imgs/trees
  mkdir -p zinjai/imgs/welcome
  mkdir -p zinjai/imgs/16
  mkdir -p zinjai/imgs/24
  mkdir -p zinjai/imgs/32
  mkdir -p zinjai/imgs/48
  mkdir -p zinjai/imgs/src
  mkdir -p zinjai/skins
  mkdir -p zinjai/src
  mkdir -p zinjai/src_extras
#  mkdir -p zinjai/src_extras/wx_autocomp
  mkdir -p zinjai/src_extras/complement
  mkdir -p zinjai/src_extras/img_viewer
  mkdir -p zinjai/libs

elif [ "$1" = "update" ]; then

  scp $2/zinjai/readme.txt							zinjai/
  scp $2/zinjai/compiling.txt						zinjai/
  scp $2/zinjai/zinjai.dir							zinjai/
  scp $2/zinjai/Makefile							zinjai/

  scp $2/zinjai/toolchains/*						zinjai/toolchains/
  scp $2/zinjai/third/lizard/*						zinjai/third/lizard/
  scp $2/zinjai/third/gprof2dot/*					zinjai/third/gprof2dot/

  scp $2/zinjai/parser/*							zinjai/parser/
  scp $2/zinjai/parser/common/*						zinjai/parser/common/
  scp $2/zinjai/parser/cpp/*						zinjai/parser/cpp/
  scp $2/zinjai/parser/cpp/cpplib/*					zinjai/parser/cpp/cpplib/
  scp $2/zinjai/parser/hyper/*						zinjai/parser/hyper/
  scp $2/zinjai/parser/misc/*						zinjai/parser/misc/

  scp $2/zinjai/src_extras/*						zinjai/src_extras/
  scp $2/zinjai/src_extras/complement/*				zinjai/src_extras/complement
  scp $2/zinjai/src_extras/img_viewer/*				zinjai/src_extras/img_viewer
  scp $2/zinjai/src/*								zinjai/src/

  scp $2/zinjai/imgs/*.png							zinjai/imgs/
  scp $2/zinjai/imgs/*.xpm							zinjai/imgs/
  scp $2/zinjai/imgs/*.ico							zinjai/imgs/
  scp $2/zinjai/imgs/*.txt							zinjai/imgs/
  scp $2/zinjai/imgs/16/*							zinjai/imgs/16/
  scp $2/zinjai/imgs/24/*							zinjai/imgs/24/
  scp $2/zinjai/imgs/32/*							zinjai/imgs/32/
  scp $2/zinjai/imgs/48/*							zinjai/imgs/48/
  scp $2/zinjai/imgs/src/*							zinjai/imgs/src/
  scp $2/zinjai/imgs/welcome/*						zinjai/imgs/welcome/
  scp $2/zinjai/imgs/trees/*						zinjai/imgs/trees/
  scp $2/zinjai/imgs/dialogs/*						zinjai/imgs/dialogs/
  scp -r $2/zinjai/imgs/icons/*						zinjai/imgs/icons/
  scp -r $2/zinjai/skins/*							zinjai/skins/

  scp $2/zinjai/samples/*							zinjai/samples/
#   scp -r $2/zinjai/cppreference/*					zinjai/cppreference/
  scp $2/zinjai/colours/*							zinjai/colours/
  scp $2/zinjai/guihelp/*							zinjai/guihelp/
  scp $2/zinjai/autocomp/*							zinjai/autocomp/
  scp -r $2/zinjai/templates/*						zinjai/templates/

  scp $2/zinjai/lang/*.txt							zinjai/lang/
  scp $2/zinjai/lang/*.pre							zinjai/lang/
  scp $2/zinjai/lang/*.src							zinjai/lang/
  scp $2/zinjai/lang/*.sgn							zinjai/lang/
  scp $2/zinjai/lang/tools/*.cpp  					zinjai/lang/tools/
  scp $2/zinjai/lang/tools/mxLangTool.bin		  	zinjai/lang/tools/mxLangTool/
  scp $2/zinjai/lang/tools/mxLangTool/Makefile.*	zinjai/lang/tools/mxLangTool/
  scp $2/zinjai/lang/tools/mxLangTool/*.cpp			zinjai/lang/tools/mxLangTool/
  scp $2/zinjai/lang/tools/mxLangTool/*.h			zinjai/lang/tools/mxLangTool/
  scp $2/zinjai/lang/tools/mxLangTool/*.zpr			zinjai/lang/tools/mxLangTool/
  scp $2/zinjai/libs/*.txt							zinjai/libs/

elif [ "$1" = "compile" ]; then

  make -C zinjai/src -f Makefile.lnx || exit 1
  make -C zinjai/src_extras -f Makefile.lnx || exit 1
  make -C zinjai/parser -f Makefile.lnx || exit 1

elif [ "$1" = "pack" ]; then

  make -C zinjai/src -f Makefile.lnx || exit 1
  make -C zinjai/src_extras -f Makefile.lnx || exit 1
  make -C zinjai/parser -f Makefile.lnx || exit 1
  
  rm -f zinjai/lang/tools/mxLangTool/release.lnx/*
  rmdir -f zinjai/lang/tools/mxLangTool/release.lnx

  VER=`cat zinjai/src/version.h | head -n 1 | cut -d ' ' -f 3`
  ARCH=`sh zinjai/src_extras/get-arch.sh`
  EXCLUDES="--exclude=zinjai/src --exclude=zinjai/imgs/src --exclude=zinjai/src_extras --exclude=zinjai/guihelp/src --exclude=zinjai/Makefile  --exclude=zinjai/compiling.txt"
  if ! tar -czvf zinjai-${ARCH}-${VER}.tgz zinjai --exclude=zinjai/temp --exclude=zinjai/bin/*.dbg $EXCLUDES; then exit; fi
  echo -n "Done: "
  ls -sh zinjai-${ARCH}-${VER}.tgz

else

  echo Use: 
  echo "    $1 prepare              create directory tree"
  echo "    $1 update \<source\>    update src from \<source\> with scp"
  echo "    $1 compile              build binaries"
  echo "    $1 pack                 rebuild, clean, and make tar file"

fi

wget -O qt5.zip https://github.com/francescmm/ci-utils/releases/download/qt_5.13/qt5.zip;
unzip -qq qt5.zip;
export QTDIR=$PWD/qt5
export LD_LIBRARY_PATH=$QTDIR/lib/:$LD_LIBRARY_PATH
export PATH=$QTDIR/bin:LD_LIBRARY_PATH:$PATH;
export QT_PLUGIN_PATH=$PWD/qt5/plugins;
$QTDIR/bin/qmake CONFIG+=release GitQlient.pro
make -j 4

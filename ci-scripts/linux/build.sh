wget -O qt5.zip https://github.com/francescmm/ci-utils/releases/download/qt_5.13/qt5.zip;
unzip -qq qt5.zip;
ls -ls qt5
ls -ls qt5/bin
export QTDIR=qt5
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export PATH=$QTDIR/bin:/usr/bin;
export QT_PLUGIN_PATH=$PWD/qt5/plugins;
qmake CONFIG+=release GitQlient.pro
make -j 4

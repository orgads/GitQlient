win32:VERSION = 0.13.0.0
else:VERSION = 0.13.0

DEFINES += \
    VER=\\\"$$VERSION\\\" \
    APP_NAME=\\\"$$TARGET\\\"

RESOURCES += \
    $$PWD/resources.qrc

FORMS += \
    $$PWD/AddSubmoduleDlg.ui \
    $$PWD/BranchDlg.ui \
    $$PWD/CreateRepoDlg.ui \
    $$PWD/GitConfigDlg.ui \
    $$PWD/TagDlg.ui \
    $$PWD/WorkInProgressWidget.ui

HEADERS += \
    $$PWD/AGitProcess.h \
    $$PWD/AddSubmoduleDlg.h \
    $$PWD/BranchContextMenu.h \
    $$PWD/BranchTreeWidget.h \
    $$PWD/BranchesViewDelegate.h \
    $$PWD/BranchesWidget.h \
    $$PWD/ClickableFrame.h \
    $$PWD/CommitHistoryColumns.h \
    $$PWD/CommitHistoryContextMenu.h \
    $$PWD/CommitHistoryModel.h \
    $$PWD/CommitHistoryView.h \
    $$PWD/CommitInfo.h \
    $$PWD/CommitInfoWidget.h \
    $$PWD/ConfigWidget.h \
    $$PWD/Controls.h \
    $$PWD/CreateRepoDlg.h \
    $$PWD/FileBlameWidget.h \
    $$PWD/FileContextMenu.h \
    $$PWD/FileDiffHighlighter.h \
    $$PWD/FileDiffView.h \
    $$PWD/FileDiffWidget.h \
    $$PWD/FileHistoryWidget.h \
    $$PWD/FileListDelegate.h \
    $$PWD/FileListWidget.h \
    $$PWD/FullDiffWidget.h \
    $$PWD/GeneralConfigPage.h \
    $$PWD/GitCloneProcess.h \
    $$PWD/GitConfigDlg.h \
    $$PWD/GitQlient.h \
    $$PWD/GitQlientRepo.h \
    $$PWD/GitQlientSettings.h \
    $$PWD/GitQlientStyles.h \
    $$PWD/GitRequestorProcess.h \
    $$PWD/GitSyncProcess.h \
    $$PWD/ProgressDlg.h \
    $$PWD/RepositoryViewDelegate.h \
    $$PWD/RevisionFile.h \
    $$PWD/RevisionsCache.h \
    $$PWD/ShaFilterProxyModel.h \
    $$PWD/StashesContextMenu.h \
    $$PWD/TagDlg.h \
    $$PWD/UnstagedFilesContextMenu.h \
    $$PWD/WorkInProgressWidget.h \
    $$PWD/git.h \
    $$PWD/lanes.h \
    $$PWD/BranchDlg.h

SOURCES += \
    $$PWD/AGitProcess.cpp \
    $$PWD/AddSubmoduleDlg.cpp \
    $$PWD/BranchContextMenu.cpp \
    $$PWD/BranchTreeWidget.cpp \
    $$PWD/BranchesViewDelegate.cpp \
    $$PWD/BranchesWidget.cpp \
    $$PWD/ClickableFrame.cpp \
    $$PWD/CommitHistoryContextMenu.cpp \
    $$PWD/CommitHistoryModel.cpp \
    $$PWD/CommitHistoryView.cpp \
    $$PWD/CommitInfo.cpp \
    $$PWD/CommitInfoWidget.cpp \
    $$PWD/ConfigWidget.cpp \
    $$PWD/Controls.cpp \
    $$PWD/CreateRepoDlg.cpp \
    $$PWD/FileBlameWidget.cpp \
    $$PWD/FileContextMenu.cpp \
    $$PWD/FileDiffHighlighter.cpp \
    $$PWD/FileDiffView.cpp \
    $$PWD/FileDiffWidget.cpp \
    $$PWD/FileHistoryWidget.cpp \
    $$PWD/FileListDelegate.cpp \
    $$PWD/FileListWidget.cpp \
    $$PWD/FullDiffWidget.cpp \
    $$PWD/GeneralConfigPage.cpp \
    $$PWD/GitCloneProcess.cpp \
    $$PWD/GitConfigDlg.cpp \
    $$PWD/GitQlient.cpp \
    $$PWD/GitQlientRepo.cpp \
    $$PWD/GitQlientSettings.cpp \
    $$PWD/GitQlientStyles.cpp \
    $$PWD/GitRequestorProcess.cpp \
    $$PWD/GitSyncProcess.cpp \
    $$PWD/ProgressDlg.cpp \
    $$PWD/RepositoryViewDelegate.cpp \
    $$PWD/RevisionFile.cpp \
    $$PWD/RevisionsCache.cpp \
    $$PWD/ShaFilterProxyModel.cpp \
    $$PWD/StashesContextMenu.cpp \
    $$PWD/TagDlg.cpp \
    $$PWD/UnstagedFilesContextMenu.cpp \
    $$PWD/WorkInProgressWidget.cpp \
    $$PWD/git.cpp \
    $$PWD/lanes.cpp \
    $$PWD/BranchDlg.cpp

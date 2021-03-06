/*
        Description: interface to git programs

        Author: Marco Costalba (C) 2005-2007

        Copyright: See COPYING file that comes with this distribution

*/
#include "git.h"

#include <RevisionsCache.h>
#include <CommitInfo.h>
#include <lanes.h>
#include <GitSyncProcess.h>
#include <GitCloneProcess.h>
#include <GitRequestorProcess.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QPalette>
#include <QRegExp>
#include <QSettings>
#include <QTextCodec>
#include <QTextDocument>
#include <QTextStream>
#include <QDateTime>

#include <QLogger.h>

using namespace QLogger;

static const QString GIT_LOG_FORMAT = "%m%HX%P%n%cn<%ce>%n%an<%ae>%n%at%n%s%n%b";

namespace
{
#ifndef Q_OS_WIN32
#   include <sys/types.h> // used by chmod()
#   include <sys/stat.h> // used by chmod()
#endif

bool writeToFile(const QString &fileName, const QString &data)
{
   QFile file(fileName);
   if (!file.open(QIODevice::WriteOnly))
      return false;

   QString data2(data);
   QTextStream stream(&file);

#ifdef Q_OS_WIN32
   data2.replace("\r\n", "\n"); // change windows CRLF to linux
   data2.replace("\n", "\r\n"); // then change all linux CRLF to windows
#endif
   stream << data2;
   file.close();

   return true;
}
}

Git::Git()
   : QObject()
   , mRevCache(new RevisionsCache())
{
}

const QString Git::quote(const QString &nm)
{
   return ("$" + nm + "$");
}

// CT TODO utility function; can go elsewhere
const QString Git::quote(const QStringList &sl)
{
   QString q(sl.join(QString("$%1$").arg(' ')));
   q.prepend("$").append("$");
   return q;
}

uint Git::checkRef(const QString &sha, uint mask) const
{
   QHash<QString, Reference>::const_iterator it(mRefsShaMap.constFind(sha));
   return (it != mRefsShaMap.constEnd() ? (*it).type & mask : 0);
}

const QStringList Git::getRefNames(const QString &sha, uint mask) const
{
   QStringList result;
   if (!checkRef(sha, mask))
      return result;

   const Reference &rf = mRefsShaMap[sha];

   if (mask & TAG)
      result << rf.tags;

   if (mask & BRANCH)
      result << rf.branches;

   if (mask & RMT_BRANCH)
      result << rf.remoteBranches;

   if (mask & REF)
      result << rf.refs;

   if (mask == APPLIED || mask == UN_APPLIED)
      result << QStringList(rf.stgitPatch);

   return result;
}

const QString Git::filePath(const RevisionFile &rf, int i) const
{
   return mDirNames[rf.dirAt(i)] + mFileNames[rf.nameAt(i)];
}

CommitInfo Git::getCommitInfo(const QString &sha) const
{
   return mRevCache->getCommitInfo(sha);
}

QPair<bool, QString> Git::run(const QString &runCmd) const
{
   QString runOutput;
   GitSyncProcess p(mWorkingDir);
   connect(this, &Git::cancelAllProcesses, &p, &AGitProcess::onCancel);

   const auto ret = p.run(runCmd, runOutput);

   return qMakePair(ret, runOutput);
}

int Git::findFileIndex(const RevisionFile &rf, const QString &name)
{
   if (name.isEmpty())
      return -1;

   const auto idx = name.lastIndexOf('/') + 1;
   const auto dr = name.left(idx);
   const auto nm = name.mid(idx);

   for (int i = 0, cnt = rf.count(); i < cnt; ++i)
   {
      const auto isRevFile = mFileNames[rf.nameAt(i)];
      const auto isRevDir = mDirNames[rf.dirAt(i)];

      if (isRevFile == nm && isRevDir == dr)
         return i;
   }

   return -1;
}

GitExecResult Git::getCommitDiff(const QString &sha, const QString &diffToSha)
{
   if (!sha.isEmpty())
   {
      QString runCmd;

      if (sha != ZERO_SHA)
      {
         runCmd = "git diff-tree --no-color -r --patch-with-stat -C -m ";

         if (mRevCache->getCommitInfo(sha).parentsCount() == 0)
            runCmd.append("--root ");

         runCmd.append(QString("%1 %2").arg(diffToSha, sha)); // diffToSha could be empty
      }
      else
         runCmd = "git diff-index --no-color -r -m --patch-with-stat HEAD";

      return run(runCmd);
   }
   return qMakePair(false, QString());
}

QString Git::getFileDiff(const QString &currentSha, const QString &previousSha, const QString &file)
{
   QByteArray output;
   const auto ret = run(QString("git diff -U15000 %1 %2 %3").arg(previousSha, currentSha, file));

   if (ret.first)
      return ret.second;

   return QString();
}

bool Git::isNothingToCommit()
{
   if (!mRevCache->containsRevisionFile(ZERO_SHA))
      return true;

   const auto rf = mRevCache->getRevisionFile(ZERO_SHA);
   return rf.count() == workingDirInfo.otherFiles.count();
}

bool Git::resetFile(const QString &fileName)
{
   if (fileName.isEmpty())
      return false;

   return run(QString("git checkout %1").arg(fileName)).first;
}

GitExecResult Git::blame(const QString &file)
{
   return run(QString("git annotate %1").arg(file));
}

GitExecResult Git::history(const QString &file)
{
   return run(QString("git log --follow --pretty=%H %1").arg(file));
}

QPair<QString, QString> Git::getSplitCommitMsg(const QString &sha)
{
   const auto c = mRevCache->getCommitInfo(sha);

   return qMakePair(c.shortLog(), c.longLog().trimmed());
}

QVector<QString> Git::getSubmodules()
{
   QVector<QString> submodulesList;
   const auto ret = run("git config --file .gitmodules --name-only --get-regexp path");
   if (ret.first)
   {
      const auto submodules = ret.second.split('\n');
      for (auto submodule : submodules)
         if (!submodule.isEmpty() && submodule != "\n")
            submodulesList.append(submodule.split('.').at(1));
   }

   return submodulesList;
}

bool Git::submoduleAdd(const QString &url, const QString &name)
{
   return run(QString("git submodule add %1 %2").arg(url).arg(name)).first;
}

bool Git::submoduleUpdate(const QString &)
{
   return run("git submodule update --init --recursive").first;
}

bool Git::submoduleRemove(const QString &)
{
   return false;
}

RevisionFile Git::insertNewFiles(const QString &sha, const QString &data)
{
   /* we use an independent FileNamesLoader to avoid data
    * corruption if we are loading file names in background
    */
   FileNamesLoader fl;

   RevisionFile rf;
   parseDiffFormat(rf, data, fl);
   flushFileNames(fl);

   mRevCache->insertRevisionFile(sha, rf);

   return rf;
}

bool Git::runDiffTreeWithRenameDetection(const QString &runCmd, QString *runOutput)
{
   /* Under some cases git could warn out:

       "too many files, skipping inexact rename detection"

    So if this occurs fallback on NO rename detection.
 */
   QString cmd(runCmd); // runCmd must be without -C option
   cmd.replace("git diff-tree", "git diff-tree -C");

   const auto renameDetection = run(cmd);

   *runOutput = renameDetection.second;

   if (!renameDetection.first) // retry without rename detection
   {
      const auto ret2 = run(runCmd);
      if (ret2.first)
         *runOutput = ret2.second;

      return ret2.first;
   }

   return true;
}

RevisionFile Git::getWipFiles()
{
   return mRevCache->getRevisionFile(ZERO_SHA); // ZERO_SHA search arrives here
}

RevisionFile Git::getCommitFiles(const QString &sha) const
{
   const auto r = mRevCache->getCommitInfo(sha);

   if (r.parentsCount() != 0 && mRevCache->containsRevisionFile(sha))
      return mRevCache->getRevisionFile(sha);

   return RevisionFile();
}

RevisionFile Git::getDiffFiles(const QString &sha, const QString &diffToSha, bool allFiles)
{
   const auto r = mRevCache->getCommitInfo(sha);
   if (r.parentsCount() == 0)
      return RevisionFile();

   QString mySha;
   QString runCmd = QString("git diff-tree --no-color -r -m ");

   if (r.parentsCount() > 1 && diffToSha.isEmpty() && allFiles)
   {
      mySha = QString("ALL_MERGE_FILES" + QString(sha));
      runCmd.append(sha);
   }
   else if (!diffToSha.isEmpty() && (sha != ZERO_SHA))
   {
      mySha = sha;
      runCmd.append(diffToSha + " " + sha);
   }

   if (mRevCache->containsRevisionFile(mySha))
      return mRevCache->getRevisionFile(mySha);

   QString runOutput;

   if (runDiffTreeWithRenameDetection(runCmd, &runOutput))
      return insertNewFiles(mySha, runOutput);

   return RevisionFile();
}

bool Git::resetCommits(int parentDepth)
{
   QString runCmd("git reset --soft HEAD~");
   runCmd.append(QString::number(parentDepth));
   return run(runCmd).first;
}

GitExecResult Git::checkoutCommit(const QString &sha)
{
   return run(QString("git checkout %1").arg(sha));
}

GitExecResult Git::markFileAsResolved(const QString &fileName)
{
   const auto ret = run(QString("git add %1").arg(fileName));

   if (ret.first)
      updateWipRevision();

   return ret;
}

GitExecResult Git::merge(const QString &into, QStringList sources)
{
   const auto ret = run(QString("git checkout -q %1").arg(into));

   if (!ret.first)
      return ret;

   return run(QString("git merge -q ") + sources.join(" "));
}

const QStringList Git::getOtherFiles(const QStringList &selFiles)
{
   RevisionFile files = getWipFiles(); // files != nullptr
   QStringList notSelFiles;
   for (auto i = 0; i < files.count(); ++i)
   {
      const QString &fp = filePath(files, i);
      if (selFiles.indexOf(fp) == -1 && files.statusCmp(i, RevisionFile::IN_INDEX))
         notSelFiles.append(fp);
   }
   return notSelFiles;
}

bool Git::updateIndex(const QStringList &selFiles)
{
   const auto files = getWipFiles(); // files != nullptr

   QStringList toAdd, toRemove;

   for (auto it : selFiles)
   {
      const auto idx = findFileIndex(files, it);

      idx != -1 && files.statusCmp(idx, RevisionFile::DELETED) ? toRemove << it : toAdd << it;
   }
   if (!toRemove.isEmpty() && !run("git rm --cached --ignore-unmatch -- " + quote(toRemove)).first)
      return false;

   if (!toAdd.isEmpty() && !run("git add -- " + quote(toAdd)).first)
      return false;

   return true;
}

bool Git::commitFiles(QStringList &selFiles, const QString &msg, bool amend, const QString &author)
{
   const QString msgFile(mGitDir + "/qgit_cmt_msg.txt");
   if (!writeToFile(msgFile, msg)) // early skip
      return false;

   // add user selectable commit options
   QString cmtOptions;

   if (amend)
   {
      cmtOptions.append(" --amend");

      if (!author.isEmpty())
         cmtOptions.append(QString(" --author \"%1\"").arg(author));
   }

   bool ret = true;

   // get not selected files but updated in index to restore at the end
   const QStringList notSel(getOtherFiles(selFiles));

   // call git reset to remove not selected files from index
   if ((!notSel.empty() && !run("git reset -- " + quote(notSel)).first) || !updateIndex(selFiles)
       || !run("git commit" + cmtOptions + " -F " + quote(msgFile)).first || (!notSel.empty() && !updateIndex(notSel)))
   {
      QDir dir(mWorkingDir);
      dir.remove(msgFile);
      ret = false;
   }

   return ret;
}

GitExecResult Git::exportPatch(const QStringList &shaList)
{
   auto val = 1;
   QStringList files;

   for (const auto &sha : shaList)
   {
      const auto ret = run(QString("git format-patch -1 %1").arg(sha));

      if (!ret.first)
         break;
      else
      {
         auto filename = ret.second;
         filename = filename.remove("\n");
         const auto text = filename.mid(filename.indexOf("-") + 1);
         const auto number = QString("%1").arg(val, 4, 10, QChar('0'));
         const auto newFileName = QString("%1-%2").arg(number, text);
         files.append(newFileName);

         QFile::rename(QString("%1/%2").arg(mWorkingDir, filename), QString("%1/%2").arg(mWorkingDir, newFileName));
         ++val;
      }
   }

   if (val != shaList.count())
      QLog_Error("Git", QString("Problem generating patches. Stop after {%1} iterations").arg(val));

   return qMakePair(true, QVariant(files));
}

bool Git::apply(const QString &fileName, bool asCommit)
{
   const auto cmd = asCommit ? QString("git am --signof") : QString("git apply");
   const auto ret = run(QString("%1 %2").arg(cmd, fileName));

   return ret.first;
}

GitExecResult Git::push(bool force)
{
   const auto ret = run(QString("git push ").append(force ? QString("--force") : QString()));

   if (ret.second.contains("has no upstream branch"))
      return run(QString("git push --set-upstream origin %1").arg(mCurrentBranchName));

   return ret;
}

GitExecResult Git::pull()
{
   return run("git pull");
}

bool Git::fetch()
{
   return run("git fetch --all --tags --prune --force").first;
}

GitExecResult Git::cherryPickCommit(const QString &sha)
{
   return run(QString("git cherry-pick %1").arg(sha));
}

GitExecResult Git::pop() const
{
   return run("git stash pop");
}

bool Git::stash()
{
   return run("git stash").first;
}

GitExecResult Git::stashBranch(const QString &stashId, const QString &branchName)
{
   return run(QString("git stash branch %1 %2").arg(branchName, stashId));
}

GitExecResult Git::stashDrop(const QString &stashId)
{
   return run(QString("git stash drop -q %1").arg(stashId));
}

GitExecResult Git::stashClear()
{
   return run("git stash clear");
}

bool Git::resetCommit(const QString &sha, Git::CommitResetType type)
{
   QString typeStr;

   switch (type)
   {
      case CommitResetType::SOFT:
         typeStr = "soft";
         break;
      case CommitResetType::MIXED:
         typeStr = "mixed";
         break;
      case CommitResetType::HARD:
         typeStr = "hard";
         break;
   }

   return run(QString("git reset --%1 %2").arg(typeStr, sha)).first;
}

GitExecResult Git::createBranchFromAnotherBranch(const QString &oldName, const QString &newName)
{
   return run(QString("git branch %1 %2").arg(newName, oldName));
}

GitExecResult Git::createBranchAtCommit(const QString &commitSha, const QString &branchName)
{
   return run(QString("git branch %1 %2").arg(branchName, commitSha));
}

GitExecResult Git::checkoutRemoteBranch(const QString &branchName)
{
   return run(QString("git checkout -q %1").arg(branchName));
}

GitExecResult Git::checkoutNewLocalBranch(const QString &branchName)
{
   return run(QString("git checkout -b %1").arg(branchName));
}

GitExecResult Git::renameBranch(const QString &oldName, const QString &newName)
{
   return run(QString("git branch -m %1 %2").arg(oldName, newName));
}

GitExecResult Git::removeLocalBranch(const QString &branchName)
{
   return run(QString("git branch -D %1").arg(branchName));
}

GitExecResult Git::removeRemoteBranch(const QString &branchName)
{
   return run(QString("git push --delete origin %1").arg(branchName));
}

GitExecResult Git::getBranches()
{
   return run(QString("git branch -a"));
}

GitExecResult Git::getDistanceBetweenBranches(bool toMaster, const QString &right)
{
   const QString firstArg = toMaster ? QString::fromUtf8("--left-right") : QString::fromUtf8("");
   const QString gitCmd = QString("git rev-list %1 --count %2...%3")
                              .arg(firstArg)
                              .arg(toMaster ? QString::fromUtf8("origin/master") : QString::fromUtf8("origin/%3"))
                              .arg(right);

   return run(gitCmd);
}

GitExecResult Git::getBranchesOfCommit(const QString &sha)
{
   return run(QString("git branch --contains %1 --all").arg(sha));
}

GitExecResult Git::getLastCommitOfBranch(const QString &branch)
{
   auto ret = run(QString("git rev-parse %1").arg(branch));

   if (ret.first)
      ret.second.remove(ret.second.count() - 1, ret.second.count());

   return ret;
}

GitExecResult Git::prune()
{
   return run("git remote prune origin");
}

QVector<QString> Git::getTags() const
{
   const auto ret = run("git tag");

   QVector<QString> tags;

   if (ret.first)
   {
      const auto tagsTmp = ret.second.split("\n");

      for (auto tag : tagsTmp)
         if (tag != "\n" && !tag.isEmpty())
            tags.append(tag);
   }

   return tags;
}

QVector<QString> Git::getLocalTags() const
{
   const auto ret = run("git push --tags --dry-run");

   QVector<QString> tags;

   if (ret.first)
   {
      const auto tagsTmp = ret.second.split("\n");

      for (auto tag : tagsTmp)
         if (tag != "\n" && !tag.isEmpty() && tag.contains("[new tag]"))
            tags.append(tag.split(" -> ").last());
   }

   return tags;
}

bool Git::addTag(const QString &tagName, const QString &tagMessage, const QString &sha, QByteArray &output)
{
   const auto ret = run(QString("git tag -a %1 %2 -m \"%3\"").arg(tagName).arg(sha).arg(tagMessage));
   output = ret.second.toUtf8();

   return ret.first;
}

bool Git::removeTag(const QString &tagName, bool remote)
{
   auto ret = false;

   if (remote)
      ret = run(QString("git push origin --delete %1").arg(tagName)).first;

   if (!remote || (remote && ret))
      ret = run(QString("git tag -d %1").arg(tagName)).first;

   return ret;
}

bool Git::pushTag(const QString &tagName, QByteArray &output)
{
   const auto ret = run(QString("git push origin %1").arg(tagName));
   output = ret.second.toUtf8();

   return ret.first;
}

GitExecResult Git::getTagCommit(const QString &tagName)
{
   const auto ret = run(QString("git rev-list -n 1 %1").arg(tagName));
   QString output = ret.second;

   if (ret.first)
   {
      output.remove(output.count() - 2, output.count() - 1);
   }

   return qMakePair(ret.first, output);
}

QVector<QString> Git::getStashes()
{
   const auto ret = run("git stash list");

   QVector<QString> stashes;

   if (ret.first)
   {
      const auto tagsTmp = ret.second.split("\n");

      for (auto tag : tagsTmp)
         if (tag != "\n" && !tag.isEmpty())
            stashes.append(tag);
   }

   return stashes;
}

bool Git::setGitDbDir(const QString &wd)
{
   auto tmp = mWorkingDir;
   mWorkingDir = wd;

   const auto success = run("git rev-parse --git-dir"); // run under newWorkDir
   mWorkingDir = tmp;

   const auto runOutput = success.second.trimmed();

   if (success.first)
   {
      QDir d(runOutput.startsWith("/") ? runOutput : wd + "/" + runOutput);
      mGitDir = d.absolutePath();
   }

   return success.first;
}

GitExecResult Git::getBaseDir(const QString &wd)
{
   auto tmp = mWorkingDir;
   mWorkingDir = wd;

   const auto ret = run("git rev-parse --show-cdup");
   mWorkingDir = tmp;

   auto baseDir = wd;

   if (ret.first)
   {
      QDir d(QString("%1/%2").arg(wd, ret.second.trimmed()));
      baseDir = d.absolutePath();
   }

   return qMakePair(ret.first, baseDir);
}

Git::Reference *Git::lookupOrAddReference(const QString &sha)
{
   QHash<QString, Reference>::iterator it(mRefsShaMap.find(sha));
   if (it == mRefsShaMap.end())
      it = mRefsShaMap.insert(sha, Reference());
   return &(*it);
}

bool Git::loadCurrentBranch()
{
   const auto ret2 = run("git branch");

   if (!ret2.first)
      return false;

   const auto branches = ret2.second.trimmed().split('\n');
   for (auto branch : branches)
   {
      if (branch.startsWith("*"))
      {
         mCurrentBranchName = branch.remove("*").trimmed();
         break;
      }
   }

   if (mCurrentBranchName.contains(" detached "))
      mCurrentBranchName = "";

   return true;
}

void Git::Reference::configure(const QString &refName, bool isCurrentBranch, const QString &prevRefSha)
{
   if (refName.startsWith("refs/tags/"))
   {
      if (refName.endsWith("^{}"))
      {
         // we assume that a tag dereference follows strictly
         // the corresponding tag object in rLst. So the
         // last added tag is a tag object, not a commit object
         tags.append(refName.mid(10, refName.length() - 13));

         // store tag object. Will be used to fetching
         // tag message (if any) when necessary.
         tagObj = prevRefSha;
      }
      else
         tags.append(refName.mid(10));

      type |= TAG;
   }
   else if (refName.startsWith("refs/heads/"))
   {
      branches.append(refName.mid(11));
      type |= BRANCH;

      if (isCurrentBranch)
         type |= CUR_BRANCH;
   }
   else if (refName.startsWith("refs/remotes/") && !refName.endsWith("HEAD"))
   {
      remoteBranches.append(refName.mid(13));
      type |= RMT_BRANCH;
   }
   else if (!refName.startsWith("refs/bases/") && !refName.endsWith("HEAD"))
   {
      refs.append(refName);
      type |= REF;
   }
}

bool Git::getRefs()
{
   const auto branchLoaded = loadCurrentBranch();

   if (branchLoaded)
   {
      const auto ret3 = run("git show-ref -d");

      if (ret3.first)
      {
         mRefsShaMap.clear();

         const auto ret = getLastCommitOfBranch("HEAD");

         QString prevRefSha;
         const auto curBranchSHA = ret.output.toString();
         const auto referencesList = ret3.second.split('\n', QString::SkipEmptyParts);

         for (auto reference : referencesList)
         {
            const auto revSha = reference.left(40);
            const auto refName = reference.mid(41);

            // one Revision could have many tags
            const auto cur = lookupOrAddReference(revSha);

            cur->configure(refName, curBranchSHA == revSha, prevRefSha);

            if (refName.startsWith("refs/tags/") && refName.endsWith("^{}") && !prevRefSha.isEmpty())
               mRefsShaMap.remove(prevRefSha);

            prevRefSha = revSha;
         }

         // mark current head (even when detached)
         auto cur = lookupOrAddReference(curBranchSHA);
         cur->type |= CUR_BRANCH;

         return !mRefsShaMap.empty();
      }
   }

   return false;
}

const QStringList Git::getOthersFiles()
{
   // add files present in working directory but not in git archive

   QString runCmd("git ls-files --others");
   QString exFile(".git/info/exclude");
   QString path = QString("%1/%2").arg(mWorkingDir, exFile);

   if (QFile::exists(path))
      runCmd.append(" --exclude-from=" + quote(exFile));

   runCmd.append(" --exclude-per-directory=" + quote(".gitignore"));

   const auto runOutput = run(runCmd).second;
   return runOutput.split('\n', QString::SkipEmptyParts);
}

RevisionFile Git::fakeWorkDirRevFile(const WorkingDirInfo &wd)
{
   FileNamesLoader fl;
   RevisionFile rf;
   parseDiffFormat(rf, wd.diffIndex, fl);
   rf.setOnlyModified(false);

   for (auto it : wd.otherFiles)
   {
      appendFileName(rf, it, fl);
      rf.setStatus(RevisionFile::UNKNOWN);
      rf.mergeParent.append(1);
   }

   RevisionFile cachedFiles;
   parseDiffFormat(cachedFiles, wd.diffIndexCached, fl);
   flushFileNames(fl);

   for (auto i = 0; i < rf.count(); i++)
   {
      if (findFileIndex(cachedFiles, filePath(rf, i)) != -1)
      {
         if (cachedFiles.statusCmp(i, RevisionFile::CONFLICT))
            rf.appendStatus(i, RevisionFile::CONFLICT);

         rf.appendStatus(i, RevisionFile::IN_INDEX);
      }
   }

   return rf;
}

void Git::updateWipRevision()
{
   const auto ret = run("git status");
   if (!ret.first) // git status refreshes the index, run as first
      return;

   QString status = ret.second;

   const auto ret2 = run("git rev-parse --revs-only HEAD");
   if (!ret2.first)
      return;

   QString head = ret2.second;

   head = head.trimmed();
   if (!head.isEmpty())
   { // repository initialized but still no history
      const auto ret3 = run("git diff-index " + head);

      if (!ret3.first)
         return;

      workingDirInfo.diffIndex = ret3.second;

      // check for files already updated in cache, we will
      // save this information in status third field
      const auto ret4 = run("git diff-index --cached " + head);
      if (!ret4.first)
         return;

      workingDirInfo.diffIndexCached = ret4.second;
   }
   // get any file not in tree
   workingDirInfo.otherFiles = getOthersFiles();

   // now mockup a RevisionFile
   mRevCache->insertRevisionFile(ZERO_SHA, fakeWorkDirRevFile(workingDirInfo));

   // then mockup the corresponding Revision
   const QString &log = (isNothingToCommit() ? QString("No local changes") : QString("Local changes"));

   CommitInfo c(ZERO_SHA, { head }, "-", QDateTime::currentDateTime().toSecsSinceEpoch(), log, status, 0);
   c.isDiffCache = true;

   mRevCache->updateWipCommit(std::move(c));
}

void Git::parseDiffFormatLine(RevisionFile &rf, const QString &line, int parNum, FileNamesLoader &fl)
{
   if (line[1] == ':')
   { // it's a combined merge
      /* For combined merges rename/copy information is useless
       * because nor the original file name, nor similarity info
       * is given, just the status tracks that in the left/right
       * branch a renamed/copy occurred (as example status could
       * be RM or MR). For visualization purposes we could consider
       * the file as modified
       */
      appendFileName(rf, line.section('\t', -1), fl);
      rf.setStatus("M");
      rf.mergeParent.append(parNum);
   }
   else
   { // faster parsing in normal case
      if (line.at(98) == '\t')
      {
         appendFileName(rf, line.mid(99), fl);
         rf.setStatus(line.at(97));
         rf.mergeParent.append(parNum);
      }
      else
         // it's a rename or a copy, we are not in fast path now!
         setExtStatus(rf, line.mid(97), parNum, fl);
   }
}

void Git::setExtStatus(RevisionFile &rf, const QString &rowSt, int parNum, FileNamesLoader &fl)
{
   const QStringList sl(rowSt.split('\t', QString::SkipEmptyParts));
   if (sl.count() != 3)
      return;

   // we want store extra info with format "orig --> dest (Rxx%)"
   // but git give us something like "Rxx\t<orig>\t<dest>"
   QString type = sl[0];
   type.remove(0, 1);
   const QString &orig = sl[1];
   const QString &dest = sl[2];
   const QString extStatusInfo(orig + " --> " + dest + " (" + QString::number(type.toInt()) + "%)");

   /*
    NOTE: we set rf.extStatus size equal to position of latest
          copied/renamed file. So it can have size lower then
          rf.count() if after copied/renamed file there are
          others. Here we have no possibility to know final
          dimension of this RefFile. We are still in parsing.
 */

   // simulate new file
   appendFileName(rf, dest, fl);
   rf.mergeParent.append(parNum);
   rf.setStatus(RevisionFile::NEW);
   rf.appendExtStatus(extStatusInfo);

   // simulate deleted orig file only in case of rename
   if (type.at(0) == 'R')
   { // renamed file
      appendFileName(rf, orig, fl);
      rf.mergeParent.append(parNum);
      rf.setStatus(RevisionFile::DELETED);
      rf.appendExtStatus(extStatusInfo);
   }
   rf.setOnlyModified(false);
}

// CT TODO utility function; can go elsewhere
void Git::parseDiffFormat(RevisionFile &rf, const QString &buf, FileNamesLoader &fl)
{
   int parNum = 1, startPos = 0, endPos = buf.indexOf('\n');

   while (endPos != -1)
   {
      const QString &line = buf.mid(startPos, endPos - startPos);
      if (line[0] == ':') // avoid sha's in merges output
         parseDiffFormatLine(rf, line, parNum, fl);
      else
         parNum++;

      startPos = endPos + 1;
      endPos = buf.indexOf('\n', endPos + 99);
   }
}

bool Git::checkoutRevisions()
{
   auto baseCmd = QString("git log --date-order --no-color --log-size --parents --boundary -z --pretty=format:")
                      .append(GIT_LOG_FORMAT)
                      .append(" --all");

   const auto requestor = new GitRequestorProcess(mWorkingDir);
   connect(requestor, &GitRequestorProcess::procDataReady, this, &Git::processRevision);
   connect(this, &Git::cancelAllProcesses, requestor, &AGitProcess::onCancel);

   QString buf;
   const auto ret = requestor->run(baseCmd, buf);

   return ret;
}

bool Git::clone(const QString &url, const QString &fullPath)
{
   const auto asyncRun = new GitCloneProcess(mWorkingDir);
   connect(asyncRun, &GitCloneProcess::signalProgress, this, &Git::signalCloningProgress, Qt::DirectConnection);

   QString buffer;
   return asyncRun->run(QString("git clone --progress %1 %2").arg(url, fullPath), buffer);
}

bool Git::initRepo(const QString &fullPath)
{
   return run(QString("git init %1").arg(fullPath)).first;
}

GitUserInfo Git::getGlobalUserInfo() const
{
   GitUserInfo userInfo;

   const auto nameRequest = run("git config --get --global user.name");

   if (nameRequest.first)
      userInfo.mUserName = nameRequest.second.trimmed();

   const auto emailRequest = run("git config --get --global user.email");

   if (emailRequest.first)
      userInfo.mUserEmail = emailRequest.second.trimmed();

   return userInfo;
}

void Git::setGlobalUserInfo(const GitUserInfo &info)
{
   run(QString("git config --global user.name \"%1\"").arg(info.mUserName));
   run(QString("git config --global user.email %1").arg(info.mUserEmail));
}

GitUserInfo Git::getLocalUserInfo() const
{
   GitUserInfo userInfo;

   const auto nameRequest = run("git config --get --local user.name");

   if (nameRequest.first)
      userInfo.mUserName = nameRequest.second.trimmed();

   const auto emailRequest = run("git config --get --local user.email");

   if (emailRequest.first)
      userInfo.mUserEmail = emailRequest.second.trimmed();

   return userInfo;
}

void Git::setLocalUserInfo(const GitUserInfo &info)
{
   run(QString("git config --local user.name \"%1\"").arg(info.mUserName));
   run(QString("git config --local user.email %1").arg(info.mUserEmail));
}

int Git::totalCommits() const
{
   return mRevCache->count();
}

CommitInfo Git::getCommitInfoByRow(int row) const
{
   return mRevCache->getCommitInfoByRow(row);
}

CommitInfo Git::getCommitInfo(const QString &sha)
{
   return mRevCache->getCommitInfo(sha);
}

void Git::clearRevs()
{
   mRevCache->clear();
   mRevCache->clearRevisionFile();
   workingDirInfo.clear();
}

void Git::clearFileNames()
{
   mFileNamesMap.clear();
   mDirNamesMap.clear();
   mDirNames.clear();
   mFileNames.clear();
}

bool Git::loadRepository(const QString &wd)
{
   if (!isLoading)
   {
      QLog_Info("Git", "Initializing Git...");

      // normally called when changing git directory. Must be called after stop()
      clearRevs();
      clearFileNames();

      const auto isGIT = setGitDbDir(wd);

      if (!isGIT)
         return false;

      isLoading = true;

      const auto ret = getBaseDir(wd);

      if (ret.success)
         mWorkingDir = ret.output.toString();

      getRefs();

      checkoutRevisions();

      QLog_Info("Git", "... Git init finished");

      return true;
   }

   return false;
}

void Git::processRevision(const QByteArray &ba)
{
   QByteArray auxBa = ba;
   const auto commits = ba.split('\000');
   const auto totalCommits = commits.count();
   auto count = 1;

   mRevCache->configure(totalCommits);

   emit signalLoadingStarted();

   updateWipRevision();

   for (const auto &commitInfo : commits)
   {
      CommitInfo revision(commitInfo, count++);

      if (revision.isValid())
         mRevCache->insertCommitInfo(std::move(revision));
      else
         break;
   }

   isLoading = false;

   emit signalLoadingFinished();
}

void Git::flushFileNames(FileNamesLoader &fl)
{
   if (!fl.rf)
      return;

   QByteArray &b = fl.rf->pathsIdx;
   QVector<int> &dirs = fl.rfDirs;

   b.clear();
   b.resize(2 * dirs.size() * static_cast<int>(sizeof(int)));

   int *d = (int *)(b.data());

   for (int i = 0; i < dirs.size(); i++)
   {
      d[i] = dirs.at(i);
      d[dirs.size() + i] = fl.rfNames.at(i);
   }
   dirs.clear();
   fl.rfNames.clear();
   fl.rf = nullptr;
}

void Git::appendFileName(RevisionFile &rf, const QString &name, FileNamesLoader &fl)
{
   if (fl.rf != &rf)
   {
      flushFileNames(fl);
      fl.rf = &rf;
   }
   int idx = name.lastIndexOf('/') + 1;
   const QString &dr = name.left(idx);
   const QString &nm = name.mid(idx);

   QHash<QString, int>::const_iterator it(mDirNamesMap.constFind(dr));
   if (it == mDirNamesMap.constEnd())
   {
      int idx = mDirNames.count();
      mDirNamesMap.insert(dr, idx);
      mDirNames.append(dr);
      fl.rfDirs.append(idx);
   }
   else
      fl.rfDirs.append(*it);

   it = mFileNamesMap.constFind(nm);
   if (it == mFileNamesMap.constEnd())
   {
      int idx = mFileNames.count();
      mFileNamesMap.insert(nm, idx);
      mFileNames.append(nm);
      fl.rfNames.append(idx);
   }
   else
      fl.rfNames.append(*it);
}

bool GitUserInfo::isValid() const
{
   return !mUserEmail.isNull() && !mUserEmail.isEmpty() && !mUserName.isNull() && !mUserName.isEmpty();
}

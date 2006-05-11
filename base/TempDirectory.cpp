/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "TempDirectory.h"
#include "System.h"

#include <QDir>
#include <QFile>
#include <QMutexLocker>

#include <iostream>
#include <cassert>

TempDirectory *
TempDirectory::m_instance = new TempDirectory;

TempDirectory *
TempDirectory::instance()
{
    return m_instance;
}

TempDirectory::DirectoryCreationFailed::DirectoryCreationFailed(QString directory) throw() :
    m_directory(directory)
{
    std::cerr << "ERROR: Directory creation failed for directory: "
              << directory.toLocal8Bit().data() << std::endl;
}

const char *
TempDirectory::DirectoryCreationFailed::what() const throw()
{
    return QString("Directory creation failed for \"%1\"")
        .arg(m_directory).toLocal8Bit().data();
}

TempDirectory::TempDirectory() :
    m_tmpdir("")
{
}

TempDirectory::~TempDirectory()
{
    std::cerr << "TempDirectory::~TempDirectory" << std::endl;

    cleanup();
}

void
TempDirectory::cleanup()
{
    cleanupDirectory("");
}

QString
TempDirectory::getPath()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_tmpdir != "") return m_tmpdir;

    QString svDirBase = ".sv";
    QString svDir = QDir::home().filePath(svDirBase);
    if (!QFileInfo(svDir).exists()) {
        if (!QDir::home().mkdir(svDirBase)) {
            throw DirectoryCreationFailed(QString("%1 directory in $HOME")
                                          .arg(svDirBase));
        }
    } else if (!QFileInfo(svDir).isDir()) {
        throw DirectoryCreationFailed(QString("$HOME/%1 is not a directory")
                                      .arg(svDirBase));
    }

    cleanupAbandonedDirectories(svDir);

    return createTempDirectoryIn(svDir);
}

QString
TempDirectory::createTempDirectoryIn(QString dir)
{
    // Entered with mutex held.

    QDir tempDirBase(dir);

    // Generate a temporary directory.  Qt4.1 doesn't seem to be able
    // to do this for us, and mkdtemp is not standard.  This method is
    // based on the way glibc does mkdtemp.

    static QString chars =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    QString suffix;
    int padlen = 6, attempts = 100;
    unsigned int r = time(0) ^ getpid();

    for (int i = 0; i < padlen; ++i) {
        suffix += "X";
    }
    
    for (int j = 0; j < attempts; ++j) {

        unsigned int v = r;
        
        for (int i = 0; i < padlen; ++i) {
            suffix[i] = chars[v % 62];
            v /= 62;
        }

        QString candidate = QString("sv_%1").arg(suffix);

        if (tempDirBase.mkpath(candidate)) {
            m_tmpdir = tempDirBase.filePath(candidate);
            break;
        }

        r = r + 7777;
    }

    if (m_tmpdir == "") {
        throw DirectoryCreationFailed(QString("temporary subdirectory in %1")
                                      .arg(tempDirBase.canonicalPath()));
    }

    QString pidpath = QDir(m_tmpdir).filePath(QString("%1.pid").arg(getpid()));
    QFile pidfile(pidpath);

    if (!pidfile.open(QIODevice::WriteOnly)) {
        throw DirectoryCreationFailed(QString("pid file creation in %1")
                                      .arg(m_tmpdir));
    } else {
        pidfile.close();
    }

    return m_tmpdir;
}

QString
TempDirectory::getSubDirectoryPath(QString subdir)
{
    QString tmpdirpath = getPath();
    
    QMutexLocker locker(&m_mutex);

    QDir tmpdir(tmpdirpath);
    QFileInfo fi(tmpdir.filePath(subdir));

    if (!fi.exists()) {
        if (!tmpdir.mkdir(subdir)) {
            throw DirectoryCreationFailed(fi.filePath());
        } else {
            return fi.filePath();
        }
    } else if (fi.isDir()) {
        return fi.filePath();
    } else {
        throw DirectoryCreationFailed(fi.filePath());
    }
}

void
TempDirectory::cleanupDirectory(QString tmpdir)
{
    bool isRoot = false;

    if (tmpdir == "") {

        m_mutex.lock();

        isRoot = true;
        tmpdir = m_tmpdir;

        if (tmpdir == "") {
            m_mutex.unlock();
            return;
        }
    }

    QDir dir(tmpdir);
    dir.setFilter(QDir::Dirs | QDir::Files);

    for (unsigned int i = 0; i < dir.count(); ++i) {

        if (dir[i] == "." || dir[i] == "..") continue;
        QFileInfo fi(dir.filePath(dir[i]));

        if (fi.isDir()) {
            cleanupDirectory(fi.absoluteFilePath());
        } else {
            if (!QFile(fi.absoluteFilePath()).remove()) {
                std::cerr << "WARNING: TempDirectory::cleanup: "
                          << "Failed to unlink file \""
                          << fi.absoluteFilePath().toStdString() << "\""
                          << std::endl;
            }
        }
    }

    QString dirname = dir.dirName();
    if (dirname != "") {
        if (!dir.cdUp()) {
            std::cerr << "WARNING: TempDirectory::cleanup: "
                      << "Failed to cd to parent directory of "
                      << tmpdir.toStdString() << std::endl;
            return;
        }
        if (!dir.rmdir(dirname)) {
            std::cerr << "WARNING: TempDirectory::cleanup: "
                      << "Failed to remove directory "
                      << dirname.toStdString() << std::endl;
        } 
    }

    if (isRoot) {
        m_tmpdir = "";
        m_mutex.unlock();
    }
}

void
TempDirectory::cleanupAbandonedDirectories(QString svDir)
{
    QDir dir(svDir, "sv_*", QDir::Name, QDir::Dirs);

    for (unsigned int i = 0; i < dir.count(); ++i) {
        
        QDir subdir(dir.filePath(dir[i]), "*.pid", QDir::Name, QDir::Files);

        for (unsigned int j = 0; j < subdir.count(); ++j) {

            bool ok = false;
            int pid = QFileInfo(subdir[j]).baseName().toInt(&ok);
            if (!ok) continue;

            if (GetProcessStatus(pid) == ProcessNotRunning) {
                std::cerr << "INFO: Found abandoned temporary directory from "
                          << "an old Sonic Visualiser process\n(pid=" << pid
                          << ", directory=\""
                          << dir.filePath(dir[i]).toStdString()
                          << "\").  Removing it..." << std::endl;
                cleanupDirectory(dir.filePath(dir[i]));
                std::cerr << "...done." << std::endl;
                break;
            }
        }
    }
}


        
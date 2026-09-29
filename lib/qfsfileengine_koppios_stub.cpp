/**
 * @file lib/qfsfileengine_koppios_stub.cpp
 * @brief Not upstream Qt, and not a real QFSFileEngine implementation:
 *        just enough of it to link.
 *
 * Real Qt's qfile.cpp / qfiledevice.cpp reference the concrete
 * QFSFileEngine class directly in two places that have nothing to do
 * with QAbstractFileEngineHandler-based dispatch: QFileDevicePrivate::
 * engine()'s fallback (`fileEngine = std::make_unique<QFSFileEngine>()`,
 * used only when a QFile was never given a name to look a handler up by)
 * and QFile::remove()/moveToTrash()'s `static_cast<QFSFileEngine *>(...)
 * ->isUnnamedFile()` check. Neither is ever actually reached by a
 * QKoppiosFileEngine-backed QFile (see lib/qfileengine_koppios.cpp,
 * whose handler intercepts every named file before QFileSystemEngine
 * would fall back to QFSFileEngine at all) -- but the calls still need
 * to compile and link regardless of whether they run.
 *
 * The real qfsfileengine.cpp (1000+ lines) is not vendored: its actual
 * work -- open/read/write/seek/stat -- is done by qfsfileengine_unix.cpp,
 * real POSIX this kernel does not have (see lib/pthread_glibc.c's file
 * comment for what it has instead). This file supplies only the handful
 * of trivial, platform-generic members (construction/destruction/
 * isUnnamedFile, the last already inline in qfsfileengine_p.h) that the
 * two call sites above actually need a symbol for.
 */
#include "qfsfileengine_p.h"

QT_BEGIN_NAMESPACE

QFSFileEnginePrivate::QFSFileEnginePrivate() : QAbstractFileEnginePrivate()
{
    init();
}

void QFSFileEnginePrivate::init()
{
    is_sequential = 0;
    tried_stat = 0;
    need_lstat = 1;
    is_link = 0;
    openMode = QIODevice::NotOpen;
    fd = -1;
    fh = nullptr;
    lastIOCommand = IOFlushCommand;
    lastFlushFailed = false;
    closeFileHandle = false;
}

void QFSFileEnginePrivate::unmapAll()
{
    /* No mapping is ever created against an object of this class in this
     * tree -- map()/unmap() are not implemented here at all. */
}

QFSFileEngine::QFSFileEngine() : QAbstractFileEngine(*new QFSFileEnginePrivate)
{
}

QFSFileEngine::QFSFileEngine(const QString &file) : QAbstractFileEngine(*new QFSFileEnginePrivate)
{
    Q_D(QFSFileEngine);
    d->fileEntry = QFileSystemEntry(file);
}

QFSFileEngine::~QFSFileEngine()
{
    /* Real Qt's destructor closes d->fh/d->fd and unmaps here; both are
     * always their init() defaults (nullptr / -1) on this class in this
     * tree, so there is nothing to release. */
}

/* Every method below is a declared override real Qt gives a genuine
 * POSIX-backed body in qfsfileengine_unix.cpp (see this file's own
 * comment for why that is not vendored here). None of them are ever
 * actually invoked on an instance of this class in this tree -- but a
 * virtual override still needs a vtable slot filled to link at all, even
 * for a call site nothing reaches at runtime. Each one just forwards to
 * QAbstractFileEngine's own real, already-vendored (qabstractfileengine.cpp)
 * "not supported" default, i.e. this subclass genuinely adds nothing
 * beyond the abstract base. rename_helper() has no base-class equivalent
 * to forward to (it is QFSFileEngine's own private helper, not part of
 * QAbstractFileEngine's interface), so it just reports failure directly. */

bool QFSFileEngine::open(QIODevice::OpenMode openMode, std::optional<QFile::Permissions> permissions)
{
    return QAbstractFileEngine::open(openMode, permissions);
}
bool QFSFileEngine::close()
{
    return QAbstractFileEngine::close();
}
bool QFSFileEngine::flush()
{
    return QAbstractFileEngine::flush();
}
bool QFSFileEngine::syncToDisk()
{
    return QAbstractFileEngine::syncToDisk();
}
qint64 QFSFileEngine::size() const
{
    return QAbstractFileEngine::size();
}
qint64 QFSFileEngine::pos() const
{
    return QAbstractFileEngine::pos();
}
bool QFSFileEngine::seek(qint64 pos)
{
    return QAbstractFileEngine::seek(pos);
}
bool QFSFileEngine::isSequential() const
{
    return QAbstractFileEngine::isSequential();
}
bool QFSFileEngine::remove()
{
    return QAbstractFileEngine::remove();
}
bool QFSFileEngine::copy(const QString &newName)
{
    return QAbstractFileEngine::copy(newName);
}
bool QFSFileEngine::rename_helper(const QString &, RenameMode)
{
    return false;
}
bool QFSFileEngine::link(const QString &newName)
{
    return QAbstractFileEngine::link(newName);
}
bool QFSFileEngine::mkdir(const QString &dirName, bool createParentDirectories,
                          std::optional<QFile::Permissions> permissions) const
{
    return QAbstractFileEngine::mkdir(dirName, createParentDirectories, permissions);
}
bool QFSFileEngine::rmdir(const QString &dirName, bool recurseParentDirectories) const
{
    return QAbstractFileEngine::rmdir(dirName, recurseParentDirectories);
}
bool QFSFileEngine::setSize(qint64 size)
{
    return QAbstractFileEngine::setSize(size);
}
bool QFSFileEngine::caseSensitive() const
{
    return QAbstractFileEngine::caseSensitive();
}
bool QFSFileEngine::isRelativePath() const
{
    return QAbstractFileEngine::isRelativePath();
}
QAbstractFileEngine::FileFlags QFSFileEngine::fileFlags(FileFlags type) const
{
    return QAbstractFileEngine::fileFlags(type);
}
bool QFSFileEngine::setPermissions(uint perms)
{
    return QAbstractFileEngine::setPermissions(perms);
}
QByteArray QFSFileEngine::id() const
{
    return QAbstractFileEngine::id();
}
QString QFSFileEngine::fileName(FileName file) const
{
    return QAbstractFileEngine::fileName(file);
}
uint QFSFileEngine::ownerId(FileOwner owner) const
{
    return QAbstractFileEngine::ownerId(owner);
}
QString QFSFileEngine::owner(FileOwner owner) const
{
    return QAbstractFileEngine::owner(owner);
}
bool QFSFileEngine::setFileTime(const QDateTime &newDate, QFile::FileTime time)
{
    return QAbstractFileEngine::setFileTime(newDate, time);
}
QDateTime QFSFileEngine::fileTime(QFile::FileTime time) const
{
    return QAbstractFileEngine::fileTime(time);
}
void QFSFileEngine::setFileName(const QString &file)
{
    QAbstractFileEngine::setFileName(file);
}
int QFSFileEngine::handle() const
{
    return QAbstractFileEngine::handle();
}
#ifndef QT_NO_FILESYSTEMITERATOR
QAbstractFileEngine::IteratorUniquePtr QFSFileEngine::beginEntryList(
        const QString &path, QDirListing::IteratorFlags filters, const QStringList &filterNames)
{
    return QAbstractFileEngine::beginEntryList(path, filters, filterNames);
}
#endif
qint64 QFSFileEngine::read(char *data, qint64 maxlen)
{
    return QAbstractFileEngine::read(data, maxlen);
}
qint64 QFSFileEngine::readLine(char *data, qint64 maxlen)
{
    return QAbstractFileEngine::readLine(data, maxlen);
}
qint64 QFSFileEngine::write(const char *data, qint64 len)
{
    return QAbstractFileEngine::write(data, len);
}
bool QFSFileEngine::cloneTo(QAbstractFileEngine *target)
{
    return QAbstractFileEngine::cloneTo(target);
}
bool QFSFileEngine::extension(Extension extension, const ExtensionOption *option, ExtensionReturn *output)
{
    return QAbstractFileEngine::extension(extension, option, output);
}
bool QFSFileEngine::supportsExtension(Extension extension) const
{
    return QAbstractFileEngine::supportsExtension(extension);
}

QT_END_NAMESPACE

/**
 * @file lib/qfileengine_koppios.cpp
 * @brief A real QAbstractFileEngine backend for koppi's hobby OS, plugged
 *        into real, unmodified Qt6 source (qfile.cpp/qfiledevice.cpp/
 *        qfileinfo.cpp/qdir.cpp/...) through QAbstractFileEngineHandler,
 *        a real Qt extension point (see qabstractfileengine_p.h) meant
 *        exactly for this: QFileSystemEngine::createLegacyEngine() tries
 *        every registered handler before ever falling through to a real
 *        POSIX-backed QFSFileEngine (qfilesystemengine_unix.cpp, not
 *        vendored in this tree -- this kernel has no open()/read()/
 *        write()/close()/stat() to back it with; see lib/pthread_glibc.c's
 *        file comment for what it has instead).
 *
 * This kernel's real file model (lib/stdio.c/lib/system_calls.c) is not
 * POSIX's: fopen()/fread() read a whole file forward, in fixed 512-byte
 * blocks, with no seek; write_file() (syscall 16) creates/truncates and
 * writes an entire buffer in one call, with no partial/append writes. So
 * this engine is deliberately narrower than QFSFileEngine: sequential
 * read of an existing file, or a single whole-file write on close(), no
 * real seek()/permissions/symlinks/directory iteration. Every method the
 * base class already provides a harmless default for (rename, mkdir,
 * setPermissions, entryList, ...) is left unoverridden rather than faked.
 *
 * open(ReadOnly): fopen()s the path (relative to this kernel's process
 * cwd -- see pwd()/fopen() in lib/stdio.c; this engine does not attempt
 * to resolve or validate paths beyond what fopen() itself does).
 * read(): drains 512-byte blocks into the caller's buffer, capped by
 * this file's real length (FILE::len) and byte offset. size(): FILE::len
 * from the already-open handle, real, not guessed.
 * open(WriteOnly): buffers everything written in memory; close()/flush()
 * commit it in one write_file() call. Not memory-bounded -- fine for the
 * small files (a rendered image, a short log) this is ever likely to
 * write, wrong for anything large; documented rather than silently
 * capped.
 */
#include <QtCore/private/qabstractfileengine_p.h>
#include <QtCore/qbytearray.h>

/* Not include/lib/stdio.h: real Qt headers above transitively pull in the
 * real system <cstdio> (via <string>'s ext/string_conversions.h), so this
 * kernel's own FILE/fopen/fclose/fread -- same bare names, different
 * signatures -- would collide head-on if both were declared in this TU
 * (see lib/pthread_glibc.c's file comment for the general shape of this
 * problem). This file therefore skips the lib/stdio.c wrappers and talks
 * to fopen/fclose/fread's underlying syscalls (6/7/13) directly through
 * syscall3() -- declared below under koppios_ prefixed names -- exactly
 * mirroring what lib/stdio.c's own fopen()/fclose()/fread() do (see
 * syscall.c's table: vfs_file_open_user/vfs_file_close_user/sys_fread),
 * just without the colliding names. */
extern "C" unsigned syscall3(int n, unsigned a, unsigned b, unsigned c);
extern "C" char *pwd();

struct KoppiosFile {
    char name[32];
    unsigned flags;
    unsigned len;
    unsigned eof;
    unsigned dev;
    unsigned current_cluster;
    unsigned type;
};

static KoppiosFile *koppios_fopen(const char *filename, const char *mode) {
    /* lib/stdio.c's fopen() resolves relative to pwd() itself; replicated
     * here rather than reused (see file comment on why lib/stdio.h can't
     * be included directly). */
    char path[256];
    unsigned i = 0;
    if (filename[0] != '/') {
        /* Only relative names get the cwd prefix: the VFS wants
         * device-qualified paths ("/rd/x"), and prefixing an absolute one
         * would produce "//rd/x" whenever the cwd is "/". */
        const char *cwd = pwd();
        for (; cwd[i] && i + 1 < sizeof(path); i++) {
            path[i] = cwd[i];
        }
        if (i > 0 && path[i - 1] != '/' && i + 1 < sizeof(path)) {
            path[i++] = '/';
        }
    }
    for (unsigned j = 0; filename[j] && i + 1 < sizeof(path); j++, i++) {
        path[i] = filename[j];
    }
    path[i] = '\0';
    return (KoppiosFile *) syscall3(6, (unsigned) path, (unsigned) mode, 0);
}
static void koppios_fclose(KoppiosFile *f) {
    syscall3(7, (unsigned) f, 0, 0);
}
/* Always reads the next fixed 512-byte block (buf must be >= 512 bytes);
 * the caller trims it against the real remaining length -- same contract
 * as lib/stdio.c's fread(), see syscall.c's sys_fread(). */
static void koppios_fread_block(KoppiosFile *f, char *buf512) {
    syscall3(13, (unsigned) f, (unsigned) buf512, 0);
}
static int koppios_write_file(const char *path, const char *buf, unsigned len) {
    return (int) syscall3(16, (unsigned) path, (unsigned) buf, len);
}

QT_BEGIN_NAMESPACE

class QKoppiosFileEngine : public QAbstractFileEngine
{
public:
    explicit QKoppiosFileEngine(const QString &fileName) { setFileName(fileName); }
    ~QKoppiosFileEngine() override { close(); }

    void setFileName(const QString &file) override {
        close();
        m_fileName = file;
    }
    QString fileName(FileName file = DefaultName) const override {
        Q_UNUSED(file);
        return m_fileName;
    }

    bool open(QIODevice::OpenMode openMode, std::optional<QFile::Permissions> permissions = std::nullopt) override {
        Q_UNUSED(permissions);
        close();
        m_writing = openMode.testFlag(QIODevice::WriteOnly);
        if (m_writing) {
            m_writeBuffer.clear();
            return true;
        }

        QByteArray path = m_fileName.toLocal8Bit();
        path.append('\0');
        m_handle = koppios_fopen(path.constData(), "r");
        if (!m_handle) {
            return false;
        }
        m_readPos = 0;
        return true;
    }

    bool close() override {
        bool ok = true;
        if (m_writing && m_handle == nullptr && !m_writeBuffer.isEmpty()) {
            /* Nothing to flush -- writes are only ever committed by
             * flush()/the destructor below, never buffered-then-dropped. */
        }
        if (m_writing) {
            ok = flush();
        }
        if (m_handle) {
            koppios_fclose(m_handle);
            m_handle = nullptr;
        }
        m_writing = false;
        return ok;
    }

    bool flush() override {
        if (!m_writing) {
            return true;
        }
        QByteArray path = m_fileName.toLocal8Bit();
        path.append('\0');
        return koppios_write_file(path.constData(), m_writeBuffer.constData(), (unsigned) m_writeBuffer.size()) == 0;
    }

    qint64 size() const override {
        return m_handle ? (qint64) m_handle->len : (qint64) m_writeBuffer.size();
    }

    qint64 pos() const override {
        return m_writing ? (qint64) m_writeBuffer.size() : m_readPos;
    }

    bool isSequential() const override {
        /* Real, not a cop-out: this kernel's fread() has no seek, so a
         * QKoppiosFileEngine-backed QFile genuinely cannot seek backward. */
        return true;
    }

    qint64 read(char *data, qint64 maxlen) override {
        if (!m_handle || maxlen <= 0) {
            return m_handle ? 0 : -1;
        }
        qint64 total = (qint64) m_handle->len;
        qint64 want = total - m_readPos;
        if (want > maxlen) {
            want = maxlen;
        }
        if (want <= 0) {
            return 0;
        }
        qint64 produced = 0;
        while (produced < want && !m_handle->eof) {
            char block[512];
            koppios_fread_block(m_handle, block);
            qint64 n = want - produced;
            if (n > 512) {
                n = 512;
            }
            memcpy(data + produced, block, (size_t) n);
            produced += n;
            m_readPos += n;
        }
        return produced;
    }

    qint64 write(const char *data, qint64 len) override {
        if (!m_writing || len < 0) {
            return -1;
        }
        m_writeBuffer.append(data, (qsizetype) len);
        return len;
    }

    FileFlags fileFlags(FileFlags type = FileInfoAll) const override {
        FileFlags flags;
        QByteArray path = m_fileName.toLocal8Bit();
        path.append('\0');
        KoppiosFile *probe = koppios_fopen(path.constData(), "r");
        if (probe) {
            flags |= ExistsFlag | ReadOwnerPerm | ReadUserPerm | ReadGroupPerm | ReadOtherPerm | FileType;
            koppios_fclose(probe);
        }
        return flags & type;
    }

private:
    QString m_fileName;
    KoppiosFile *m_handle = nullptr;
    qint64 m_readPos = 0;
    bool m_writing = false;
    QByteArray m_writeBuffer;
};

class QKoppiosFileEngineHandler : public QAbstractFileEngineHandler
{
public:
    std::unique_ptr<QAbstractFileEngine> create(const QString &fileName) const override {
        return std::make_unique<QKoppiosFileEngine>(fileName);
    }
};

Q_GLOBAL_STATIC(QKoppiosFileEngineHandler, qt_koppiosFileEngineHandler)

void qt_koppios_install_file_engine_handler() {
    (void) qt_koppiosFileEngineHandler();
}

QT_END_NAMESPACE

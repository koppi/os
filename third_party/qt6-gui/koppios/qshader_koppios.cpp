/* koppios addition, not upstream Qt: the five QShader members that
 * QBackingStoreDefaultCompositor / QRhi reference, taken verbatim from
 * src/gui/rhi/qshader.cpp except fromSerialized(). Real qshader.cpp is not
 * compiled: it calls qCompress()/qUncompress(), and this port has no zlib
 * (QT_NO_COMPRESS). A serialized .qsb is zlib-compressed, so it can never be
 * decoded here; fromSerialized() therefore returns an invalid (null)
 * QShader, which is what real Qt returns for any data it cannot decode.
 * Nothing on the raster QMinimalBackingStore path ever builds a shader. */
#include <rhi/qshader_p.h>

QT_BEGIN_NAMESPACE

QShader::QShader()
    : d(nullptr)
{
}

QShader::QShader(const QShader &other)
    : d(other.d)
{
    if (d)
        d->ref.ref();
}

QShader &QShader::operator=(const QShader &other)
{
    if (d) {
        if (other.d) {
            qAtomicAssign(d, other.d);
        } else {
            if (!d->ref.deref())
                delete d;
            d = nullptr;
        }
    } else if (other.d) {
        other.d->ref.ref();
        d = other.d;
    }
    return *this;
}

QShader::~QShader()
{
    if (d && !d->ref.deref())
        delete d;
}

QShader QShader::fromSerialized(const QByteArray &data)
{
    Q_UNUSED(data);
    return QShader();
}

QT_END_NAMESPACE

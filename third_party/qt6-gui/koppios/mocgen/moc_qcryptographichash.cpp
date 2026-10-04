/****************************************************************************
** Meta object code from reading C++ file 'qcryptographichash.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/corelib/tools/qcryptographichash.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qcryptographichash.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN18QCryptographicHashE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN18QCryptographicHashE = QtMocHelpers::stringData(
    "QCryptographicHash",
    "Algorithm",
    "Md4",
    "Md5",
    "Sha1",
    "Sha224",
    "Sha256",
    "Sha384",
    "Sha512",
    "Keccak_224",
    "Keccak_256",
    "Keccak_384",
    "Keccak_512",
    "RealSha3_224",
    "RealSha3_256",
    "RealSha3_384",
    "RealSha3_512",
    "Sha3_224",
    "Sha3_256",
    "Sha3_384",
    "Sha3_512",
    "Blake2b_160",
    "Blake2b_256",
    "Blake2b_384",
    "Blake2b_512",
    "Blake2s_128",
    "Blake2s_160",
    "Blake2s_224",
    "Blake2s_256",
    "NumAlgorithms"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN18QCryptographicHashE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       1,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,   28,   19,

 // enum data: key, value
       2, uint(QCryptographicHash::Md4),
       3, uint(QCryptographicHash::Md5),
       4, uint(QCryptographicHash::Sha1),
       5, uint(QCryptographicHash::Sha224),
       6, uint(QCryptographicHash::Sha256),
       7, uint(QCryptographicHash::Sha384),
       8, uint(QCryptographicHash::Sha512),
       9, uint(QCryptographicHash::Keccak_224),
      10, uint(QCryptographicHash::Keccak_256),
      11, uint(QCryptographicHash::Keccak_384),
      12, uint(QCryptographicHash::Keccak_512),
      13, uint(QCryptographicHash::RealSha3_224),
      14, uint(QCryptographicHash::RealSha3_256),
      15, uint(QCryptographicHash::RealSha3_384),
      16, uint(QCryptographicHash::RealSha3_512),
      17, uint(QCryptographicHash::Sha3_224),
      18, uint(QCryptographicHash::Sha3_256),
      19, uint(QCryptographicHash::Sha3_384),
      20, uint(QCryptographicHash::Sha3_512),
      21, uint(QCryptographicHash::Blake2b_160),
      22, uint(QCryptographicHash::Blake2b_256),
      23, uint(QCryptographicHash::Blake2b_384),
      24, uint(QCryptographicHash::Blake2b_512),
      25, uint(QCryptographicHash::Blake2s_128),
      26, uint(QCryptographicHash::Blake2s_160),
      27, uint(QCryptographicHash::Blake2s_224),
      28, uint(QCryptographicHash::Blake2s_256),
      29, uint(QCryptographicHash::NumAlgorithms),

       0        // eod
};

Q_CONSTINIT const QMetaObject QCryptographicHash::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN18QCryptographicHashE.offsetsAndSizes,
    qt_meta_data_ZN18QCryptographicHashE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN18QCryptographicHashE_t,
        // enum 'Algorithm'
        QtPrivate::TypeAndForceComplete<QCryptographicHash::Algorithm, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QCryptographicHash, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP

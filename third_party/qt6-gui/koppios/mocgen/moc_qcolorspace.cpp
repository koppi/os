/****************************************************************************
** Meta object code from reading C++ file 'qcolorspace.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/painting/qcolorspace.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qcolorspace.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11QColorSpaceE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QColorSpaceE = QtMocHelpers::stringData(
    "QColorSpace",
    "NamedColorSpace",
    "SRgb",
    "SRgbLinear",
    "AdobeRgb",
    "DisplayP3",
    "ProPhotoRgb",
    "Bt2020",
    "Bt2100Pq",
    "Bt2100Hlg",
    "Primaries",
    "Custom",
    "DciP3D65",
    "TransferFunction",
    "Linear",
    "Gamma",
    "St2084",
    "Hlg",
    "TransformModel",
    "ThreeComponentMatrix",
    "ElementListProcessing",
    "ColorModel",
    "Undefined",
    "Rgb",
    "Gray",
    "Cmyk"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QColorSpaceE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       5,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,    8,   39,
      10,   10, 0x2,    6,   55,
      13,   13, 0x2,    8,   67,
      18,   18, 0x2,    2,   83,
      21,   21, 0x2,    4,   87,

 // enum data: key, value
       2, uint(QColorSpace::SRgb),
       3, uint(QColorSpace::SRgbLinear),
       4, uint(QColorSpace::AdobeRgb),
       5, uint(QColorSpace::DisplayP3),
       6, uint(QColorSpace::ProPhotoRgb),
       7, uint(QColorSpace::Bt2020),
       8, uint(QColorSpace::Bt2100Pq),
       9, uint(QColorSpace::Bt2100Hlg),
      11, uint(QColorSpace::Primaries::Custom),
       2, uint(QColorSpace::Primaries::SRgb),
       4, uint(QColorSpace::Primaries::AdobeRgb),
      12, uint(QColorSpace::Primaries::DciP3D65),
       6, uint(QColorSpace::Primaries::ProPhotoRgb),
       7, uint(QColorSpace::Primaries::Bt2020),
      11, uint(QColorSpace::TransferFunction::Custom),
      14, uint(QColorSpace::TransferFunction::Linear),
      15, uint(QColorSpace::TransferFunction::Gamma),
       2, uint(QColorSpace::TransferFunction::SRgb),
       6, uint(QColorSpace::TransferFunction::ProPhotoRgb),
       7, uint(QColorSpace::TransferFunction::Bt2020),
      16, uint(QColorSpace::TransferFunction::St2084),
      17, uint(QColorSpace::TransferFunction::Hlg),
      19, uint(QColorSpace::TransformModel::ThreeComponentMatrix),
      20, uint(QColorSpace::TransformModel::ElementListProcessing),
      22, uint(QColorSpace::ColorModel::Undefined),
      23, uint(QColorSpace::ColorModel::Rgb),
      24, uint(QColorSpace::ColorModel::Gray),
      25, uint(QColorSpace::ColorModel::Cmyk),

       0        // eod
};

Q_CONSTINIT const QMetaObject QColorSpace::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN11QColorSpaceE.offsetsAndSizes,
    qt_meta_data_ZN11QColorSpaceE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QColorSpaceE_t,
        // enum 'NamedColorSpace'
        QtPrivate::TypeAndForceComplete<QColorSpace::NamedColorSpace, std::true_type>,
        // enum 'Primaries'
        QtPrivate::TypeAndForceComplete<QColorSpace::Primaries, std::true_type>,
        // enum 'TransferFunction'
        QtPrivate::TypeAndForceComplete<QColorSpace::TransferFunction, std::true_type>,
        // enum 'TransformModel'
        QtPrivate::TypeAndForceComplete<QColorSpace::TransformModel, std::true_type>,
        // enum 'ColorModel'
        QtPrivate::TypeAndForceComplete<QColorSpace::ColorModel, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QColorSpace, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP

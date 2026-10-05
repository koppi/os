/****************************************************************************
** Meta object code from reading C++ file 'qformlayout.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qformlayout.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qformlayout.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11QFormLayoutE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN11QFormLayoutE = QtMocHelpers::stringData(
    "QFormLayout",
    "fieldGrowthPolicy",
    "FieldGrowthPolicy",
    "rowWrapPolicy",
    "RowWrapPolicy",
    "labelAlignment",
    "Qt::Alignment",
    "formAlignment",
    "horizontalSpacing",
    "verticalSpacing",
    "FieldsStayAtSizeHint",
    "ExpandingFieldsGrow",
    "AllNonFixedFieldsGrow",
    "DontWrapRows",
    "WrapLongRows",
    "WrapAllRows",
    "ItemRole",
    "LabelRole",
    "FieldRole",
    "SpanningRole"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN11QFormLayoutE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       6,   14, // properties
       3,   44, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x0001510f, uint(-1), 0,
       3, 0x80000000 | 4, 0x0001510f, uint(-1), 0,
       5, 0x80000000 | 6, 0x0001510f, uint(-1), 0,
       7, 0x80000000 | 6, 0x0001510f, uint(-1), 0,
       8, QMetaType::Int, 0x00015103, uint(-1), 0,
       9, QMetaType::Int, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       2,    2, 0x0,    3,   59,
       4,    4, 0x0,    3,   65,
      16,   16, 0x0,    3,   71,

 // enum data: key, value
      10, uint(QFormLayout::FieldsStayAtSizeHint),
      11, uint(QFormLayout::ExpandingFieldsGrow),
      12, uint(QFormLayout::AllNonFixedFieldsGrow),
      13, uint(QFormLayout::DontWrapRows),
      14, uint(QFormLayout::WrapLongRows),
      15, uint(QFormLayout::WrapAllRows),
      17, uint(QFormLayout::LabelRole),
      18, uint(QFormLayout::FieldRole),
      19, uint(QFormLayout::SpanningRole),

       0        // eod
};

Q_CONSTINIT const QMetaObject QFormLayout::staticMetaObject = { {
    QMetaObject::SuperData::link<QLayout::staticMetaObject>(),
    qt_meta_stringdata_ZN11QFormLayoutE.offsetsAndSizes,
    qt_meta_data_ZN11QFormLayoutE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN11QFormLayoutE_t,
        // property 'fieldGrowthPolicy'
        QtPrivate::TypeAndForceComplete<FieldGrowthPolicy, std::true_type>,
        // property 'rowWrapPolicy'
        QtPrivate::TypeAndForceComplete<RowWrapPolicy, std::true_type>,
        // property 'labelAlignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'formAlignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'horizontalSpacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'verticalSpacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'FieldGrowthPolicy'
        QtPrivate::TypeAndForceComplete<QFormLayout::FieldGrowthPolicy, std::true_type>,
        // enum 'RowWrapPolicy'
        QtPrivate::TypeAndForceComplete<QFormLayout::RowWrapPolicy, std::true_type>,
        // enum 'ItemRole'
        QtPrivate::TypeAndForceComplete<QFormLayout::ItemRole, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QFormLayout, std::true_type>
    >,
    nullptr
} };

void QFormLayout::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QFormLayout *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< FieldGrowthPolicy*>(_v) = _t->fieldGrowthPolicy(); break;
        case 1: *reinterpret_cast< RowWrapPolicy*>(_v) = _t->rowWrapPolicy(); break;
        case 2: *reinterpret_cast< Qt::Alignment*>(_v) = _t->labelAlignment(); break;
        case 3: *reinterpret_cast< Qt::Alignment*>(_v) = _t->formAlignment(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->horizontalSpacing(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->verticalSpacing(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setFieldGrowthPolicy(*reinterpret_cast< FieldGrowthPolicy*>(_v)); break;
        case 1: _t->setRowWrapPolicy(*reinterpret_cast< RowWrapPolicy*>(_v)); break;
        case 2: _t->setLabelAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 3: _t->setFormAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 4: _t->setHorizontalSpacing(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setVerticalSpacing(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 0: _t->resetFieldGrowthPolicy(); break;
        case 1: _t->resetRowWrapPolicy(); break;
        case 2: _t->resetLabelAlignment(); break;
        case 3: _t->resetFormAlignment(); break;
        default: break;
        }
    }
}

const QMetaObject *QFormLayout::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QFormLayout::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN11QFormLayoutE.stringdata0))
        return static_cast<void*>(this);
    return QLayout::qt_metacast(_clname);
}

int QFormLayout::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QLayout::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP

/****************************************************************************
** Meta object code from reading C++ file 'qabstractspinbox.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qabstractspinbox.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qabstractspinbox.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16QAbstractSpinBoxE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN16QAbstractSpinBoxE = QtMocHelpers::stringData(
    "QAbstractSpinBox",
    "editingFinished",
    "",
    "stepUp",
    "stepDown",
    "selectAll",
    "clear",
    "wrapping",
    "frame",
    "alignment",
    "Qt::Alignment",
    "readOnly",
    "buttonSymbols",
    "ButtonSymbols",
    "specialValueText",
    "text",
    "accelerated",
    "correctionMode",
    "CorrectionMode",
    "acceptableInput",
    "keyboardTracking",
    "showGroupSeparator",
    "UpDownArrows",
    "PlusMinus",
    "NoButtons",
    "CorrectToPreviousValue",
    "CorrectToNearestValue",
    "StepType",
    "DefaultStepType",
    "AdaptiveDecimalStepType"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN16QAbstractSpinBoxE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
      12,   49, // properties
       3,  109, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   44,    2, 0x06,   16 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       3,    0,   45,    2, 0x0a,   17 /* Public */,
       4,    0,   46,    2, 0x0a,   18 /* Public */,
       5,    0,   47,    2, 0x0a,   19 /* Public */,
       6,    0,   48,    2, 0x0a,   20 /* Public */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
       7, QMetaType::Bool, 0x00015103, uint(-1), 0,
       8, QMetaType::Bool, 0x00015103, uint(-1), 0,
       9, 0x80000000 | 10, 0x0001510b, uint(-1), 0,
      11, QMetaType::Bool, 0x00015103, uint(-1), 0,
      12, 0x80000000 | 13, 0x0001510b, uint(-1), 0,
      14, QMetaType::QString, 0x00015103, uint(-1), 0,
      15, QMetaType::QString, 0x00015001, uint(-1), 0,
      16, QMetaType::Bool, 0x00015103, uint(-1), 0,
      17, 0x80000000 | 18, 0x0001510b, uint(-1), 0,
      19, QMetaType::Bool, 0x00015001, uint(-1), 0,
      20, QMetaType::Bool, 0x00015103, uint(-1), 0,
      21, QMetaType::Bool, 0x00015003, uint(-1), 0,

 // enums: name, alias, flags, count, data
      13,   13, 0x0,    3,  124,
      18,   18, 0x0,    2,  130,
      27,   27, 0x0,    2,  134,

 // enum data: key, value
      22, uint(QAbstractSpinBox::UpDownArrows),
      23, uint(QAbstractSpinBox::PlusMinus),
      24, uint(QAbstractSpinBox::NoButtons),
      25, uint(QAbstractSpinBox::CorrectToPreviousValue),
      26, uint(QAbstractSpinBox::CorrectToNearestValue),
      28, uint(QAbstractSpinBox::DefaultStepType),
      29, uint(QAbstractSpinBox::AdaptiveDecimalStepType),

       0        // eod
};

Q_CONSTINIT const QMetaObject QAbstractSpinBox::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN16QAbstractSpinBoxE.offsetsAndSizes,
    qt_meta_data_ZN16QAbstractSpinBoxE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN16QAbstractSpinBoxE_t,
        // property 'wrapping'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'frame'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'alignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'readOnly'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'buttonSymbols'
        QtPrivate::TypeAndForceComplete<ButtonSymbols, std::true_type>,
        // property 'specialValueText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'text'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'accelerated'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'correctionMode'
        QtPrivate::TypeAndForceComplete<CorrectionMode, std::true_type>,
        // property 'acceptableInput'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'keyboardTracking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'showGroupSeparator'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'ButtonSymbols'
        QtPrivate::TypeAndForceComplete<QAbstractSpinBox::ButtonSymbols, std::true_type>,
        // enum 'CorrectionMode'
        QtPrivate::TypeAndForceComplete<QAbstractSpinBox::CorrectionMode, std::true_type>,
        // enum 'StepType'
        QtPrivate::TypeAndForceComplete<QAbstractSpinBox::StepType, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QAbstractSpinBox, std::true_type>,
        // method 'editingFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stepUp'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stepDown'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectAll'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clear'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QAbstractSpinBox::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QAbstractSpinBox *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->editingFinished(); break;
        case 1: _t->stepUp(); break;
        case 2: _t->stepDown(); break;
        case 3: _t->selectAll(); break;
        case 4: _t->clear(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QAbstractSpinBox::*)();
            if (_q_method_type _q_method = &QAbstractSpinBox::editingFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->wrapping(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->hasFrame(); break;
        case 2: *reinterpret_cast< Qt::Alignment*>(_v) = _t->alignment(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->isReadOnly(); break;
        case 4: *reinterpret_cast< ButtonSymbols*>(_v) = _t->buttonSymbols(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->specialValueText(); break;
        case 6: *reinterpret_cast< QString*>(_v) = _t->text(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isAccelerated(); break;
        case 8: *reinterpret_cast< CorrectionMode*>(_v) = _t->correctionMode(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->hasAcceptableInput(); break;
        case 10: *reinterpret_cast< bool*>(_v) = _t->keyboardTracking(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->isGroupSeparatorShown(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setWrapping(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setFrame(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 3: _t->setReadOnly(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setButtonSymbols(*reinterpret_cast< ButtonSymbols*>(_v)); break;
        case 5: _t->setSpecialValueText(*reinterpret_cast< QString*>(_v)); break;
        case 7: _t->setAccelerated(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setCorrectionMode(*reinterpret_cast< CorrectionMode*>(_v)); break;
        case 10: _t->setKeyboardTracking(*reinterpret_cast< bool*>(_v)); break;
        case 11: _t->setGroupSeparatorShown(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QAbstractSpinBox::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QAbstractSpinBox::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN16QAbstractSpinBoxE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QAbstractSpinBox::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void QAbstractSpinBox::editingFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP

/****************************************************************************
** Meta object code from reading C++ file 'qcombobox.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qcombobox.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qcombobox.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QComboBoxE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QComboBoxE = QtMocHelpers::stringData(
    "QComboBox",
    "editTextChanged",
    "",
    "activated",
    "index",
    "textActivated",
    "highlighted",
    "textHighlighted",
    "currentIndexChanged",
    "currentTextChanged",
    "clear",
    "clearEditText",
    "setEditText",
    "text",
    "setCurrentIndex",
    "setCurrentText",
    "inputMethodQuery",
    "QVariant",
    "Qt::InputMethodQuery",
    "query",
    "argument",
    "editable",
    "count",
    "currentText",
    "currentIndex",
    "currentData",
    "maxVisibleItems",
    "maxCount",
    "insertPolicy",
    "InsertPolicy",
    "sizeAdjustPolicy",
    "SizeAdjustPolicy",
    "minimumContentsLength",
    "iconSize",
    "placeholderText",
    "duplicatesEnabled",
    "frame",
    "modelColumn",
    "NoInsert",
    "InsertAtTop",
    "InsertAtCurrent",
    "InsertAtBottom",
    "InsertAfterCurrent",
    "InsertBeforeCurrent",
    "InsertAlphabetically",
    "AdjustToContents",
    "AdjustToContentsOnFirstShow",
    "AdjustToMinimumContentsLengthWithIcon"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QComboBoxE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
      15,  129, // properties
       2,  204, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   92,    2, 0x06,   18 /* Public */,
       3,    1,   95,    2, 0x06,   20 /* Public */,
       5,    1,   98,    2, 0x06,   22 /* Public */,
       6,    1,  101,    2, 0x06,   24 /* Public */,
       7,    1,  104,    2, 0x06,   26 /* Public */,
       8,    1,  107,    2, 0x06,   28 /* Public */,
       9,    1,  110,    2, 0x06,   30 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    0,  113,    2, 0x0a,   32 /* Public */,
      11,    0,  114,    2, 0x0a,   33 /* Public */,
      12,    1,  115,    2, 0x0a,   34 /* Public */,
      14,    1,  118,    2, 0x0a,   36 /* Public */,
      15,    1,  121,    2, 0x0a,   38 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      16,    2,  124,    2, 0x102,   40 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::QString,    2,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::QString,   13,

 // methods: parameters
    0x80000000 | 17, 0x80000000 | 18, 0x80000000 | 17,   19,   20,

 // properties: name, type, flags, notifyId, revision
      21, QMetaType::Bool, 0x00015103, uint(-1), 0,
      22, QMetaType::Int, 0x00015001, uint(-1), 0,
      23, QMetaType::QString, 0x00115103, uint(6), 0,
      24, QMetaType::Int, 0x00015103, uint(5), 0,
      25, 0x80000000 | 17, 0x00015009, uint(-1), 0,
      26, QMetaType::Int, 0x00015103, uint(-1), 0,
      27, QMetaType::Int, 0x00015103, uint(-1), 0,
      28, 0x80000000 | 29, 0x0001510b, uint(-1), 0,
      30, 0x80000000 | 31, 0x0001510b, uint(-1), 0,
      32, QMetaType::Int, 0x00015103, uint(-1), 0,
      33, QMetaType::QSize, 0x00015103, uint(-1), 0,
      34, QMetaType::QString, 0x00015103, uint(-1), 0,
      35, QMetaType::Bool, 0x00015103, uint(-1), 0,
      36, QMetaType::Bool, 0x00015103, uint(-1), 0,
      37, QMetaType::Int, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
      29,   29, 0x0,    7,  214,
      31,   31, 0x0,    3,  228,

 // enum data: key, value
      38, uint(QComboBox::NoInsert),
      39, uint(QComboBox::InsertAtTop),
      40, uint(QComboBox::InsertAtCurrent),
      41, uint(QComboBox::InsertAtBottom),
      42, uint(QComboBox::InsertAfterCurrent),
      43, uint(QComboBox::InsertBeforeCurrent),
      44, uint(QComboBox::InsertAlphabetically),
      45, uint(QComboBox::AdjustToContents),
      46, uint(QComboBox::AdjustToContentsOnFirstShow),
      47, uint(QComboBox::AdjustToMinimumContentsLengthWithIcon),

       0        // eod
};

Q_CONSTINIT const QMetaObject QComboBox::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN9QComboBoxE.offsetsAndSizes,
    qt_meta_data_ZN9QComboBoxE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QComboBoxE_t,
        // property 'editable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'count'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'currentText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'currentIndex'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'currentData'
        QtPrivate::TypeAndForceComplete<QVariant, std::true_type>,
        // property 'maxVisibleItems'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maxCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'insertPolicy'
        QtPrivate::TypeAndForceComplete<InsertPolicy, std::true_type>,
        // property 'sizeAdjustPolicy'
        QtPrivate::TypeAndForceComplete<SizeAdjustPolicy, std::true_type>,
        // property 'minimumContentsLength'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'iconSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'placeholderText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'duplicatesEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'frame'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'modelColumn'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'InsertPolicy'
        QtPrivate::TypeAndForceComplete<QComboBox::InsertPolicy, std::true_type>,
        // enum 'SizeAdjustPolicy'
        QtPrivate::TypeAndForceComplete<QComboBox::SizeAdjustPolicy, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QComboBox, std::true_type>,
        // method 'editTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'activated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'textActivated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'highlighted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'textHighlighted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'currentIndexChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'currentTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'clear'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clearEditText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setEditText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setCurrentIndex'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setCurrentText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'inputMethodQuery'
        QtPrivate::TypeAndForceComplete<QVariant, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::InputMethodQuery, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVariant &, std::false_type>
    >,
    nullptr
} };

void QComboBox::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QComboBox *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->editTextChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->activated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->textActivated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->highlighted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->textHighlighted((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->currentTextChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->clear(); break;
        case 8: _t->clearEditText(); break;
        case 9: _t->setEditText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->setCurrentIndex((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->setCurrentText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: { QVariant _r = _t->inputMethodQuery((*reinterpret_cast< std::add_pointer_t<Qt::InputMethodQuery>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariant*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QComboBox::*)(const QString & );
            if (_q_method_type _q_method = &QComboBox::editTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(int );
            if (_q_method_type _q_method = &QComboBox::activated; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(const QString & );
            if (_q_method_type _q_method = &QComboBox::textActivated; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(int );
            if (_q_method_type _q_method = &QComboBox::highlighted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(const QString & );
            if (_q_method_type _q_method = &QComboBox::textHighlighted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(int );
            if (_q_method_type _q_method = &QComboBox::currentIndexChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QComboBox::*)(const QString & );
            if (_q_method_type _q_method = &QComboBox::currentTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isEditable(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->count(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->currentText(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->currentIndex(); break;
        case 4: *reinterpret_cast< QVariant*>(_v) = _t->currentData(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->maxVisibleItems(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->maxCount(); break;
        case 7: *reinterpret_cast< InsertPolicy*>(_v) = _t->insertPolicy(); break;
        case 8: *reinterpret_cast< SizeAdjustPolicy*>(_v) = _t->sizeAdjustPolicy(); break;
        case 9: *reinterpret_cast< int*>(_v) = _t->minimumContentsLength(); break;
        case 10: *reinterpret_cast< QSize*>(_v) = _t->iconSize(); break;
        case 11: *reinterpret_cast< QString*>(_v) = _t->placeholderText(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->duplicatesEnabled(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->hasFrame(); break;
        case 14: *reinterpret_cast< int*>(_v) = _t->modelColumn(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setEditable(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setCurrentText(*reinterpret_cast< QString*>(_v)); break;
        case 3: _t->setCurrentIndex(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setMaxVisibleItems(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setMaxCount(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setInsertPolicy(*reinterpret_cast< InsertPolicy*>(_v)); break;
        case 8: _t->setSizeAdjustPolicy(*reinterpret_cast< SizeAdjustPolicy*>(_v)); break;
        case 9: _t->setMinimumContentsLength(*reinterpret_cast< int*>(_v)); break;
        case 10: _t->setIconSize(*reinterpret_cast< QSize*>(_v)); break;
        case 11: _t->setPlaceholderText(*reinterpret_cast< QString*>(_v)); break;
        case 12: _t->setDuplicatesEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 13: _t->setFrame(*reinterpret_cast< bool*>(_v)); break;
        case 14: _t->setModelColumn(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QComboBox::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QComboBox::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QComboBoxE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QComboBox::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void QComboBox::editTextChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QComboBox::activated(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QComboBox::textActivated(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QComboBox::highlighted(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QComboBox::textHighlighted(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QComboBox::currentIndexChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QComboBox::currentTextChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
QT_WARNING_POP

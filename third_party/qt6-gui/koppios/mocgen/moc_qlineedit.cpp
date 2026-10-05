/****************************************************************************
** Meta object code from reading C++ file 'qlineedit.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qlineedit.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qlineedit.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QLineEditE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QLineEditE = QtMocHelpers::stringData(
    "QLineEdit",
    "textChanged",
    "",
    "textEdited",
    "cursorPositionChanged",
    "returnPressed",
    "editingFinished",
    "selectionChanged",
    "inputRejected",
    "setText",
    "clear",
    "selectAll",
    "undo",
    "redo",
    "inputMethodQuery",
    "QVariant",
    "Qt::InputMethodQuery",
    "property",
    "argument",
    "inputMask",
    "text",
    "maxLength",
    "frame",
    "echoMode",
    "EchoMode",
    "displayText",
    "cursorPosition",
    "alignment",
    "Qt::Alignment",
    "modified",
    "hasSelectedText",
    "selectedText",
    "dragEnabled",
    "readOnly",
    "undoAvailable",
    "redoAvailable",
    "acceptableInput",
    "placeholderText",
    "cursorMoveStyle",
    "Qt::CursorMoveStyle",
    "clearButtonEnabled",
    "ActionPosition",
    "LeadingPosition",
    "TrailingPosition",
    "Normal",
    "NoEcho",
    "Password",
    "PasswordEchoOnEdit"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QLineEditE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
      19,  119, // properties
       2,  214, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   92,    2, 0x06,   22 /* Public */,
       3,    1,   95,    2, 0x06,   24 /* Public */,
       4,    2,   98,    2, 0x06,   26 /* Public */,
       5,    0,  103,    2, 0x06,   29 /* Public */,
       6,    0,  104,    2, 0x06,   30 /* Public */,
       7,    0,  105,    2, 0x06,   31 /* Public */,
       8,    0,  106,    2, 0x06,   32 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       9,    1,  107,    2, 0x0a,   33 /* Public */,
      10,    0,  110,    2, 0x0a,   35 /* Public */,
      11,    0,  111,    2, 0x0a,   36 /* Public */,
      12,    0,  112,    2, 0x0a,   37 /* Public */,
      13,    0,  113,    2, 0x0a,   38 /* Public */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      14,    2,  114,    2, 0x102,   39 /* Public | MethodIsConst  */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    0x80000000 | 15, 0x80000000 | 16, 0x80000000 | 15,   17,   18,

 // properties: name, type, flags, notifyId, revision
      19, QMetaType::QString, 0x00015103, uint(-1), 0,
      20, QMetaType::QString, 0x00115103, uint(0), 0,
      21, QMetaType::Int, 0x00015103, uint(-1), 0,
      22, QMetaType::Bool, 0x00015103, uint(-1), 0,
      23, 0x80000000 | 24, 0x0001510b, uint(-1), 0,
      25, QMetaType::QString, 0x00015001, uint(-1), 0,
      26, QMetaType::Int, 0x00015103, uint(-1), 0,
      27, 0x80000000 | 28, 0x0001510b, uint(-1), 0,
      29, QMetaType::Bool, 0x00014103, uint(-1), 0,
      30, QMetaType::Bool, 0x00015001, uint(-1), 0,
      31, QMetaType::QString, 0x00015001, uint(-1), 0,
      32, QMetaType::Bool, 0x00015103, uint(-1), 0,
      33, QMetaType::Bool, 0x00015103, uint(-1), 0,
      34, QMetaType::Bool, 0x00015001, uint(-1), 0,
      35, QMetaType::Bool, 0x00015001, uint(-1), 0,
      36, QMetaType::Bool, 0x00015001, uint(-1), 0,
      37, QMetaType::QString, 0x00015103, uint(-1), 0,
      38, 0x80000000 | 39, 0x0001510b, uint(-1), 0,
      40, QMetaType::Bool, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
      41,   41, 0x0,    2,  224,
      24,   24, 0x0,    4,  228,

 // enum data: key, value
      42, uint(QLineEdit::LeadingPosition),
      43, uint(QLineEdit::TrailingPosition),
      44, uint(QLineEdit::Normal),
      45, uint(QLineEdit::NoEcho),
      46, uint(QLineEdit::Password),
      47, uint(QLineEdit::PasswordEchoOnEdit),

       0        // eod
};

Q_CONSTINIT const QMetaObject QLineEdit::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN9QLineEditE.offsetsAndSizes,
    qt_meta_data_ZN9QLineEditE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QLineEditE_t,
        // property 'inputMask'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'text'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'maxLength'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'frame'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'echoMode'
        QtPrivate::TypeAndForceComplete<EchoMode, std::true_type>,
        // property 'displayText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'cursorPosition'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'alignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'modified'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'hasSelectedText'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'selectedText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'dragEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'readOnly'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'undoAvailable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'redoAvailable'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'acceptableInput'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'placeholderText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'cursorMoveStyle'
        QtPrivate::TypeAndForceComplete<Qt::CursorMoveStyle, std::true_type>,
        // property 'clearButtonEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // enum 'ActionPosition'
        QtPrivate::TypeAndForceComplete<QLineEdit::ActionPosition, std::true_type>,
        // enum 'EchoMode'
        QtPrivate::TypeAndForceComplete<QLineEdit::EchoMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QLineEdit, std::true_type>,
        // method 'textChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'textEdited'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cursorPositionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'returnPressed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'editingFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'inputRejected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'clear'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectAll'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'undo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'redo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'inputMethodQuery'
        QtPrivate::TypeAndForceComplete<QVariant, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::InputMethodQuery, std::false_type>,
        QtPrivate::TypeAndForceComplete<QVariant, std::false_type>
    >,
    nullptr
} };

void QLineEdit::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QLineEdit *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->textChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->textEdited((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->cursorPositionChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->returnPressed(); break;
        case 4: _t->editingFinished(); break;
        case 5: _t->selectionChanged(); break;
        case 6: _t->inputRejected(); break;
        case 7: _t->setText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->clear(); break;
        case 9: _t->selectAll(); break;
        case 10: _t->undo(); break;
        case 11: _t->redo(); break;
        case 12: { QVariant _r = _t->inputMethodQuery((*reinterpret_cast< std::add_pointer_t<Qt::InputMethodQuery>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QVariant>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariant*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QLineEdit::*)(const QString & );
            if (_q_method_type _q_method = &QLineEdit::textChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)(const QString & );
            if (_q_method_type _q_method = &QLineEdit::textEdited; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)(int , int );
            if (_q_method_type _q_method = &QLineEdit::cursorPositionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)();
            if (_q_method_type _q_method = &QLineEdit::returnPressed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)();
            if (_q_method_type _q_method = &QLineEdit::editingFinished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)();
            if (_q_method_type _q_method = &QLineEdit::selectionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QLineEdit::*)();
            if (_q_method_type _q_method = &QLineEdit::inputRejected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->inputMask(); break;
        case 1: *reinterpret_cast< QString*>(_v) = _t->text(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->maxLength(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->hasFrame(); break;
        case 4: *reinterpret_cast< EchoMode*>(_v) = _t->echoMode(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->displayText(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->cursorPosition(); break;
        case 7: *reinterpret_cast< Qt::Alignment*>(_v) = _t->alignment(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->isModified(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->hasSelectedText(); break;
        case 10: *reinterpret_cast< QString*>(_v) = _t->selectedText(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->dragEnabled(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->isReadOnly(); break;
        case 13: *reinterpret_cast< bool*>(_v) = _t->isUndoAvailable(); break;
        case 14: *reinterpret_cast< bool*>(_v) = _t->isRedoAvailable(); break;
        case 15: *reinterpret_cast< bool*>(_v) = _t->hasAcceptableInput(); break;
        case 16: *reinterpret_cast< QString*>(_v) = _t->placeholderText(); break;
        case 17: *reinterpret_cast< Qt::CursorMoveStyle*>(_v) = _t->cursorMoveStyle(); break;
        case 18: *reinterpret_cast< bool*>(_v) = _t->isClearButtonEnabled(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setInputMask(*reinterpret_cast< QString*>(_v)); break;
        case 1: _t->setText(*reinterpret_cast< QString*>(_v)); break;
        case 2: _t->setMaxLength(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setFrame(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setEchoMode(*reinterpret_cast< EchoMode*>(_v)); break;
        case 6: _t->setCursorPosition(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 8: _t->setModified(*reinterpret_cast< bool*>(_v)); break;
        case 11: _t->setDragEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 12: _t->setReadOnly(*reinterpret_cast< bool*>(_v)); break;
        case 16: _t->setPlaceholderText(*reinterpret_cast< QString*>(_v)); break;
        case 17: _t->setCursorMoveStyle(*reinterpret_cast< Qt::CursorMoveStyle*>(_v)); break;
        case 18: _t->setClearButtonEnabled(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QLineEdit::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QLineEdit::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QLineEditE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QLineEdit::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
        _id -= 19;
    }
    return _id;
}

// SIGNAL 0
void QLineEdit::textChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QLineEdit::textEdited(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QLineEdit::cursorPositionChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QLineEdit::returnPressed()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void QLineEdit::editingFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void QLineEdit::selectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void QLineEdit::inputRejected()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP

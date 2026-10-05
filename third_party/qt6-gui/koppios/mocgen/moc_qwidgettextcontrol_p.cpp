/****************************************************************************
** Meta object code from reading C++ file 'qwidgettextcontrol_p.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/widgets/qwidgettextcontrol_p.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwidgettextcontrol_p.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN18QWidgetTextControlE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN18QWidgetTextControlE = QtMocHelpers::stringData(
    "QWidgetTextControl",
    "textChanged",
    "",
    "undoAvailable",
    "b",
    "redoAvailable",
    "currentCharFormatChanged",
    "QTextCharFormat",
    "format",
    "copyAvailable",
    "selectionChanged",
    "cursorPositionChanged",
    "updateRequest",
    "rect",
    "documentSizeChanged",
    "blockCountChanged",
    "newBlockCount",
    "visibilityRequest",
    "microFocusChanged",
    "linkActivated",
    "link",
    "linkHovered",
    "blockMarkerHovered",
    "QTextBlock",
    "block",
    "modificationChanged",
    "m",
    "setPlainText",
    "text",
    "setHtml",
    "undo",
    "redo",
    "clear",
    "selectAll",
    "insertPlainText",
    "append",
    "appendHtml",
    "html",
    "appendPlainText",
    "adjustSize",
    "_q_deleteSelected",
    "_q_copyLink",
    "loadResource",
    "QVariant",
    "type",
    "name",
    "overwriteMode",
    "acceptRichText",
    "cursorWidth",
    "textInteractionFlags",
    "Qt::TextInteractionFlags",
    "openExternalLinks",
    "ignoreUnusedNavigationEvents"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN18QWidgetTextControlE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      31,   14, // methods
       6,  271, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      17,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  200,    2, 0x06,    7 /* Public */,
       3,    1,  201,    2, 0x06,    8 /* Public */,
       5,    1,  204,    2, 0x06,   10 /* Public */,
       6,    1,  207,    2, 0x06,   12 /* Public */,
       9,    1,  210,    2, 0x06,   14 /* Public */,
      10,    0,  213,    2, 0x06,   16 /* Public */,
      11,    0,  214,    2, 0x06,   17 /* Public */,
      12,    1,  215,    2, 0x06,   18 /* Public */,
      12,    0,  218,    2, 0x26,   20 /* Public | MethodCloned */,
      14,    1,  219,    2, 0x06,   21 /* Public */,
      15,    1,  222,    2, 0x06,   23 /* Public */,
      17,    1,  225,    2, 0x06,   25 /* Public */,
      18,    0,  228,    2, 0x06,   27 /* Public */,
      19,    1,  229,    2, 0x06,   28 /* Public */,
      21,    1,  232,    2, 0x06,   30 /* Public */,
      22,    1,  235,    2, 0x06,   32 /* Public */,
      25,    1,  238,    2, 0x06,   34 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      27,    1,  241,    2, 0x0a,   36 /* Public */,
      29,    1,  244,    2, 0x0a,   38 /* Public */,
      30,    0,  247,    2, 0x0a,   40 /* Public */,
      31,    0,  248,    2, 0x0a,   41 /* Public */,
      32,    0,  249,    2, 0x0a,   42 /* Public */,
      33,    0,  250,    2, 0x0a,   43 /* Public */,
      34,    1,  251,    2, 0x0a,   44 /* Public */,
      35,    1,  254,    2, 0x0a,   46 /* Public */,
      36,    1,  257,    2, 0x0a,   48 /* Public */,
      38,    1,  260,    2, 0x0a,   50 /* Public */,
      39,    0,  263,    2, 0x0a,   52 /* Public */,
      40,    0,  264,    2, 0x08,   53 /* Private */,
      41,    0,  265,    2, 0x08,   54 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      42,    2,  266,    2, 0x02,   55 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void, 0x80000000 | 7,    8,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QRectF,   13,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QSizeF,    2,
    QMetaType::Void, QMetaType::Int,   16,
    QMetaType::Void, QMetaType::QRectF,   13,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   20,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, 0x80000000 | 23,   24,
    QMetaType::Void, QMetaType::Bool,   26,

 // slots: parameters
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void, QMetaType::QString,   37,
    QMetaType::Void, QMetaType::QString,   28,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // methods: parameters
    0x80000000 | 43, QMetaType::Int, QMetaType::QUrl,   44,   45,

 // properties: name, type, flags, notifyId, revision
      46, QMetaType::Bool, 0x00015103, uint(-1), 0,
      47, QMetaType::Bool, 0x00015103, uint(-1), 0,
      48, QMetaType::Int, 0x00015103, uint(-1), 0,
      49, 0x80000000 | 50, 0x0001510b, uint(-1), 0,
      51, QMetaType::Bool, 0x00015103, uint(-1), 0,
      52, QMetaType::Bool, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QWidgetTextControl::staticMetaObject = { {
    QMetaObject::SuperData::link<QInputControl::staticMetaObject>(),
    qt_meta_stringdata_ZN18QWidgetTextControlE.offsetsAndSizes,
    qt_meta_data_ZN18QWidgetTextControlE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN18QWidgetTextControlE_t,
        // property 'overwriteMode'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'acceptRichText'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'cursorWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'textInteractionFlags'
        QtPrivate::TypeAndForceComplete<Qt::TextInteractionFlags, std::true_type>,
        // property 'openExternalLinks'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'ignoreUnusedNavigationEvents'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QWidgetTextControl, std::true_type>,
        // method 'textChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'undoAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'redoAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'currentCharFormatChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QTextCharFormat &, std::false_type>,
        // method 'copyAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'selectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'cursorPositionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateRequest'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRectF &, std::false_type>,
        // method 'updateRequest'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'documentSizeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QSizeF &, std::false_type>,
        // method 'blockCountChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'visibilityRequest'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRectF &, std::false_type>,
        // method 'microFocusChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'linkActivated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'linkHovered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'blockMarkerHovered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QTextBlock &, std::false_type>,
        // method 'modificationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setPlainText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setHtml'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'undo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'redo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'clear'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectAll'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'insertPlainText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'append'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'appendHtml'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'appendPlainText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'adjustSize'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method '_q_deleteSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method '_q_copyLink'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadResource'
        QtPrivate::TypeAndForceComplete<QVariant, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QUrl &, std::false_type>
    >,
    nullptr
} };

void QWidgetTextControl::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QWidgetTextControl *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->textChanged(); break;
        case 1: _t->undoAvailable((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->redoAvailable((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->currentCharFormatChanged((*reinterpret_cast< std::add_pointer_t<QTextCharFormat>>(_a[1]))); break;
        case 4: _t->copyAvailable((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->selectionChanged(); break;
        case 6: _t->cursorPositionChanged(); break;
        case 7: _t->updateRequest((*reinterpret_cast< std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 8: _t->updateRequest(); break;
        case 9: _t->documentSizeChanged((*reinterpret_cast< std::add_pointer_t<QSizeF>>(_a[1]))); break;
        case 10: _t->blockCountChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->visibilityRequest((*reinterpret_cast< std::add_pointer_t<QRectF>>(_a[1]))); break;
        case 12: _t->microFocusChanged(); break;
        case 13: _t->linkActivated((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->linkHovered((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->blockMarkerHovered((*reinterpret_cast< std::add_pointer_t<QTextBlock>>(_a[1]))); break;
        case 16: _t->modificationChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 17: _t->setPlainText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->setHtml((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->undo(); break;
        case 20: _t->redo(); break;
        case 21: _t->clear(); break;
        case 22: _t->selectAll(); break;
        case 23: _t->insertPlainText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->append((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->appendHtml((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->appendPlainText((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->adjustSize(); break;
        case 28: _t->d_func()->_q_deleteSelected(); break;
        case 29: _t->d_func()->_q_copyLink(); break;
        case 30: { QVariant _r = _t->loadResource((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QUrl>>(_a[2])));
            if (_a[0]) *reinterpret_cast< QVariant*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QWidgetTextControl::*)();
            if (_q_method_type _q_method = &QWidgetTextControl::textChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(bool );
            if (_q_method_type _q_method = &QWidgetTextControl::undoAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(bool );
            if (_q_method_type _q_method = &QWidgetTextControl::redoAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QTextCharFormat & );
            if (_q_method_type _q_method = &QWidgetTextControl::currentCharFormatChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(bool );
            if (_q_method_type _q_method = &QWidgetTextControl::copyAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)();
            if (_q_method_type _q_method = &QWidgetTextControl::selectionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)();
            if (_q_method_type _q_method = &QWidgetTextControl::cursorPositionChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QRectF & );
            if (_q_method_type _q_method = &QWidgetTextControl::updateRequest; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QSizeF & );
            if (_q_method_type _q_method = &QWidgetTextControl::documentSizeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(int );
            if (_q_method_type _q_method = &QWidgetTextControl::blockCountChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QRectF & );
            if (_q_method_type _q_method = &QWidgetTextControl::visibilityRequest; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)();
            if (_q_method_type _q_method = &QWidgetTextControl::microFocusChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QString & );
            if (_q_method_type _q_method = &QWidgetTextControl::linkActivated; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QString & );
            if (_q_method_type _q_method = &QWidgetTextControl::linkHovered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(const QTextBlock & );
            if (_q_method_type _q_method = &QWidgetTextControl::blockMarkerHovered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (QWidgetTextControl::*)(bool );
            if (_q_method_type _q_method = &QWidgetTextControl::modificationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->overwriteMode(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->acceptRichText(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->cursorWidth(); break;
        case 3: *reinterpret_cast< Qt::TextInteractionFlags*>(_v) = _t->textInteractionFlags(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->openExternalLinks(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->ignoreUnusedNavigationEvents(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setOverwriteMode(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setAcceptRichText(*reinterpret_cast< bool*>(_v)); break;
        case 2: _t->setCursorWidth(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setTextInteractionFlags(*reinterpret_cast< Qt::TextInteractionFlags*>(_v)); break;
        case 4: _t->setOpenExternalLinks(*reinterpret_cast< bool*>(_v)); break;
        case 5: _t->setIgnoreUnusedNavigationEvents(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QWidgetTextControl::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QWidgetTextControl::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN18QWidgetTextControlE.stringdata0))
        return static_cast<void*>(this);
    return QInputControl::qt_metacast(_clname);
}

int QWidgetTextControl::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QInputControl::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 31)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 31;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 31)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 31;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void QWidgetTextControl::textChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QWidgetTextControl::undoAvailable(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QWidgetTextControl::redoAvailable(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QWidgetTextControl::currentCharFormatChanged(const QTextCharFormat & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QWidgetTextControl::copyAvailable(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QWidgetTextControl::selectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void QWidgetTextControl::cursorPositionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void QWidgetTextControl::updateRequest(const QRectF & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 9
void QWidgetTextControl::documentSizeChanged(const QSizeF & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void QWidgetTextControl::blockCountChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void QWidgetTextControl::visibilityRequest(const QRectF & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void QWidgetTextControl::microFocusChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void QWidgetTextControl::linkActivated(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 13, _a);
}

// SIGNAL 14
void QWidgetTextControl::linkHovered(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 14, _a);
}

// SIGNAL 15
void QWidgetTextControl::blockMarkerHovered(const QTextBlock & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 15, _a);
}

// SIGNAL 16
void QWidgetTextControl::modificationChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 16, _a);
}
QT_WARNING_POP

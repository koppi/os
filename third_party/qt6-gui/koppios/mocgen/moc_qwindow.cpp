/****************************************************************************
** Meta object code from reading C++ file 'qwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/kernel/qwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QWindowE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QWindowE = QtMocHelpers::stringData(
    "QWindow",
    "screenChanged",
    "",
    "QScreen*",
    "screen",
    "modalityChanged",
    "Qt::WindowModality",
    "modality",
    "windowStateChanged",
    "Qt::WindowState",
    "windowState",
    "windowTitleChanged",
    "title",
    "xChanged",
    "arg",
    "yChanged",
    "widthChanged",
    "heightChanged",
    "minimumWidthChanged",
    "minimumHeightChanged",
    "maximumWidthChanged",
    "maximumHeightChanged",
    "visibleChanged",
    "visibilityChanged",
    "QWindow::Visibility",
    "visibility",
    "activeChanged",
    "contentOrientationChanged",
    "Qt::ScreenOrientation",
    "orientation",
    "focusObjectChanged",
    "object",
    "opacityChanged",
    "opacity",
    "transientParentChanged",
    "QWindow*",
    "transientParent",
    "requestActivate",
    "setVisible",
    "visible",
    "show",
    "hide",
    "showMinimized",
    "showMaximized",
    "showFullScreen",
    "showNormal",
    "close",
    "raise",
    "lower",
    "startSystemResize",
    "Qt::Edges",
    "edges",
    "startSystemMove",
    "setTitle",
    "setX",
    "setY",
    "setWidth",
    "setHeight",
    "setGeometry",
    "posx",
    "posy",
    "w",
    "h",
    "rect",
    "setMinimumWidth",
    "setMinimumHeight",
    "setMaximumWidth",
    "setMaximumHeight",
    "alert",
    "msec",
    "requestUpdate",
    "_q_clearAlert",
    "flags",
    "Qt::WindowFlags",
    "x",
    "y",
    "width",
    "height",
    "minimumWidth",
    "minimumHeight",
    "maximumWidth",
    "maximumHeight",
    "active",
    "Visibility",
    "contentOrientation",
    "Hidden",
    "AutomaticVisibility",
    "Windowed",
    "Minimized",
    "Maximized",
    "FullScreen",
    "AncestorMode",
    "ExcludeTransients",
    "IncludeTransients"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QWindowE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      46,   14, // methods
      17,  452, // properties
       2,  537, // enums/sets
       0,    0, // constructors
       0,       // flags
      19,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  336,    2, 0x06,   20 /* Public */,
       5,    1,  339,    2, 0x06,   22 /* Public */,
       8,    1,  342,    2, 0x06,   24 /* Public */,
      11,    1,  345,    2, 0x86,   26 /* Public | MethodRevisioned */,
      13,    1,  348,    2, 0x06,   28 /* Public */,
      15,    1,  351,    2, 0x06,   30 /* Public */,
      16,    1,  354,    2, 0x06,   32 /* Public */,
      17,    1,  357,    2, 0x06,   34 /* Public */,
      18,    1,  360,    2, 0x06,   36 /* Public */,
      19,    1,  363,    2, 0x06,   38 /* Public */,
      20,    1,  366,    2, 0x06,   40 /* Public */,
      21,    1,  369,    2, 0x06,   42 /* Public */,
      22,    1,  372,    2, 0x06,   44 /* Public */,
      23,    1,  375,    2, 0x86,   46 /* Public | MethodRevisioned */,
      26,    0,  378,    2, 0x86,   48 /* Public | MethodRevisioned */,
      27,    1,  379,    2, 0x06,   49 /* Public */,
      30,    1,  382,    2, 0x06,   51 /* Public */,
      32,    1,  385,    2, 0x86,   53 /* Public | MethodRevisioned */,
      34,    1,  388,    2, 0x86,   55 /* Public | MethodRevisioned */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      37,    0,  391,    2, 0x8a,   57 /* Public | MethodRevisioned */,
      38,    1,  392,    2, 0x0a,   58 /* Public */,
      40,    0,  395,    2, 0x0a,   60 /* Public */,
      41,    0,  396,    2, 0x0a,   61 /* Public */,
      42,    0,  397,    2, 0x0a,   62 /* Public */,
      43,    0,  398,    2, 0x0a,   63 /* Public */,
      44,    0,  399,    2, 0x0a,   64 /* Public */,
      45,    0,  400,    2, 0x0a,   65 /* Public */,
      46,    0,  401,    2, 0x0a,   66 /* Public */,
      47,    0,  402,    2, 0x0a,   67 /* Public */,
      48,    0,  403,    2, 0x0a,   68 /* Public */,
      49,    1,  404,    2, 0x0a,   69 /* Public */,
      52,    0,  407,    2, 0x0a,   71 /* Public */,
      53,    1,  408,    2, 0x0a,   72 /* Public */,
      54,    1,  411,    2, 0x0a,   74 /* Public */,
      55,    1,  414,    2, 0x0a,   76 /* Public */,
      56,    1,  417,    2, 0x0a,   78 /* Public */,
      57,    1,  420,    2, 0x0a,   80 /* Public */,
      58,    4,  423,    2, 0x0a,   82 /* Public */,
      58,    1,  432,    2, 0x0a,   87 /* Public */,
      64,    1,  435,    2, 0x0a,   89 /* Public */,
      65,    1,  438,    2, 0x0a,   91 /* Public */,
      66,    1,  441,    2, 0x0a,   93 /* Public */,
      67,    1,  444,    2, 0x0a,   95 /* Public */,
      68,    1,  447,    2, 0x8a,   97 /* Public | MethodRevisioned */,
      70,    0,  450,    2, 0x8a,   99 /* Public | MethodRevisioned */,
      71,    0,  451,    2, 0x08,  100 /* Private */,

 // signals: revision
       0,
       0,
       0,
     514,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
     513,
     513,
       0,
       0,
     513,
     525,

 // slots: revision
     513,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
       0,
     513,
     515,
       0,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, QMetaType::QString,   12,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Bool,   14,
    QMetaType::Void, 0x80000000 | 24,   25,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 28,   29,
    QMetaType::Void, QMetaType::QObjectStar,   31,
    QMetaType::Void, QMetaType::QReal,   33,
    QMetaType::Void, 0x80000000 | 35,   36,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   39,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool, 0x80000000 | 50,   51,
    QMetaType::Bool,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::Int,   59,   60,   61,   62,
    QMetaType::Void, QMetaType::QRect,   63,
    QMetaType::Void, QMetaType::Int,   61,
    QMetaType::Void, QMetaType::Int,   62,
    QMetaType::Void, QMetaType::Int,   61,
    QMetaType::Void, QMetaType::Int,   62,
    QMetaType::Void, QMetaType::Int,   69,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
      12, QMetaType::QString, 0x00015103, uint(3), 0,
       7, 0x80000000 | 6, 0x0001510b, uint(1), 0,
      72, 0x80000000 | 73, 0x0001510b, uint(-1), 0,
      74, QMetaType::Int, 0x00015103, uint(4), 0,
      75, QMetaType::Int, 0x00015103, uint(5), 0,
      76, QMetaType::Int, 0x00015103, uint(6), 0,
      77, QMetaType::Int, 0x00015103, uint(7), 0,
      78, QMetaType::Int, 0x00015103, uint(8), 0,
      79, QMetaType::Int, 0x00015103, uint(9), 0,
      80, QMetaType::Int, 0x00015103, uint(10), 0,
      81, QMetaType::Int, 0x00015103, uint(11), 0,
      39, QMetaType::Bool, 0x00015103, uint(12), 0,
      82, QMetaType::Bool, 0x00015001, uint(14), 513,
      25, 0x80000000 | 83, 0x0001510b, uint(13), 513,
      84, 0x80000000 | 28, 0x0001500b, uint(15), 0,
      33, QMetaType::QReal, 0x00015103, uint(17), 513,
      36, 0x80000000 | 35, 0x0001510b, uint(18), 525,

 // enums: name, alias, flags, count, data
      83,   83, 0x0,    6,  547,
      91,   91, 0x0,    2,  559,

 // enum data: key, value
      85, uint(QWindow::Hidden),
      86, uint(QWindow::AutomaticVisibility),
      87, uint(QWindow::Windowed),
      88, uint(QWindow::Minimized),
      89, uint(QWindow::Maximized),
      90, uint(QWindow::FullScreen),
      92, uint(QWindow::ExcludeTransients),
      93, uint(QWindow::IncludeTransients),

       0        // eod
};

Q_CONSTINIT const QMetaObject QWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN7QWindowE.offsetsAndSizes,
    qt_meta_data_ZN7QWindowE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QWindowE_t,
        // property 'title'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'modality'
        QtPrivate::TypeAndForceComplete<Qt::WindowModality, std::true_type>,
        // property 'flags'
        QtPrivate::TypeAndForceComplete<Qt::WindowFlags, std::true_type>,
        // property 'x'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'y'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'width'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'height'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minimumWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minimumHeight'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximumWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximumHeight'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'visible'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'active'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'visibility'
        QtPrivate::TypeAndForceComplete<Visibility, std::true_type>,
        // property 'contentOrientation'
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::true_type>,
        // property 'opacity'
        QtPrivate::TypeAndForceComplete<qreal, std::true_type>,
        // property 'transientParent'
        QtPrivate::TypeAndForceComplete<QWindow*, std::true_type>,
        // enum 'Visibility'
        QtPrivate::TypeAndForceComplete<QWindow::Visibility, std::true_type>,
        // enum 'AncestorMode'
        QtPrivate::TypeAndForceComplete<QWindow::AncestorMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QWindow, std::true_type>,
        // method 'screenChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QScreen *, std::false_type>,
        // method 'modalityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::WindowModality, std::false_type>,
        // method 'windowStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::WindowState, std::false_type>,
        // method 'windowTitleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'xChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'yChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'widthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'heightChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'minimumWidthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'minimumHeightChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'maximumWidthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'maximumHeightChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'visibleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'visibilityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QWindow::Visibility, std::false_type>,
        // method 'activeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'contentOrientationChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::ScreenOrientation, std::false_type>,
        // method 'focusObjectChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QObject *, std::false_type>,
        // method 'opacityChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'transientParentChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QWindow *, std::false_type>,
        // method 'requestActivate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setVisible'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'show'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'hide'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showMinimized'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showMaximized'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showFullScreen'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showNormal'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'close'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'raise'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'lower'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'startSystemResize'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::Edges, std::false_type>,
        // method 'startSystemMove'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setTitle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setX'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setY'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setWidth'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setHeight'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setGeometry'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'setMinimumWidth'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMinimumHeight'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaximumWidth'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setMaximumHeight'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'alert'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'requestUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method '_q_clearAlert'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->screenChanged((*reinterpret_cast< std::add_pointer_t<QScreen*>>(_a[1]))); break;
        case 1: _t->modalityChanged((*reinterpret_cast< std::add_pointer_t<Qt::WindowModality>>(_a[1]))); break;
        case 2: _t->windowStateChanged((*reinterpret_cast< std::add_pointer_t<Qt::WindowState>>(_a[1]))); break;
        case 3: _t->windowTitleChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->xChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->yChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->widthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->heightChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->minimumWidthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->minimumHeightChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->maximumWidthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->maximumHeightChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->visibleChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->visibilityChanged((*reinterpret_cast< std::add_pointer_t<QWindow::Visibility>>(_a[1]))); break;
        case 14: _t->activeChanged(); break;
        case 15: _t->contentOrientationChanged((*reinterpret_cast< std::add_pointer_t<Qt::ScreenOrientation>>(_a[1]))); break;
        case 16: _t->focusObjectChanged((*reinterpret_cast< std::add_pointer_t<QObject*>>(_a[1]))); break;
        case 17: _t->opacityChanged((*reinterpret_cast< std::add_pointer_t<qreal>>(_a[1]))); break;
        case 18: _t->transientParentChanged((*reinterpret_cast< std::add_pointer_t<QWindow*>>(_a[1]))); break;
        case 19: _t->requestActivate(); break;
        case 20: _t->setVisible((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 21: _t->show(); break;
        case 22: _t->hide(); break;
        case 23: _t->showMinimized(); break;
        case 24: _t->showMaximized(); break;
        case 25: _t->showFullScreen(); break;
        case 26: _t->showNormal(); break;
        case 27: { bool _r = _t->close();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 28: _t->raise(); break;
        case 29: _t->lower(); break;
        case 30: { bool _r = _t->startSystemResize((*reinterpret_cast< std::add_pointer_t<Qt::Edges>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 31: { bool _r = _t->startSystemMove();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 32: _t->setTitle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 33: _t->setX((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 34: _t->setY((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 35: _t->setWidth((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 36: _t->setHeight((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 37: _t->setGeometry((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 38: _t->setGeometry((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        case 39: _t->setMinimumWidth((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 40: _t->setMinimumHeight((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 41: _t->setMaximumWidth((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 42: _t->setMaximumHeight((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 43: _t->alert((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 44: _t->requestUpdate(); break;
        case 45: _t->d_func()->_q_clearAlert(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 18:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QWindow* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QWindow::*)(QScreen * );
            if (_q_method_type _q_method = &QWindow::screenChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(Qt::WindowModality );
            if (_q_method_type _q_method = &QWindow::modalityChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(Qt::WindowState );
            if (_q_method_type _q_method = &QWindow::windowStateChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(const QString & );
            if (_q_method_type _q_method = &QWindow::windowTitleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::xChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::yChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::widthChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::heightChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::minimumWidthChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::minimumHeightChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::maximumWidthChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(int );
            if (_q_method_type _q_method = &QWindow::maximumHeightChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(bool );
            if (_q_method_type _q_method = &QWindow::visibleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(QWindow::Visibility );
            if (_q_method_type _q_method = &QWindow::visibilityChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)();
            if (_q_method_type _q_method = &QWindow::activeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(Qt::ScreenOrientation );
            if (_q_method_type _q_method = &QWindow::contentOrientationChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 15;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(QObject * );
            if (_q_method_type _q_method = &QWindow::focusObjectChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 16;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(qreal );
            if (_q_method_type _q_method = &QWindow::opacityChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 17;
                return;
            }
        }
        {
            using _q_method_type = void (QWindow::*)(QWindow * );
            if (_q_method_type _q_method = &QWindow::transientParentChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 18;
                return;
            }
        }
    }
    if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 16:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QWindow* >(); break;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QString*>(_v) = _t->title(); break;
        case 1: *reinterpret_cast< Qt::WindowModality*>(_v) = _t->modality(); break;
        case 2: *reinterpret_cast< Qt::WindowFlags*>(_v) = _t->flags(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->x(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->y(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->width(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->height(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->minimumWidth(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->minimumHeight(); break;
        case 9: *reinterpret_cast< int*>(_v) = _t->maximumWidth(); break;
        case 10: *reinterpret_cast< int*>(_v) = _t->maximumHeight(); break;
        case 11: *reinterpret_cast< bool*>(_v) = _t->isVisible(); break;
        case 12: *reinterpret_cast< bool*>(_v) = _t->isActive(); break;
        case 13: *reinterpret_cast< Visibility*>(_v) = _t->visibility(); break;
        case 14: *reinterpret_cast< Qt::ScreenOrientation*>(_v) = _t->contentOrientation(); break;
        case 15: *reinterpret_cast< qreal*>(_v) = _t->opacity(); break;
        case 16: *reinterpret_cast< QWindow**>(_v) = _t->QWindow::d_func()->transientParent; break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setTitle(*reinterpret_cast< QString*>(_v)); break;
        case 1: _t->setModality(*reinterpret_cast< Qt::WindowModality*>(_v)); break;
        case 2: _t->setFlags(*reinterpret_cast< Qt::WindowFlags*>(_v)); break;
        case 3: _t->setX(*reinterpret_cast< int*>(_v)); break;
        case 4: _t->setY(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setWidth(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setHeight(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setMinimumWidth(*reinterpret_cast< int*>(_v)); break;
        case 8: _t->setMinimumHeight(*reinterpret_cast< int*>(_v)); break;
        case 9: _t->setMaximumWidth(*reinterpret_cast< int*>(_v)); break;
        case 10: _t->setMaximumHeight(*reinterpret_cast< int*>(_v)); break;
        case 11: _t->setVisible(*reinterpret_cast< bool*>(_v)); break;
        case 13: _t->setVisibility(*reinterpret_cast< Visibility*>(_v)); break;
        case 14: _t->reportContentOrientationChange(*reinterpret_cast< Qt::ScreenOrientation*>(_v)); break;
        case 15: _t->setOpacity(*reinterpret_cast< qreal*>(_v)); break;
        case 16: _t->QWindow::d_func()->setTransientParent(*reinterpret_cast< QWindow**>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QWindowE.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QSurface"))
        return static_cast< QSurface*>(this);
    return QObject::qt_metacast(_clname);
}

int QWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 46)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 46;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 46)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 46;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void QWindow::screenChanged(QScreen * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QWindow::modalityChanged(Qt::WindowModality _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QWindow::windowStateChanged(Qt::WindowState _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QWindow::windowTitleChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QWindow::xChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QWindow::yChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QWindow::widthChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void QWindow::heightChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void QWindow::minimumWidthChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}

// SIGNAL 9
void QWindow::minimumHeightChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void QWindow::maximumWidthChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 10, _a);
}

// SIGNAL 11
void QWindow::maximumHeightChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 11, _a);
}

// SIGNAL 12
void QWindow::visibleChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}

// SIGNAL 13
void QWindow::visibilityChanged(QWindow::Visibility _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 13, _a);
}

// SIGNAL 14
void QWindow::activeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void QWindow::contentOrientationChanged(Qt::ScreenOrientation _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 15, _a);
}

// SIGNAL 16
void QWindow::focusObjectChanged(QObject * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 16, _a);
}

// SIGNAL 17
void QWindow::opacityChanged(qreal _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 17, _a);
}

// SIGNAL 18
void QWindow::transientParentChanged(QWindow * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 18, _a);
}
QT_WARNING_POP

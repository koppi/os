/****************************************************************************
** Meta object code from reading C++ file 'qwidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/widgets/kernel/qwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QWidgetE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QWidgetE = QtMocHelpers::stringData(
    "QWidget",
    "windowTitleChanged",
    "",
    "title",
    "windowIconChanged",
    "icon",
    "windowIconTextChanged",
    "iconText",
    "customContextMenuRequested",
    "pos",
    "setEnabled",
    "setDisabled",
    "setWindowModified",
    "setWindowTitle",
    "setStyleSheet",
    "styleSheet",
    "setFocus",
    "update",
    "repaint",
    "setVisible",
    "visible",
    "setHidden",
    "hidden",
    "show",
    "hide",
    "showMinimized",
    "showMaximized",
    "showFullScreen",
    "showNormal",
    "close",
    "raise",
    "lower",
    "updateMicroFocus",
    "Qt::InputMethodQuery",
    "query",
    "_q_showIfNotHidden",
    "_q_closestWindowHandle",
    "QWindow*",
    "grab",
    "rectangle",
    "modal",
    "windowModality",
    "Qt::WindowModality",
    "enabled",
    "geometry",
    "frameGeometry",
    "normalGeometry",
    "x",
    "y",
    "frameSize",
    "size",
    "width",
    "height",
    "rect",
    "childrenRect",
    "childrenRegion",
    "sizePolicy",
    "minimumSize",
    "maximumSize",
    "minimumWidth",
    "minimumHeight",
    "maximumWidth",
    "maximumHeight",
    "sizeIncrement",
    "baseSize",
    "palette",
    "font",
    "cursor",
    "mouseTracking",
    "tabletTracking",
    "isActiveWindow",
    "focusPolicy",
    "Qt::FocusPolicy",
    "focus",
    "contextMenuPolicy",
    "Qt::ContextMenuPolicy",
    "updatesEnabled",
    "minimized",
    "maximized",
    "fullScreen",
    "sizeHint",
    "minimumSizeHint",
    "acceptDrops",
    "windowTitle",
    "windowIcon",
    "windowIconText",
    "windowOpacity",
    "windowModified",
    "layoutDirection",
    "Qt::LayoutDirection",
    "autoFillBackground",
    "locale",
    "windowFilePath",
    "inputMethodHints",
    "Qt::InputMethodHints"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QWidgetE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      29,   14, // methods
      53,  243, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  188,    2, 0x06,   54 /* Public */,
       4,    1,  191,    2, 0x06,   56 /* Public */,
       6,    1,  194,    2, 0x06,   58 /* Public */,
       8,    1,  197,    2, 0x06,   60 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      10,    1,  200,    2, 0x0a,   62 /* Public */,
      11,    1,  203,    2, 0x0a,   64 /* Public */,
      12,    1,  206,    2, 0x0a,   66 /* Public */,
      13,    1,  209,    2, 0x0a,   68 /* Public */,
      14,    1,  212,    2, 0x0a,   70 /* Public */,
      16,    0,  215,    2, 0x0a,   72 /* Public */,
      17,    0,  216,    2, 0x0a,   73 /* Public */,
      18,    0,  217,    2, 0x0a,   74 /* Public */,
      19,    1,  218,    2, 0x0a,   75 /* Public */,
      21,    1,  221,    2, 0x0a,   77 /* Public */,
      23,    0,  224,    2, 0x0a,   79 /* Public */,
      24,    0,  225,    2, 0x0a,   80 /* Public */,
      25,    0,  226,    2, 0x0a,   81 /* Public */,
      26,    0,  227,    2, 0x0a,   82 /* Public */,
      27,    0,  228,    2, 0x0a,   83 /* Public */,
      28,    0,  229,    2, 0x0a,   84 /* Public */,
      29,    0,  230,    2, 0x0a,   85 /* Public */,
      30,    0,  231,    2, 0x0a,   86 /* Public */,
      31,    0,  232,    2, 0x0a,   87 /* Public */,
      32,    1,  233,    2, 0x09,   88 /* Protected */,
      32,    0,  236,    2, 0x29,   90 /* Protected | MethodCloned */,
      35,    0,  237,    2, 0x08,   91 /* Private */,
      36,    0,  238,    2, 0x08,   92 /* Private */,

 // methods: name, argc, parameters, tag, flags, initial metatype offsets
      38,    1,  239,    2, 0x02,   93 /* Public */,
      38,    0,  242,    2, 0x22,   95 /* Public | MethodCloned */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QIcon,    5,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QPoint,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   20,
    QMetaType::Void, QMetaType::Bool,   22,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 33,   34,
    QMetaType::Void,
    QMetaType::Void,
    0x80000000 | 37,

 // methods: parameters
    QMetaType::QPixmap, QMetaType::QRect,   39,
    QMetaType::QPixmap,

 // properties: name, type, flags, notifyId, revision
      40, QMetaType::Bool, 0x00015001, uint(-1), 0,
      41, 0x80000000 | 42, 0x0001510b, uint(-1), 0,
      43, QMetaType::Bool, 0x00015103, uint(-1), 0,
      44, QMetaType::QRect, 0x00015103, uint(-1), 0,
      45, QMetaType::QRect, 0x00015001, uint(-1), 0,
      46, QMetaType::QRect, 0x00015001, uint(-1), 0,
      47, QMetaType::Int, 0x00015001, uint(-1), 0,
      48, QMetaType::Int, 0x00015001, uint(-1), 0,
       9, QMetaType::QPoint, 0x00004003, uint(-1), 0,
      49, QMetaType::QSize, 0x00015001, uint(-1), 0,
      50, QMetaType::QSize, 0x00004003, uint(-1), 0,
      51, QMetaType::Int, 0x00015001, uint(-1), 0,
      52, QMetaType::Int, 0x00015001, uint(-1), 0,
      53, QMetaType::QRect, 0x00015001, uint(-1), 0,
      54, QMetaType::QRect, 0x00015001, uint(-1), 0,
      55, QMetaType::QRegion, 0x00015001, uint(-1), 0,
      56, QMetaType::QSizePolicy, 0x00015103, uint(-1), 0,
      57, QMetaType::QSize, 0x00015103, uint(-1), 0,
      58, QMetaType::QSize, 0x00015103, uint(-1), 0,
      59, QMetaType::Int, 0x00004103, uint(-1), 0,
      60, QMetaType::Int, 0x00004103, uint(-1), 0,
      61, QMetaType::Int, 0x00004103, uint(-1), 0,
      62, QMetaType::Int, 0x00004103, uint(-1), 0,
      63, QMetaType::QSize, 0x00015103, uint(-1), 0,
      64, QMetaType::QSize, 0x00015103, uint(-1), 0,
      65, QMetaType::QPalette, 0x00015103, uint(-1), 0,
      66, QMetaType::QFont, 0x00015103, uint(-1), 0,
      67, QMetaType::QCursor, 0x00015107, uint(-1), 0,
      68, QMetaType::Bool, 0x00015103, uint(-1), 0,
      69, QMetaType::Bool, 0x00015103, uint(-1), 0,
      70, QMetaType::Bool, 0x00015001, uint(-1), 0,
      71, 0x80000000 | 72, 0x0001510b, uint(-1), 0,
      73, QMetaType::Bool, 0x00015001, uint(-1), 0,
      74, 0x80000000 | 75, 0x0001510b, uint(-1), 0,
      76, QMetaType::Bool, 0x00014103, uint(-1), 0,
      20, QMetaType::Bool, 0x00014103, uint(-1), 0,
      77, QMetaType::Bool, 0x00015001, uint(-1), 0,
      78, QMetaType::Bool, 0x00015001, uint(-1), 0,
      79, QMetaType::Bool, 0x00015001, uint(-1), 0,
      80, QMetaType::QSize, 0x00015001, uint(-1), 0,
      81, QMetaType::QSize, 0x00015001, uint(-1), 0,
      82, QMetaType::Bool, 0x00015103, uint(-1), 0,
      83, QMetaType::QString, 0x00015103, uint(0), 0,
      84, QMetaType::QIcon, 0x00015103, uint(1), 0,
      85, QMetaType::QString, 0x00015103, uint(2), 0,
      86, QMetaType::Double, 0x00015103, uint(-1), 0,
      87, QMetaType::Bool, 0x00015103, uint(-1), 0,
      88, 0x80000000 | 89, 0x0001510f, uint(-1), 0,
      90, QMetaType::Bool, 0x00015103, uint(-1), 0,
      15, QMetaType::QString, 0x00015103, uint(-1), 0,
      91, QMetaType::QLocale, 0x00015107, uint(-1), 0,
      92, QMetaType::QString, 0x00015103, uint(-1), 0,
      93, 0x80000000 | 94, 0x0001510b, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN7QWidgetE.offsetsAndSizes,
    qt_meta_data_ZN7QWidgetE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QWidgetE_t,
        // property 'modal'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'windowModality'
        QtPrivate::TypeAndForceComplete<Qt::WindowModality, std::true_type>,
        // property 'enabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'geometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'frameGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'normalGeometry'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'x'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'y'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'pos'
        QtPrivate::TypeAndForceComplete<QPoint, std::true_type>,
        // property 'frameSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'size'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'width'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'height'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'rect'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'childrenRect'
        QtPrivate::TypeAndForceComplete<QRect, std::true_type>,
        // property 'childrenRegion'
        QtPrivate::TypeAndForceComplete<QRegion, std::true_type>,
        // property 'sizePolicy'
        QtPrivate::TypeAndForceComplete<QSizePolicy, std::true_type>,
        // property 'minimumSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'maximumSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'minimumWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'minimumHeight'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximumWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'maximumHeight'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'sizeIncrement'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'baseSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'palette'
        QtPrivate::TypeAndForceComplete<QPalette, std::true_type>,
        // property 'font'
        QtPrivate::TypeAndForceComplete<QFont, std::true_type>,
        // property 'cursor'
        QtPrivate::TypeAndForceComplete<QCursor, std::true_type>,
        // property 'mouseTracking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tabletTracking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'isActiveWindow'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'focusPolicy'
        QtPrivate::TypeAndForceComplete<Qt::FocusPolicy, std::true_type>,
        // property 'focus'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'contextMenuPolicy'
        QtPrivate::TypeAndForceComplete<Qt::ContextMenuPolicy, std::true_type>,
        // property 'updatesEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'visible'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'minimized'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'maximized'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'fullScreen'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'sizeHint'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'minimumSizeHint'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'acceptDrops'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'windowTitle'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'windowIcon'
        QtPrivate::TypeAndForceComplete<QIcon, std::true_type>,
        // property 'windowIconText'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'windowOpacity'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'windowModified'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'layoutDirection'
        QtPrivate::TypeAndForceComplete<Qt::LayoutDirection, std::true_type>,
        // property 'autoFillBackground'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'styleSheet'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'locale'
        QtPrivate::TypeAndForceComplete<QLocale, std::true_type>,
        // property 'windowFilePath'
        QtPrivate::TypeAndForceComplete<QString, std::true_type>,
        // property 'inputMethodHints'
        QtPrivate::TypeAndForceComplete<Qt::InputMethodHints, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QWidget, std::true_type>,
        // method 'windowTitleChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'windowIconChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QIcon &, std::false_type>,
        // method 'windowIconTextChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'customContextMenuRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'setEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setDisabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setWindowModified'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setWindowTitle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setStyleSheet'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'setFocus'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'update'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'repaint'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setVisible'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setHidden'
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
        // method 'updateMicroFocus'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<Qt::InputMethodQuery, std::false_type>,
        // method 'updateMicroFocus'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method '_q_showIfNotHidden'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method '_q_closestWindowHandle'
        QtPrivate::TypeAndForceComplete<QWindow *, std::false_type>,
        // method 'grab'
        QtPrivate::TypeAndForceComplete<QPixmap, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'grab'
        QtPrivate::TypeAndForceComplete<QPixmap, std::false_type>
    >,
    nullptr
} };

void QWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_ASSERT(_o == nullptr || staticMetaObject.cast(_o));
    auto *_t = static_cast<QWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->windowTitleChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->windowIconChanged((*reinterpret_cast< std::add_pointer_t<QIcon>>(_a[1]))); break;
        case 2: _t->windowIconTextChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->customContextMenuRequested((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 4: _t->setEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->setDisabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->setWindowModified((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->setWindowTitle((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->setStyleSheet((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->setFocus(); break;
        case 10: _t->update(); break;
        case 11: _t->repaint(); break;
        case 12: _t->setVisible((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->setHidden((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->show(); break;
        case 15: _t->hide(); break;
        case 16: _t->showMinimized(); break;
        case 17: _t->showMaximized(); break;
        case 18: _t->showFullScreen(); break;
        case 19: _t->showNormal(); break;
        case 20: { bool _r = _t->close();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 21: _t->raise(); break;
        case 22: _t->lower(); break;
        case 23: _t->updateMicroFocus((*reinterpret_cast< std::add_pointer_t<Qt::InputMethodQuery>>(_a[1]))); break;
        case 24: _t->updateMicroFocus(); break;
        case 25: _t->d_func()->_q_showIfNotHidden(); break;
        case 26: { QWindow* _r = _t->d_func()->_q_closestWindowHandle();
            if (_a[0]) *reinterpret_cast< QWindow**>(_a[0]) = std::move(_r); }  break;
        case 27: { QPixmap _r = _t->grab((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1])));
            if (_a[0]) *reinterpret_cast< QPixmap*>(_a[0]) = std::move(_r); }  break;
        case 28: { QPixmap _r = _t->grab();
            if (_a[0]) *reinterpret_cast< QPixmap*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QWidget::*)(const QString & );
            if (_q_method_type _q_method = &QWidget::windowTitleChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QWidget::*)(const QIcon & );
            if (_q_method_type _q_method = &QWidget::windowIconChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QWidget::*)(const QString & );
            if (_q_method_type _q_method = &QWidget::windowIconTextChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QWidget::*)(const QPoint & );
            if (_q_method_type _q_method = &QWidget::customContextMenuRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isModal(); break;
        case 1: *reinterpret_cast< Qt::WindowModality*>(_v) = _t->windowModality(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->isEnabled(); break;
        case 3: *reinterpret_cast< QRect*>(_v) = _t->geometry(); break;
        case 4: *reinterpret_cast< QRect*>(_v) = _t->frameGeometry(); break;
        case 5: *reinterpret_cast< QRect*>(_v) = _t->normalGeometry(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->x(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->y(); break;
        case 8: *reinterpret_cast< QPoint*>(_v) = _t->pos(); break;
        case 9: *reinterpret_cast< QSize*>(_v) = _t->frameSize(); break;
        case 10: *reinterpret_cast< QSize*>(_v) = _t->size(); break;
        case 11: *reinterpret_cast< int*>(_v) = _t->width(); break;
        case 12: *reinterpret_cast< int*>(_v) = _t->height(); break;
        case 13: *reinterpret_cast< QRect*>(_v) = _t->rect(); break;
        case 14: *reinterpret_cast< QRect*>(_v) = _t->childrenRect(); break;
        case 15: *reinterpret_cast< QRegion*>(_v) = _t->childrenRegion(); break;
        case 16: *reinterpret_cast< QSizePolicy*>(_v) = _t->sizePolicy(); break;
        case 17: *reinterpret_cast< QSize*>(_v) = _t->minimumSize(); break;
        case 18: *reinterpret_cast< QSize*>(_v) = _t->maximumSize(); break;
        case 19: *reinterpret_cast< int*>(_v) = _t->minimumWidth(); break;
        case 20: *reinterpret_cast< int*>(_v) = _t->minimumHeight(); break;
        case 21: *reinterpret_cast< int*>(_v) = _t->maximumWidth(); break;
        case 22: *reinterpret_cast< int*>(_v) = _t->maximumHeight(); break;
        case 23: *reinterpret_cast< QSize*>(_v) = _t->sizeIncrement(); break;
        case 24: *reinterpret_cast< QSize*>(_v) = _t->baseSize(); break;
        case 25: *reinterpret_cast< QPalette*>(_v) = _t->palette(); break;
        case 26: *reinterpret_cast< QFont*>(_v) = _t->font(); break;
        case 27: *reinterpret_cast< QCursor*>(_v) = _t->cursor(); break;
        case 28: *reinterpret_cast< bool*>(_v) = _t->hasMouseTracking(); break;
        case 29: *reinterpret_cast< bool*>(_v) = _t->hasTabletTracking(); break;
        case 30: *reinterpret_cast< bool*>(_v) = _t->isActiveWindow(); break;
        case 31: *reinterpret_cast< Qt::FocusPolicy*>(_v) = _t->focusPolicy(); break;
        case 32: *reinterpret_cast< bool*>(_v) = _t->hasFocus(); break;
        case 33: *reinterpret_cast< Qt::ContextMenuPolicy*>(_v) = _t->contextMenuPolicy(); break;
        case 34: *reinterpret_cast< bool*>(_v) = _t->updatesEnabled(); break;
        case 35: *reinterpret_cast< bool*>(_v) = _t->isVisible(); break;
        case 36: *reinterpret_cast< bool*>(_v) = _t->isMinimized(); break;
        case 37: *reinterpret_cast< bool*>(_v) = _t->isMaximized(); break;
        case 38: *reinterpret_cast< bool*>(_v) = _t->isFullScreen(); break;
        case 39: *reinterpret_cast< QSize*>(_v) = _t->sizeHint(); break;
        case 40: *reinterpret_cast< QSize*>(_v) = _t->minimumSizeHint(); break;
        case 41: *reinterpret_cast< bool*>(_v) = _t->acceptDrops(); break;
        case 42: *reinterpret_cast< QString*>(_v) = _t->windowTitle(); break;
        case 43: *reinterpret_cast< QIcon*>(_v) = _t->windowIcon(); break;
        case 44: *reinterpret_cast< QString*>(_v) = _t->windowIconText(); break;
        case 45: *reinterpret_cast< double*>(_v) = _t->windowOpacity(); break;
        case 46: *reinterpret_cast< bool*>(_v) = _t->isWindowModified(); break;
        case 47: *reinterpret_cast< Qt::LayoutDirection*>(_v) = _t->layoutDirection(); break;
        case 48: *reinterpret_cast< bool*>(_v) = _t->autoFillBackground(); break;
        case 49: *reinterpret_cast< QString*>(_v) = _t->styleSheet(); break;
        case 50: *reinterpret_cast< QLocale*>(_v) = _t->locale(); break;
        case 51: *reinterpret_cast< QString*>(_v) = _t->windowFilePath(); break;
        case 52: *reinterpret_cast< Qt::InputMethodHints*>(_v) = _t->inputMethodHints(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 1: _t->setWindowModality(*reinterpret_cast< Qt::WindowModality*>(_v)); break;
        case 2: _t->setEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setGeometry(*reinterpret_cast< QRect*>(_v)); break;
        case 8: _t->move(*reinterpret_cast< QPoint*>(_v)); break;
        case 10: _t->resize(*reinterpret_cast< QSize*>(_v)); break;
        case 16: _t->setSizePolicy(*reinterpret_cast< QSizePolicy*>(_v)); break;
        case 17: _t->setMinimumSize(*reinterpret_cast< QSize*>(_v)); break;
        case 18: _t->setMaximumSize(*reinterpret_cast< QSize*>(_v)); break;
        case 19: _t->setMinimumWidth(*reinterpret_cast< int*>(_v)); break;
        case 20: _t->setMinimumHeight(*reinterpret_cast< int*>(_v)); break;
        case 21: _t->setMaximumWidth(*reinterpret_cast< int*>(_v)); break;
        case 22: _t->setMaximumHeight(*reinterpret_cast< int*>(_v)); break;
        case 23: _t->setSizeIncrement(*reinterpret_cast< QSize*>(_v)); break;
        case 24: _t->setBaseSize(*reinterpret_cast< QSize*>(_v)); break;
        case 25: _t->setPalette(*reinterpret_cast< QPalette*>(_v)); break;
        case 26: _t->setFont(*reinterpret_cast< QFont*>(_v)); break;
        case 27: _t->setCursor(*reinterpret_cast< QCursor*>(_v)); break;
        case 28: _t->setMouseTracking(*reinterpret_cast< bool*>(_v)); break;
        case 29: _t->setTabletTracking(*reinterpret_cast< bool*>(_v)); break;
        case 31: _t->setFocusPolicy(*reinterpret_cast< Qt::FocusPolicy*>(_v)); break;
        case 33: _t->setContextMenuPolicy(*reinterpret_cast< Qt::ContextMenuPolicy*>(_v)); break;
        case 34: _t->setUpdatesEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 35: _t->setVisible(*reinterpret_cast< bool*>(_v)); break;
        case 41: _t->setAcceptDrops(*reinterpret_cast< bool*>(_v)); break;
        case 42: _t->setWindowTitle(*reinterpret_cast< QString*>(_v)); break;
        case 43: _t->setWindowIcon(*reinterpret_cast< QIcon*>(_v)); break;
        case 44: _t->setWindowIconText(*reinterpret_cast< QString*>(_v)); break;
        case 45: _t->setWindowOpacity(*reinterpret_cast< double*>(_v)); break;
        case 46: _t->setWindowModified(*reinterpret_cast< bool*>(_v)); break;
        case 47: _t->setLayoutDirection(*reinterpret_cast< Qt::LayoutDirection*>(_v)); break;
        case 48: _t->setAutoFillBackground(*reinterpret_cast< bool*>(_v)); break;
        case 49: _t->setStyleSheet(*reinterpret_cast< QString*>(_v)); break;
        case 50: _t->setLocale(*reinterpret_cast< QLocale*>(_v)); break;
        case 51: _t->setWindowFilePath(*reinterpret_cast< QString*>(_v)); break;
        case 52: _t->setInputMethodHints(*reinterpret_cast< Qt::InputMethodHints*>(_v)); break;
        default: break;
        }
    }
if (_c == QMetaObject::ResetProperty) {
        switch (_id) {
        case 27: _t->unsetCursor(); break;
        case 47: _t->unsetLayoutDirection(); break;
        case 50: _t->unsetLocale(); break;
        default: break;
        }
    }
}

const QMetaObject *QWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QWidgetE.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QPaintDevice"))
        return static_cast< QPaintDevice*>(this);
    return QObject::qt_metacast(_clname);
}

int QWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 29)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 29;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 29)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 29;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 53;
    }
    return _id;
}

// SIGNAL 0
void QWidget::windowTitleChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QWidget::windowIconChanged(const QIcon & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QWidget::windowIconTextChanged(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QWidget::customContextMenuRequested(const QPoint & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP

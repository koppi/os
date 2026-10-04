/****************************************************************************
** Meta object code from reading C++ file 'qfont.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtbase/src/gui/text/qfont.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qfont.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN5QFontE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN5QFontE = QtMocHelpers::stringData(
    "QFont",
    "StyleHint",
    "Helvetica",
    "SansSerif",
    "Times",
    "Serif",
    "Courier",
    "TypeWriter",
    "OldEnglish",
    "Decorative",
    "System",
    "AnyStyle",
    "Cursive",
    "Monospace",
    "Fantasy",
    "StyleStrategy",
    "PreferDefault",
    "PreferBitmap",
    "PreferDevice",
    "PreferOutline",
    "ForceOutline",
    "PreferMatch",
    "PreferQuality",
    "PreferAntialias",
    "NoAntialias",
    "NoSubpixelAntialias",
    "PreferNoShaping",
    "ContextFontMerging",
    "PreferTypoLineMetrics",
    "NoFontMerging",
    "HintingPreference",
    "PreferDefaultHinting",
    "PreferNoHinting",
    "PreferVerticalHinting",
    "PreferFullHinting",
    "Weight",
    "Thin",
    "ExtraLight",
    "Light",
    "Normal",
    "Medium",
    "DemiBold",
    "Bold",
    "ExtraBold",
    "Black",
    "Style",
    "StyleNormal",
    "StyleItalic",
    "StyleOblique",
    "Stretch",
    "AnyStretch",
    "UltraCondensed",
    "ExtraCondensed",
    "Condensed",
    "SemiCondensed",
    "Unstretched",
    "SemiExpanded",
    "Expanded",
    "ExtraExpanded",
    "UltraExpanded",
    "Capitalization",
    "MixedCase",
    "AllUppercase",
    "AllLowercase",
    "SmallCaps",
    "Capitalize",
    "SpacingType",
    "PercentageSpacing",
    "AbsoluteSpacing",
    "ResolveProperties",
    "NoPropertiesResolved",
    "FamilyResolved",
    "SizeResolved",
    "StyleHintResolved",
    "StyleStrategyResolved",
    "WeightResolved",
    "StyleResolved",
    "UnderlineResolved",
    "OverlineResolved",
    "StrikeOutResolved",
    "FixedPitchResolved",
    "StretchResolved",
    "KerningResolved",
    "CapitalizationResolved",
    "LetterSpacingResolved",
    "WordSpacingResolved",
    "HintingPreferenceResolved",
    "StyleNameResolved",
    "FamiliesResolved",
    "FeaturesResolved",
    "VariableAxesResolved",
    "AllPropertiesResolved"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN5QFontE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       9,   14, // enums/sets
       0,    0, // constructors
       4,       // flags
       0,       // signalCount

 // enums: name, alias, flags, count, data
       1,    1, 0x0,   13,   59,
      15,   15, 0x0,   14,   85,
      30,   30, 0x0,    4,  113,
      35,   35, 0x0,    9,  121,
      45,   45, 0x0,    3,  139,
      49,   49, 0x0,   10,  145,
      60,   60, 0x0,    5,  165,
      66,   66, 0x0,    2,  175,
      69,   69, 0x0,   22,  179,

 // enum data: key, value
       2, uint(QFont::Helvetica),
       3, uint(QFont::SansSerif),
       4, uint(QFont::Times),
       5, uint(QFont::Serif),
       6, uint(QFont::Courier),
       7, uint(QFont::TypeWriter),
       8, uint(QFont::OldEnglish),
       9, uint(QFont::Decorative),
      10, uint(QFont::System),
      11, uint(QFont::AnyStyle),
      12, uint(QFont::Cursive),
      13, uint(QFont::Monospace),
      14, uint(QFont::Fantasy),
      16, uint(QFont::PreferDefault),
      17, uint(QFont::PreferBitmap),
      18, uint(QFont::PreferDevice),
      19, uint(QFont::PreferOutline),
      20, uint(QFont::ForceOutline),
      21, uint(QFont::PreferMatch),
      22, uint(QFont::PreferQuality),
      23, uint(QFont::PreferAntialias),
      24, uint(QFont::NoAntialias),
      25, uint(QFont::NoSubpixelAntialias),
      26, uint(QFont::PreferNoShaping),
      27, uint(QFont::ContextFontMerging),
      28, uint(QFont::PreferTypoLineMetrics),
      29, uint(QFont::NoFontMerging),
      31, uint(QFont::PreferDefaultHinting),
      32, uint(QFont::PreferNoHinting),
      33, uint(QFont::PreferVerticalHinting),
      34, uint(QFont::PreferFullHinting),
      36, uint(QFont::Thin),
      37, uint(QFont::ExtraLight),
      38, uint(QFont::Light),
      39, uint(QFont::Normal),
      40, uint(QFont::Medium),
      41, uint(QFont::DemiBold),
      42, uint(QFont::Bold),
      43, uint(QFont::ExtraBold),
      44, uint(QFont::Black),
      46, uint(QFont::StyleNormal),
      47, uint(QFont::StyleItalic),
      48, uint(QFont::StyleOblique),
      50, uint(QFont::AnyStretch),
      51, uint(QFont::UltraCondensed),
      52, uint(QFont::ExtraCondensed),
      53, uint(QFont::Condensed),
      54, uint(QFont::SemiCondensed),
      55, uint(QFont::Unstretched),
      56, uint(QFont::SemiExpanded),
      57, uint(QFont::Expanded),
      58, uint(QFont::ExtraExpanded),
      59, uint(QFont::UltraExpanded),
      61, uint(QFont::MixedCase),
      62, uint(QFont::AllUppercase),
      63, uint(QFont::AllLowercase),
      64, uint(QFont::SmallCaps),
      65, uint(QFont::Capitalize),
      67, uint(QFont::PercentageSpacing),
      68, uint(QFont::AbsoluteSpacing),
      70, uint(QFont::NoPropertiesResolved),
      71, uint(QFont::FamilyResolved),
      72, uint(QFont::SizeResolved),
      73, uint(QFont::StyleHintResolved),
      74, uint(QFont::StyleStrategyResolved),
      75, uint(QFont::WeightResolved),
      76, uint(QFont::StyleResolved),
      77, uint(QFont::UnderlineResolved),
      78, uint(QFont::OverlineResolved),
      79, uint(QFont::StrikeOutResolved),
      80, uint(QFont::FixedPitchResolved),
      81, uint(QFont::StretchResolved),
      82, uint(QFont::KerningResolved),
      83, uint(QFont::CapitalizationResolved),
      84, uint(QFont::LetterSpacingResolved),
      85, uint(QFont::WordSpacingResolved),
      86, uint(QFont::HintingPreferenceResolved),
      87, uint(QFont::StyleNameResolved),
      88, uint(QFont::FamiliesResolved),
      89, uint(QFont::FeaturesResolved),
      90, uint(QFont::VariableAxesResolved),
      91, uint(QFont::AllPropertiesResolved),

       0        // eod
};

Q_CONSTINIT const QMetaObject QFont::staticMetaObject = { {
    nullptr,
    qt_meta_stringdata_ZN5QFontE.offsetsAndSizes,
    qt_meta_data_ZN5QFontE,
    nullptr,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN5QFontE_t,
        // enum 'StyleHint'
        QtPrivate::TypeAndForceComplete<QFont::StyleHint, std::true_type>,
        // enum 'StyleStrategy'
        QtPrivate::TypeAndForceComplete<QFont::StyleStrategy, std::true_type>,
        // enum 'HintingPreference'
        QtPrivate::TypeAndForceComplete<QFont::HintingPreference, std::true_type>,
        // enum 'Weight'
        QtPrivate::TypeAndForceComplete<QFont::Weight, std::true_type>,
        // enum 'Style'
        QtPrivate::TypeAndForceComplete<QFont::Style, std::true_type>,
        // enum 'Stretch'
        QtPrivate::TypeAndForceComplete<QFont::Stretch, std::true_type>,
        // enum 'Capitalization'
        QtPrivate::TypeAndForceComplete<QFont::Capitalization, std::true_type>,
        // enum 'SpacingType'
        QtPrivate::TypeAndForceComplete<QFont::SpacingType, std::true_type>,
        // enum 'ResolveProperties'
        QtPrivate::TypeAndForceComplete<QFont::ResolveProperties, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QFont, std::true_type>
    >,
    nullptr
} };

QT_WARNING_POP

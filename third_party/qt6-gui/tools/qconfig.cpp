/* koppios addition, not upstream Qt: real Qt6's CMake build generates this
 * from qconfig.cpp.in, substituting real install paths. koppios has no
 * install tree or plugin loader to make those paths meaningful -- every
 * entry is "." (current process directory), matching this whole file's
 * honest-stub pattern for concepts this kernel genuinely doesn't have. */
#include "private/qoffsetstringarray_p.h"

static const char qt_configure_prefix_path_str[12+256] = "qt_prfxpath=.";

static constexpr auto qt_configure_strs = QT_PREPEND_NAMESPACE(qOffsetStringArray)(
    ".", ".", ".", ".", ".", ".", ".", ".", ".", ".", ".", ".", "."
);

#define QT_CONFIGURE_SETTINGS_PATH "."
#define QT_CONFIGURE_LIBLOCATION_TO_PREFIX_PATH "."_L1
#define QT_CONFIGURE_PREFIX_PATH qt_configure_prefix_path_str + 12

/* koppios addition, not upstream Qt: Qt's build compiles resource files with rcc into
 * qrc_*.cpp, which define qInitResources_<name>()/qCleanupResources_<name>() and register the
 * embedded data. QCommonStyle calls Q_INIT_RESOURCE(qstyle) for its standard icon images
 * (message-box icons, file dialog icons, ...). This port builds no resources (rcc is not part
 * of the build, and compressed resources would need zlib), so the initializer is a no-op:
 * QStyle::standardPixmap() simply returns a null pixmap for those icons, which is what Qt
 * itself does when a resource is missing. None of the controls the demo apps draw use them. */
int qInitResources_qstyle() { return 1; }
int qCleanupResources_qstyle() { return 1; }

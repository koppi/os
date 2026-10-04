/* koppios addition, not upstream FreeType: the trimmed module set. Qt's
 * bundled ftmodule.h registers every driver; a QFreeTypeFontDatabase fed
 * TrueType files from memory needs only: autofit (Qt asks for light
 * auto-hinting), the TrueType driver + sfnt tables, PostScript glyph names,
 * and the smooth (anti-aliased) + mono rasterizers. No Type1/CFF/CID/PFR/
 * WinFNT/PCF/BDF/SVG/SDF, which also keeps the binary small. */
FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )

/* koppios addition, not upstream FreeType: replaces src/gzip/ftgzip.c (which
 * needs zlib). Gzip-compressed fonts are unsupported here; FreeType's own
 * documented answer for a build without zlib is FT_Err_Unimplemented_Feature. */
#include <ft2build.h>
#include FT_CONFIG_CONFIG_H
#include <freetype/internal/ftdebug.h>
#include <freetype/ftgzip.h>
#include <freetype/fterrors.h>
#include <freetype/internal/ftstream.h>

FT_EXPORT_DEF(FT_Error) FT_Gzip_Uncompress(FT_Memory memory, FT_Byte *output, FT_ULong *output_len,
                                           const FT_Byte *input, FT_ULong input_len) {
    (void) memory; (void) output; (void) output_len; (void) input; (void) input_len;
    return FT_THROW(Unimplemented_Feature);
}

FT_BASE_DEF(FT_Error) FT_Stream_OpenGzip(FT_Stream stream, FT_Stream source) {
    (void) stream; (void) source;
    return FT_THROW(Unimplemented_Feature);
}

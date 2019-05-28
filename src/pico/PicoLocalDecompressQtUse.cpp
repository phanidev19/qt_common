#include <QString>
#include "PicoLocalDecompressQtUse.h"

_PICO_BEGIN

CompressionType PicoLocalDecompressQtUse::getCompressionType(QString compressionProperty, QString compressionVersion)
{
    // Currently only 1 version of each type exists, so we don't need to use compressionVersion yet, but we will someday

    if (compressionProperty.contains("pico:local:bkr")) return Bruker_Type0;
    if (compressionProperty.contains("pico:local:centroid")) return Centroid_Type1;
    if (compressionProperty.contains("pico:reader:waters:centroid")) return Centroid_Type1;
    if (compressionProperty.contains("pico:local:waters:profile")) return Waters_Type1;
    if (compressionProperty.contains("pico:reader:waters:profile")) return Waters_Type1;

    // Adding to help read Doron's version which puts the whole thing into Version, just in case it accidentally stays there
    if (compressionVersion.contains("pico:local:bkr")) return Bruker_Type0;
    if (compressionVersion.contains("pico:local:centroid")) return Centroid_Type1;
    if (compressionVersion.contains("pico:local:waters:profile")) return Waters_Type1;
    if (compressionVersion.contains("pico:reader:waters:profile")) return Waters_Type1;

    return Unknown;
}

_PICO_END

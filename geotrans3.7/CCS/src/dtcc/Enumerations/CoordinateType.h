// CLASSIFICATION: UNCLASSIFIED

#ifndef CoordinateType_H
#define CoordinateType_H

#include "DtccApi.h"

namespace MSP
{
  namespace CCS
  {
    /**
     * Coordinate conversion supported coordinate or projection system type enumerations.
     * @modified MSP 1.4. Added webMercator enumeration.
     */

   class MSP_DTCC_API CoordinateType
    {
    public:

      enum Enum
      {
        albersEqualAreaConic,
        azimuthalEquidistant,
        bonne,
        britishNationalGrid,
        cassini,
        cylindricalEqualArea,
        eckert4,
        eckert6,
        equidistantCylindrical,
        geocentric,
        geodetic,
        georef,
        globalAreaReferenceSystem,
        gnomonic,
        lambertConformalConic1Parallel,
        lambertConformalConic2Parallels,
        localCartesian,
        mercatorStandardParallel,
        mercatorScaleFactor,
        militaryGridReferenceSystem,
        millerCylindrical,
        mollweide,
        newZealandMapGrid,
        neys,
        obliqueMercator,
        orthographic,
        polarStereographicStandardParallel,
        polarStereographicScaleFactor,
        polyconic,
        sinusoidal,
        stereographic,
        transverseCylindricalEqualArea,
        transverseMercator,
        universalPolarStereographic,
        universalTransverseMercator,
        usNationalGrid,
        vanDerGrinten,
        webMercator
      };
    };
  }
}
	
#endif 



// CLASSIFICATION: UNCLASSIFIED

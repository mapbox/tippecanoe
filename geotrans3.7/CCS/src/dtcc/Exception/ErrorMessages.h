// CLASSIFICATION: UNCLASSIFIED

#ifndef ErrorMessages_H
#define ErrorMessages_H


/**
 * Defines all error messages which may be returned by the ccs. 
 * 
 * @author comstam
 */

#include "DtccApi.h"


namespace MSP
{
   namespace CCS
   {
      class MSP_DTCC_API ErrorMessages
      {  
      public:

         static const char* geoidFileOpenError;
         static const char* geoidFileParseError;
  
         static const char* ellipsoidFileOpenError;
         static const char* ellipsoidFileCloseError;
         static const char* ellipsoidFileParseError;
         static const char* ellipse;
         static const char* invalidEllipsoidCode;
  
         static const char* datumFileOpenError;
         static const char* datumFileCloseError;
         static const char* datumFileParseError;
         static const char* datumDomain;
         static const char* datumRotation;
         static const char* datumSigma;
         static const char* datumType;
         static const char* invalidDatumCode;

         static const char* notUserDefined;
         static const char* ellipseInUse;
   
         // Parameter error messages
         static const char* semiMajorAxis;
         static const char* ellipsoidFlattening;
         static const char* orientation;
         static const char* originLatitude;
         static const char* originLongitude;
         static const char* centralMeridian;
         static const char* scaleFactor;
         static const char* zone;
         static const char* zoneOverride;
         static const char* standardParallel1;
         static const char* standardParallel2;
         static const char* standardParallel1_2;
         static const char* standardParallelHemisphere;
         static const char* precision;
         static const char* bngEllipsoid;
         static const char* nzmgEllipsoid;
         static const char* webmEllipsoid;
         static const char* webmConversionTo;
         static const char* webmInvalidTargetCS;
         static const char* latitude1;
         static const char* latitude2;
         static const char* latitude1_2;
         static const char* longitude1;
         static const char* longitude2;
         static const char* omercHemisphere;
         static const char* hemisphere;
         static const char* radius;

         // Coordinate error messages
         static const char* latitude;
         static const char* longitude;
         static const char* easting;
         static const char* northing;
         static const char* projection;
         static const char* invalidArea;
         static const char* bngString;
         static const char* garsString;
         static const char* georefString;
         static const char* mgrsString;
         static const char* usngString;
         /*   const const char* utmZoneRow = "Invalid UTM Zone Row";
              const const char* zone = "Zone out of range (1-60)\n";
  
              const const char* mgrsString = "Invalid MGRS String\n";

              const const char* radius = "Easting/Northing too far from center of projection\n";
  
              const const char* hemisphere = "Invalid Hemisphere\n";
              const const char* signHemisphere = "Mismatched sign and hemisphere \n";
              const const char* degrees = "Degrees value out of range\n";
              const const char* minutes = "Minutes value out of range\n";
              const const char* seconds = "Seconds value out of range\n";*/
         
         static const char* invalidIndex;
         static const char* invalidName;
         static const char* invalidType;
         static const char* latitude_min;
         static const char* longitude_min;
      };
   }
}
	
#endif 

// CLASSIFICATION: UNCLASSIFIED

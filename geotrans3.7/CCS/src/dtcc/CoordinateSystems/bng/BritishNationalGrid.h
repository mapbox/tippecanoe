// CLASSIFICATION: UNCLASSIFIED

#ifndef BritishNationalGrid_H
#define BritishNationalGrid_H

/***************************************************************************/
/* RSC IDENTIFIER: BRITISH NATIONAL GRID
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and British National Grid coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using 
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       BNG_NO_ERROR               : No errors occurred in function
 *       BNG_LAT_ERROR              : Latitude outside of valid range
 *                                      (49.5 to 61.5 degrees)
 *       BNG_LON_ERROR              : Longitude outside of valid range
 *                                      (-10.0 to 3.5 degrees)
 *       BNG_EASTING_ERROR          : Easting outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       BNG_NORTHING_ERROR         : Northing outside of valid range
 *                                      (depending on ellipsoid and
 *                                       projection parameters)
 *       BNG_STRING_ERROR           : A BNG string error: string too long,
 *                                      too short, or badly formed
 *       BNG_INVALID_AREA_ERROR     : Coordinate is outside of valid area
 *       BNG_ELLIPSOID_ERROR        : Invalid ellipsoid - must be Airy
 *
 * REUSE NOTES
 *
 *    BRITISH NATIONAL GRID is intended for reuse by any application that 
 *    performs a British National Grid projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on BRITISH NATIONAL GRID can be found in the 
 *    Reuse Manual.
 *
 *    BRITISH NATIONAL GRID originated from :  
 *                      U.S. Army Topographic Engineering Center
 *                      Geospatial Information Division
 *                      7701 Telegraph Road
 *                      Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    BRITISH NATIONAL GRID has no restrictions.
 *
 * ENVIRONMENT
 *
 *    BRITISH NATIONAL GRID was tested and certified in the following 
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    09-06-00          Original Code
 *    03-02-07          Original C++ Code
 *
 *
 */

#include "DtccApi.h"
#include "CoordinateSystem.h"



namespace MSP
{
  namespace CCS
  {
    class EllipsoidParameters;
    class TransverseMercator;
    class BNGCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */
    class MSP_DTCC_API BritishNationalGrid : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid code and sets
       * the corresponding state variables. If any errors occur, an exception is thrown 
       * with a description of the error.
       *
       *   ellipsoidCode : 2-letter code for ellipsoid           (input)
       */

	    BritishNationalGrid( char *ellipsoidCode );


      BritishNationalGrid( const BritishNationalGrid &bng );


	    ~BritishNationalGrid( void );


      BritishNationalGrid& operator=( const BritishNationalGrid &bng );


      /*                         
       * The function getParameters returns the current ellipsoid
       * code.
       *
       *   ellipsoidCode : 2-letter code for ellipsoid          (output)
       */

      EllipsoidParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to a BNG coordinate string, according to the 
       * current ellipsoid parameters.  If any errors occur, an exception is thrown 
       * with a description of the error.
       * 
       *    longitude  : Longitude, in radians                   (input)
       *    latitude   : Latitude, in radians                    (input)
       *    precision  : Precision level of BNG string           (input)
       *    BNGString  : British National Grid coordinate string (output)
       *  
       */

      MSP::CCS::BNGCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates, long precision );


      /*
       * The function convertToGeodetic converts a BNG coordinate string 
       * to geodetic (latitude and longitude) coordinates, according to the current
       * ellipsoid parameters. If any errors occur, an exception is thrown 
       * with a description of the error.
       * 
       *    BNGString  : British National Grid coordinate string (input)
       *    longitude  : Longitude, in radians                   (output)
       *    latitude   : Latitude, in radians                    (output)
       *  
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::BNGCoordinates* bngCoordinates );


      /*
       * The function convertFromTransverseMercator converts Transverse Mercator
       * (easting and northing) coordinates to a BNG coordinate string, according
       * to the current ellipsoid parameters.  If any errors occur, an exception is thrown 
       * with a description of the error.
       *
       *    easting    : Easting (X), in meters                  (input)
       *    northing   : Northing (Y), in meters                 (input)
       *    precision  : Precision level of BNG string           (input)
       *    BNGString  : British National Grid coordinate string (output)
       */

      MSP::CCS::BNGCoordinates* convertFromTransverseMercator( MapProjectionCoordinates* mapProjectionCoordinates, long precision );


      /*
       * The function convertToTransverseMercator converts a BNG coordinate string
       * to Transverse Mercator projection (easting and northing) coordinates 
       * according to the current ellipsoid parameters.  If any errors occur, an exception is thrown 
       * with a description of the error.
       *
       *    BNGString  : British National Grid coordinate string (input)
       *    easting    : Easting (X), in meters                  (output)
       *    northing   : Northing (Y), in meters                 (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertToTransverseMercator( MSP::CCS::BNGCoordinates* bngCoordinates );

    private:

      TransverseMercator* transverseMercator;
    
      char BNG_Letters[3];
      double BNG_Easting;
      double BNG_Northing;
      char BNG_Ellipsoid_Code[3];

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

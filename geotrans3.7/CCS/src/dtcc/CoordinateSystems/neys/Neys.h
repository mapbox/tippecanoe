// CLASSIFICATION: UNCLASSIFIED

#ifndef Neys_H
#define Neys_H

/***************************************************************************/
/* RSC IDENTIFIER: NEYS
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Ney's (Modified Lambert
 *    Conformal Conic) projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       NEYS_NO_ERROR           : No errors occurred in function
 *       NEYS_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       NEYS_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       NEYS_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       NEYS_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       NEYS_FIRST_STDP_ERROR   : First standard parallel outside of valid
 *                                     range (ê71 or ê74 degrees)
 *       NEYS_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-89 59 59.0 to 89 59 59.0 degrees)
 *       NEYS_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       NEYS_A_ERROR            : Semi-major axis less than or equal to zero
 *       NEYS_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    NEYS is intended for reuse by any application that performs a Ney's (Modified
 *    Lambert Conformal Conic) projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on NEYS can be found in the Reuse Manual.
 *
 *    NEYS originated from:
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
 *    NEYS has no restrictions.
 *
 * ENVIRONMENT
 *
 *    NEYS was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 95 with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    8-4-00            Original Code
 *    3-2-07            Original C++ Code
 *
 *
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class NeysParameters;
    class LambertConformalConic;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Neys : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Ney's (Modified Lambert Conformal Conic) projection parameters as inputs, and sets the
       * corresponding state variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *   ellipsoidFlattening     : Flattening of ellipsoid                   (input)
       *   centralMeridian         : Longitude of origin, in radians           (input)
       *   originLatitude          : Latitude of origin, in radians            (input)
       *   standardParallel        : First standard parallel, in radians       (input)
       *   falseEasting            : False easting, in meters                  (input)
       *   falseNorthing           : False northing, in meters                 (input)
       */

	    Neys( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double standardParallel, double falseEasting, double falseNorthing );


      Neys( const Neys &n );


	    ~Neys( void );


      Neys& operator=( const Neys &n );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Ney's (Modified Lambert Conformal Conic) projection parameters.
       *
       *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *   ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       *   centralMeridian         : Longitude of origin, in radians           (output)
       *   originLatitude          : Latitude of origin, in radians            (output)
       *   standardParallel        : First standard parallel, in radians       (output)
       *   falseEasting            : False easting, in meters                  (output)
       *   falseNorthing           : False northing, in meters                 (output)
       */

      NeysParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts Geodetic (latitude and
       * longitude) coordinates to Ney's (Modified Lambert Conformal Conic) projection
       * (easting and northing) coordinates, according to the current ellipsoid and
       * Ney's (Modified Lambert Conformal Conic) projection parameters.  If any errors occur, an exception  
       * is thrown with a description of the error.
       *
       *    longitude        : Longitude, in radians                        (input)
       *    latitude         : Latitude, in radians                         (input)
       *    easting          : Easting (X), in meters                       (output)
       *    northing         : Northing (Y), in meters                      (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Ney's (Modified Lambert Conformal
       * Conic) projection (easting and northing) coordinates to Geodetic (latitude)
       * and longitude) coordinates, according to the current ellipsoid and Ney's
       * (Modified Lambert Conformal Conic) projection parameters.  If any errors occur,  
       * an exception is thrown with a description f the error.
       *
       *    easting          : Easting (X), in meters                       (input)
       *    northing         : Northing (Y), in meters                      (input)
       *    longitude        : Longitude, in radians                        (output)
       *    latitude         : Latitude, in radians                         (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:

      LambertConformalConic* lambertConformalConic2;

      /* Ney's projection Parameters */
      double Neys_Std_Parallel_1;             /* Lower std. parallel, in radians */
      double Neys_Std_Parallel_2;             /* Upper std. parallel, in radians */
      double Neys_Origin_Lat;                 /* Latitude of origin, in radians */
      double Neys_Origin_Long;                /* Longitude of origin, in radians */
      double Neys_False_Northing;             /* False northing, in meters */
      double Neys_False_Easting;              /* False easting, in meters */

      /* Maximum variance for easting and northing values for WGS 84. */
      double Neys_Delta_Easting;
      double Neys_Delta_Northing;
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

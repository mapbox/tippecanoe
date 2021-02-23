// CLASSIFICATION: UNCLASSIFIED

#ifndef LambertConformalConic1_H
#define LambertConformalConic1_H

/***************************************************************************/
/* RSC IDENTIFIER: LAMBERT
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Lambert Conformal Conic
 *    (1 or 2 Standard Parallel) projection coordinates (easting and northing in meters) defined
 *    by one standard parallel and specified scale true along that parallel, or two standard parallels.  
 *    When both standard parallel parameters
 *    are set to the same latitude value, the result is a Lambert 
 *    Conformal Conic projection with one standard parallel at the 
 *    specified latitude.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       LAMBERT_NO_ERROR           : No errors occurred in function
 *       LAMBERT_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *       LAMBERT_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *       LAMBERT_EASTING_ERROR      : Easting outside of valid range
 *                                      (depends on ellipsoid and projection
 *                                     parameters)
 *       LAMBERT_NORTHING_ERROR     : Northing outside of valid range
 *                                      (depends on ellipsoid and projection
 *                                     parameters)
 *       LAMBERT_ORIGIN_LAT_ERROR   : Origin latitude outside of valid
 *                                      range (-89 59 59.0 to 89 59 59.0 degrees)
 *                                      or within one second of 0 degrees
 *       LAMBERT_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *		   LAMBERT_SCALE_FACTOR_ERROR : Scale factor greater than minimum scale
 *                                      factor (1.0e-9)
 *       LAMBERT_A_ERROR            : Semi-major axis less than or equal to zero
 *       LAMBERT_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                    (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    LAMBERT is intended for reuse by any application that performs a Lambert
 *    Conformal Conic (1 or 2 Standard Parallel) projection or its inverse.
 *    
 * REFERENCES
 *
 *    Further information on LAMBERT can be found in the Reuse Manual.
 *
 *    LAMBERT originated from:
 *                      Information Technologoy - Spatial Reference Model(SRM)
 *                      ISO/IEC FDIS 18026
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    LAMBERT has no restrictions.
 *
 * ENVIRONMENT
 *
 *    LAMBERT was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Windows 98/2000/XP with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    03-05-2005        Original Code
 *    03-02-2007        Original C++ Code
 *    02-25-2009        Merged Lambert 1 and 2
 *
 */


#include "CoordinateSystem.h"
#include "CoordinateType.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection5Parameters;
    class MapProjection6Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class LambertConformalConic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * Lambert Conformal Conic (1 Standard Parallel) projection parameters as inputs, and sets the
       * corresponding state variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *   ellipsoidSemiMajorAxis     : Semi-major axis of ellipsoid, in meters   (input)
       *   ellipsoidFlattening        : Flattening of ellipsoid				            (input)
       *   centralMeridian            : Longitude of origin, in radians           (input)
       *   originLatitude             : Latitude of origin, in radians            (input)
       *   falseEasting               : False easting, in meters                  (input)
       *   falseNorthing              : False northing, in meters                 (input)
       *   scaleFactor                : Projection scale factor                   (input) 
       *
       */

	    LambertConformalConic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing, double scaleFactor );


      /*
       * The constructor receives the ellipsoid parameters and
       * Lambert Conformal Conic (2 Standard Parallel) projection parameters as inputs, and sets the
       * corresponding state variables.  If any errors occur, an exception is thrown 
       * with a description of the error.
       *
       *   ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters   (input)
       *   ellipsoidFlattening      : Flattening of ellipsoid				            (input)
       *   centralMeridian          : Longitude of origin, in radians           (input)
       *   originLatitude           : Latitude of origin, in radians            (input)
       *   standardParallel1        : First standard parallel, in radians       (input)
       *   standardParallel2        : Second standard parallel, in radians      (input)
       *   falseEasting             : False easting, in meters                  (input)
       *   falseNorthing            : False northing, in meters                 (input)
       *
       *   Note that when the two standard parallel parameters are both set to the 
       *   same latitude value, the result is a Lambert Conformal Conic projection 
       *   with one standard parallel at the specified latitude.
       */

      LambertConformalConic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double standardParallel1, double standardParallel2, double falseEasting, double falseNorthing );


      LambertConformalConic( const LambertConformalConic &lcc1 );


	    ~LambertConformalConic( void );


      LambertConformalConic& operator=( const LambertConformalConic &lcc1 );


      /*                         
       * The function get1StandardParallelParameters returns the current ellipsoid
       * parameters and Lambert Conformal Conic (1 Standard Parallel) projection parameters.
       *
       *   ellipsoidSemiMajorAxis    : Semi-major axis of ellipsoid, in meters   (output)
       *   ellipsoidFlattening       : Flattening of ellipsoid					         (output)
       *   centralMeridian           : Longitude of origin, in radians           (output)
       *   originLatitude            : Latitude of origin, in radians            (output)
       *   falseEasting              : False easting, in meters                  (output)
       *   falseNorthing             : False northing, in meters                 (output)
       *   scaleFactor               : Projection scale factor                   (output) 
       */

      MapProjection5Parameters* get1StandardParallelParameters() const;


      /*                         
       * The function get2StandardParallelParameters returns the current ellipsoid
       * parameters and Lambert Conformal Conic (2 Standard Parallel) projection parameters.
       *
       *   ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *   ellipsoidFlattening     : Flattening of ellipsoid					         (output)
       *   centralMeridian         : Longitude of origin, in radians           (output)
       *   originLatitude          : Latitude of origin, in radians            (output)
       *   standardParallel1       : First standard parallel, in radians       (output)
       *   standardParallel2       : Second standard parallel, in radians      (output)
       *   falseEasting            : False easting, in meters                  (output)
       *   falseNorthing           : False northing, in meters                 (output)
       */

      MapProjection6Parameters* get2StandardParallelParameters() const;


      /*
       * The function convertFromGeodetic converts Geodetic (latitude and
       * longitude) coordinates to Lambert Conformal Conic (1 or 2 Standard Parallel) projection (easting
       * and northing) coordinates, according to the current ellipsoid and
       * Lambert Conformal Conic (1 or 2 Standard Parallel) projection parameters.  If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *    longitude        : Longitude, in radians                        (input)
       *    latitude         : Latitude, in radians                         (input)
       *    easting          : Easting (X), in meters                       (output)
       *    northing         : Northing (Y), in meters                      (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Lambert Conformal
       * Conic (1 or 2 Standard Parallel) projection (easting and northing) coordinates to Geodetic
       * (latitude and longitude) coordinates, according to the current ellipsoid
       * and Lambert Conformal Conic (1 or 2 Standard Parallel) projection parameters.  If any errors occur,
       * an exception is thrown with a description of the error.
       *
       *    easting          : Easting (X), in meters                       (input)
       *    northing         : Northing (Y), in meters                      (input)
       *    longitude        : Longitude, in radians                        (output)
       *    latitude         : Latitude, in radians                         (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      CoordinateType::Enum coordinateType;

      struct CommonParameters
      {
        double _lambertN;                          /* Ratio of angle between meridians */
        double _lambertRho0;                       /* Height above ellipsoid */
        double _lambertRhoOlat;
        double _lambertT0;

        /* Lambert_Conformal_Conic projection Parameters */
        double _lambertOriginLatitude;            /* Latitude of origin in radians */
        double _lambertFalseNorthing;             /* False northing, in meters */
        double _lambertScaleFactor;               /* Scale Factor */    
      };

      /* Ellipsoid Parameters, default to WGS 84  */
      double es;                                   /* Eccentricity of ellipsoid */
      double es_OVER_2;                            /* Eccentricity / 2.0 */
      double Lambert_1_n;                          /* Ratio of angle between meridians */
      double Lambert_1_rho0;                       /* Height above ellipsoid */
      double Lambert_1_rho_olat;
      double Lambert_1_t0;

      /* Lambert_Conformal_Conic projection Parameters */
      double Lambert_Origin_Latitude;            /* Latitude of origin in radians */
      double Lambert_Origin_Long;                /* Longitude of origin, in radians */
      double Lambert_False_Northing;             /* False northing, in meters */
      double Lambert_False_Easting;              /* False easting, in meters */
      double Lambert_Scale_Factor;               /* Scale Factor */

      /* Lambert_Conformal_Conic 2 projection Parameters */
      double Lambert_2_Std_Parallel_1;                   /* Lower std. parallel, in radians */
      double Lambert_2_Std_Parallel_2;                   /* Upper std. parallel, in radians */
      double Lambert_2_Origin_Lat;     /* Latitude of origin, in radians */

      /* Maximum variance for easting and northing values for WGS 84. */
      double Lambert_Delta_Easting;
      double Lambert_Delta_Northing;


      CommonParameters* setCommonLambert1StandardParallelParameters(double originLatitude, double falseNorthing, double scaleFactor);

      double calculateLambert2StandardParallel(double es2, double phi, double tempPhi, double c);

      double lambertM( double clat, double essin );

      double lambertT( double lat, double essin );

      double esSin(double sinlat);
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

// CLASSIFICATION: UNCLASSIFIED

#ifndef PolarStereographic_H
#define PolarStereographic_H

/***************************************************************************/
/* RSC IDENTIFIER: POLAR STEREOGRAPHIC 
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude and
 *    longitude) coordinates and Polar Stereographic (easting and northing) 
 *    coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid 
 *    value is found the error code is combined with the current error code 
 *    using the bitwise or.  This combining allows multiple error codes to 
 *    be returned. The possible error codes are:
 *
 *          POLAR_NO_ERROR           : No errors occurred in function
 *          POLAR_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          POLAR_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees) 
 *          POLAR_ORIGIN_LAT_ERROR   : Latitude of true scale outside of valid
 *                                      range (-90 to 90 degrees)
 *          POLAR_ORIGIN_LON_ERROR   : Longitude down from pole outside of valid
 *                                      range (-180 to 360 degrees)
 *          POLAR_EASTING_ERROR      : Easting outside of valid range,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          POLAR_NORTHING_ERROR     : Northing outside of valid range,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          POLAR_RADIUS_ERROR       : Coordinates too far from pole,
 *                                      depending on ellipsoid and
 *                                      projection parameters
 *          POLAR_A_ERROR            : Semi-major axis less than or equal to zero
 *          POLAR_INV_F_ERROR        : Inverse flattening outside of valid range
 *								  	                  (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    POLAR STEREOGRAPHIC is intended for reuse by any application that  
 *    performs a Polar Stereographic projection.
 *
 *
 * REFERENCES
 *
 *    Further information on POLAR STEREOGRAPHIC can be found in the
 *    Reuse Manual.
 *
 *
 *    POLAR STEREOGRAPHIC originated from :
 *                                U.S. Army Topographic Engineering Center
 *                                Geospatial Information Division
 *                                7701 Telegraph Road
 *                                Alexandria, VA  22310-3864
 *
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 *
 * RESTRICTIONS
 *
 *    POLAR STEREOGRAPHIC has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    POLAR STEREOGRAPHIC was tested and certified in the following
 *    environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. Window 95 with MS Visual C++, version 6
 *
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    2-27-07          Original Code
 *
 *
 */


#include "CoordinateSystem.h"
#include "CoordinateType.h"


namespace MSP
{
  namespace CCS
  {
    class PolarStereographicStandardParallelParameters;
    class PolarStereographicScaleFactorParameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /**********************************************************************/
    /*
     *                        DEFINES
     */

    class PolarStereographic : public CoordinateSystem
    {
    public:

      /*  
       * The constructor receives the ellipsoid
       * parameters and Polar Stereograpic (Standard Parallel) projection parameters as inputs, and
       * sets the corresponding state variables.  If any errors occur, an exception 
       * is thrown with a description of the error.
       *
       *  ellipsoidSemiMajorAxis                : Semi-major axis of ellipsoid, in meters         (input)
       *  ellipsoidFlattening                   : Flattening of ellipsoid					                (input)
       *  centralMeridian                       : Longitude down from pole, in radians            (input)
       *  standardParallel                      : Latitude of true scale, in radians              (input)
       *  falseEasting                          : Easting (X) at center of projection, in meters  (input)
       *  falseNorthing                         : Northing (Y) at center of projection, in meters (input)
       */

      PolarStereographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double standardParallel, double falseEasting, double falseNorthing );


      /*  
       * The constructor receives the ellipsoid
       * parameters and Polar Stereograpic (Scale Factor) projection parameters as inputs, and
       * sets the corresponding state variables.  If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *  ellipsoidSemiMajorAxis                : Semi-major axis of ellipsoid, in meters         (input)
       *  ellipsoidFlattening                   : Flattening of ellipsoid					                (input)
       *  centralMeridian                       : Longitude down from pole, in radians            (input)
       *  scaleFactor                           : Scale Factor              (input)
       *  falseEasting                          : Easting (X) at center of projection, in meters  (input)
       *  falseNorthing                         : Northing (Y) at center of projection, in meters (input)
       */

      PolarStereographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double scaleFactor, char hemisphere, double falseEasting, double falseNorthing );


      PolarStereographic( const PolarStereographic &ps );


	    ~PolarStereographic( void );


      PolarStereographic& operator=( const PolarStereographic &ps );


      /*
       * The function getStandardParallelParameters returns the current
       * ellipsoid parameters and Polar (Standard Parallel) projection parameters.
       *
       *  ellipsoidSemiMajorAxis          : Semi-major axis of ellipsoid, in meters         (output)
       *  ellipsoidFlattening             : Flattening of ellipsoid					                (output)
       *  centralMeridian                 : Longitude down from pole, in radians            (output)
       *  standardParallel                : Latitude of true scale, in radians              (output)
       *  falseEasting                    : Easting (X) at center of projection, in meters  (output)
       *  falseNorthing                   : Northing (Y) at center of projection, in meters (output)
       */

      PolarStereographicStandardParallelParameters* getStandardParallelParameters() const;


      /*
       * The function getScaleFactorParameters returns the current
       * ellipsoid parameters and Polar (Scale Factor) projection parameters.
       *
       *  ellipsoidSemiMajorAxis          : Semi-major axis of ellipsoid, in meters         (output)
       *  ellipsoidFlattening             : Flattening of ellipsoid					                (output)
       *  centralMeridian                 : Longitude down from pole, in radians            (output)
       *  scaleFactor                     : Scale factor                                    (output)
       *  falseEasting                    : Easting (X) at center of projection, in meters  (output)
       *  falseNorthing                   : Northing (Y) at center of projection, in meters (output)
       */

      PolarStereographicScaleFactorParameters* getScaleFactorParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic
       * coordinates (latitude and longitude) to Polar Stereographic coordinates
       * (easting and northing), according to the current ellipsoid
       * and Polar Stereographic projection parameters. If any errors occur, 
       * an exception is thrown with a description of the error.
       *
       *    longitude  :  Longitude, in radians                     (input)
       *    latitude   :  Latitude, in radians                      (input)
       *    easting    :  Easting (X), in meters                    (output)
       *    northing   :  Northing (Y), in meters                   (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Polar
       * Stereographic coordinates (easting and northing) to geodetic
       * coordinates (latitude and longitude) according to the current ellipsoid
       * and Polar Stereographic projection Parameters. If any errors occur,  
       * an exception is thrown with a description of the error.
       *
       *  easting          : Easting (X), in meters                   (input)
       *  northing         : Northing (Y), in meters                  (input)
       *  longitude        : Longitude, in radians                    (output)
       *  latitude         : Latitude, in radians                     (output)
       *
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:

      CoordinateType::Enum coordinateType;

      double es;                             /* Eccentricity of ellipsoid    */
      double es_OVER_2;                      /* es / 2.0 */
      double Southern_Hemisphere;            /* Flag variable */
      double Polar_tc;
      double Polar_k90;
      double Polar_a_mc;                     /* Polar_a * mc */
      double two_Polar_a;                    /* 2.0 * Polar_a */

      /* Polar Stereographic projection Parameters */
      double Polar_Standard_Parallel;        /* Latitude of origin in radians */
      double Polar_Central_Meridian;         /* Longitude of origin in radians */
      double Polar_False_Easting;            /* False easting in meters */
      double Polar_False_Northing;           /* False northing in meters */

      /* Maximum variance for easting and northing values for WGS 84. */
      double Polar_Delta_Easting;
      double Polar_Delta_Northing;

      double Polar_Scale_Factor;

      double polarPow( double esSin);
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

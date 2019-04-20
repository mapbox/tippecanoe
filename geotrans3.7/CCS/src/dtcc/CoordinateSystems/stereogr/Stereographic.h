// CLASSIFICATION: UNCLASSIFIED

#ifndef Stereographic_H
#define Stereographic_H

/***************************************************************************/
/* RSC IDENTIFIER: STEREOGRAPHIC
 *
 *
 * ABSTRACT
 *
 *    This component provides conversions between geodetic (latitude and
 *    longitude) coordinates and Stereographic (easting and northing)
 *    coordinates.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid
 *    value is found the error code is combined with the current error code
 *    using the bitwise or.  This combining allows multiple error codes to
 *    be returned. The possible error codes are:
 *
 *          STEREO_NO_ERROR           : No errors occurred in function
 *          STEREO_LAT_ERROR          : Latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          STEREO_LON_ERROR          : Longitude outside of valid range
 *                                      (-180 to 360 degrees)
 *          STEREO_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                      (-90 to 90 degrees)
 *          STEREO_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                      (-180 to 360 degrees)
 *          STEREO_EASTING_ERROR      : Easting outside of valid range,
 *                                      (False_Easting +/- ~1,460,090,226 m,
 *                                       depending on ellipsoid and projection
 *                                       parameters)
 *          STEREO_NORTHING_ERROR     : Northing outside of valid range,
 *                                      (False_Northing +/- ~1,460,090,226 m,
 *                                       depending on ellipsoid and projection
 *                                       parameters)
 *          STEREO_A_ERROR            : Semi-major axis less than or equal to zero
 *          STEREO_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                      (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    STEREOGRAPHIC is intended for reuse by any application that
 *    performs a Stereographic projection.
 *
 *
 * REFERENCES
 *
 *    Further information on STEREOGRAPHIC can be found in the
 *    Reuse Manual.
 *
 *
 *    STEREOGRAPHIC originated from :
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
 *    STEREOGRAPHIC has no restrictions.
 *
 *
 * ENVIRONMENT
 *
 *    STEREOGRAPHIC was tested and certified in the following
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
 *    3-1-07            Original Code
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class MapProjection4Parameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class Stereographic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid
       * parameters and Stereograpic projection parameters as inputs, and
       * sets the corresponding state variables.  If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *  ellipsoidSemiMajorAxis    : Semi-major axis of ellipsoid, in meters         (input)
       *  ellipsoidFlattening       : Flattening of ellipsoid                         (input)
       *  centralMeridian           : Longitude, in radians, at the center of         (input)
       *                              the projection
       *  originLatitude            : Latitude, in radians, at the center of          (input)
       *                              the projection
       *  falseEasting              : Easting (X) at center of projection, in meters  (input)
       *  falseNorthing             : Northing (Y) at center of projection, in meters (input)
       */

	    Stereographic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Stereographic( const Stereographic &s );


	    ~Stereographic( void );


      Stereographic& operator=( const Stereographic &s );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Stereographic projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       *    centralMeridian         : Longitude, in radians, at the center of   (output)
       *                              the projection
       *    originLatitude          : Latitude, in radians, at the center of    (output)
       *                              the projection
       *    falseEasting            : A coordinate value, in meters, assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value, in meters, assigned to the
       *                              origin latitude of the projection         (output)
       */

      MapProjection4Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic
       * coordinates (latitude and longitude) to Stereographic coordinates
       * (easting and northing), according to the current ellipsoid
       * and Stereographic projection parameters. If any errors occur, an 
       * exception is thrown with a description of the error.
       *
       *    longitude  :  Longitude, in radians                     (input)
       *    latitude   :  Latitude, in radians                      (input)
       *    easting    :  Easting (X), in meters                    (output)
       *    northing   :  Northing (Y), in meters                   (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Stereographic projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Stereographic projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X), in meters              (input)
       *    northing          : Northing (Y), in meters             (input)
       *    longitude         : Longitude (lambda), in radians      (output)
       *    latitude          : Latitude (phi), in radians          (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84  */
      double Stereo_Ra;             /* Spherical Radius */
      double Two_Stereo_Ra;        /* 2 * Spherical Radius */
      long Stereo_At_Pole;                        /* Flag variable */

      /* Stereographic projection Parameters */
      double Stereo_Origin_Lat;                 /* Latitude of origin, in radians */
      double Stereo_Origin_Long;                /* Longitude of origin, in radians */
      double Stereo_False_Easting;              /* False easting, in meters */
      double Stereo_False_Northing;             /* False northing, in meters */
      double Sin_Stereo_Origin_Lat;             /* sin(Stereo_Origin_Lat) */
      double Cos_Stereo_Origin_Lat;             /* cos(Stereo_Origin_Lat) */

      /* Maximum variance for easting and northing values for WGS 84. */
      double Stereo_Delta_Easting;
      double Stereo_Delta_Northing;
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

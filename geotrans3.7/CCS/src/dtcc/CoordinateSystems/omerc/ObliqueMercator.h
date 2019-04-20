// CLASSIFICATION: UNCLASSIFIED

#ifndef ObliqueMercator_H
#define ObliqueMercator_H

/***************************************************************************/
/* RSC IDENTIFIER: OBLIQUE MERCATOR
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Oblique Mercator
 *    projection coordinates (easting and northing in meters).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       OMERC_NO_ERROR           : No errors occurred in function
 *       OMERC_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       OMERC_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-89 to 89 degrees)
 *       OMERC_LAT1_ERROR         : First latitude outside of valid range
 *                                     (-89 to 89 degrees, excluding 0)
 *       OMERC_LAT2_ERROR         : First latitude outside of valid range
 *                                     (-89 to 89 degrees)
 *       OMERC_LON1_ERROR         : First longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_LON2_ERROR         : Second longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       OMERC_LAT1_LAT2_ERROR    : First and second latitudes can not be equal
 *       OMERC_DIFF_HEMISPHERE_ERROR: First and second latitudes can not be
 *                                     in different hemispheres
 *       OMERC_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       OMERC_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       OMERC_A_ERROR            : Semi-major axis less than or equal to zero
 *       OMERC_INV_F_ERROR        : Inverse flattening outside of valid range
 *                                     (250 to 350)
 *       OMERC_SCALE_FACTOR_ERROR : Scale factor outside of valid
 *                                     range (0.3 to 3.0)
 *       OMERC_LON_WARNING        : Distortion will result if longitude is 90 degrees or more
 *                                     from the Central Meridian
 *
 * REUSE NOTES
 *
 *    OBLIQUE MERCATOR is intended for reuse by any application that 
 *    performs an Oblique Mercator projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on OBLIQUE MERCATOR can be found in the Reuse Manual.
 *
 *    OBLIQUE MERCATOR originated from:     U.S. Army Topographic Engineering Center
 *                                          Geospatial Information Division
 *                                          7701 Telegraph Road
 *                                          Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    OBLIQUE MERCATOR has no restrictions.
 *
 * ENVIRONMENT
 *
 *    OBLIQUE MERCATOR was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    06-07-00          Original Code
 *    03-02-07          Original C++ Code
 *    
 *
 */


#include "CoordinateSystem.h"


namespace MSP
{
  namespace CCS
  {
    class ObliqueMercatorParameters;
    class MapProjectionCoordinates;
    class GeodeticCoordinates;


    /***************************************************************************/
    /*
     *                              DEFINES
     */

    class ObliqueMercator : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis   : Semi-major axis of ellipsoid, in meters  (input)
       *    ellipsoidFlattening      : Flattening of ellipsoid                  (input)
       *    originLatitude           : Latitude, in radians, at which the       (input)
       *                               point scale factor is 1.0
       *    longitude1               : Longitude, in radians, of first point lying on
       *                               central line                             (input)
       *    latitude1                : Latitude, in radians, of first point lying on
       *                               central line                             (input)
       *    longitude2               : Longitude, in radians, of second point lying on
       *                               central line                             (input)
       *    latitude2                : Latitude, in radians, of second point lying on
       *                               central line                             (input)
       *    falseEasting             : A coordinate value, in meters, assigned to the
       *                               central meridian of the projection       (input)
       *    falseNorthing            : A coordinate value, in meters, assigned to the
       *                               origin latitude of the projection        (input)
       *    scaleFactor              : Multiplier which reduces distances in the
       *                               projection to the actual distance on the
       *                               ellipsoid                                (input)
       *    errorStatus              : Error status                             (output) 
       */

	    ObliqueMercator( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double originLatitude, double longitude1, double latitude1, double longitude2, double latitude2, double falseEasting, double falseNorthing, double scaleFactor );


      ObliqueMercator( const ObliqueMercator &om );


	    ~ObliqueMercator( void );


      ObliqueMercator& operator=( const ObliqueMercator &om );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Oblique Mercator projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters  (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                  (output)
       *    originLatitude          : Latitude, in radians, at which the       (output)
       *                              point scale factor is 1.0
       *    longitude1              : Longitude, in radians, of first point lying on
       *                              central line                           (output)
       *    latitude1               : Latitude, in radians, of first point lying on
       *                              central line                           (output)
       *    longitude2              : Longitude, in radians, of second point lying on
       *                              central line                           (output)
       *    latitude2               : Latitude, in radians, of second point lying on
       *                              central line                           (output)
       *    falseEasting            : A coordinate value, in meters, assigned to the
       *                              central meridian of the projection     (output)
       *    falseNorthing           : A coordinate value, in meters, assigned to the
       *                              origin latitude of the projection      (output)
       *    scaleFactor             : Multiplier which reduces distances in the
       *                              projection to the actual distance on the
       *                              ellipsoid                              (output)
       */

      ObliqueMercatorParameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Oblique Mercator projection (easting and
       * northing) coordinates, according to the current ellipsoid and Oblique Mercator 
       * projection parameters.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    longitude         : Longitude (lambda), in radians       (input)
       *    latitude          : Latitude (phi), in radians           (input)
       *    easting           : Easting (X), in meters               (output)
       *    northing          : Northing (Y), in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Oblique Mercator projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Oblique Mercator projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X), in meters                  (input)
       *    northing          : Northing (Y), in meters                 (input)
       *    longitude         : Longitude (lambda), in radians          (output)
       *    latitude          : Latitude (phi), in radians              (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double es;
      double es_OVER_2;
      double OMerc_A;
      double OMerc_B;
      double OMerc_E;
      double OMerc_gamma;
      double OMerc_azimuth;                     /* Azimuth of central line as it crosses origin lat */
      double OMerc_Origin_Long;                 /* Longitude at center of projection */
      double cos_gamma;
      double sin_gamma;
      double sin_azimuth;  
      double cos_azimuth;
      double A_over_B;
      double B_over_A;
      double OMerc_u;                           /* Coordinates for center point (uc , vc), vc = 0 */
                                                /* at center lat and lon */
      /* Oblique Mercator projection Parameters */
      double OMerc_Origin_Lat;                  /* Latitude of projection center, in radians */
      double OMerc_Lat_1;                       /* Latitude of first point lying on central line */
      double OMerc_Lon_1;                       /* Longitude of first point lying on central line */
      double OMerc_Lat_2;                       /* Latitude of second point lying on central line */
      double OMerc_Lon_2;                       /* Longitude of second point lying on central line */
      double OMerc_Scale_Factor;                /* Scale factor at projection center */
      double OMerc_False_Northing;              /* False northing, in meters, at projection center */
      double OMerc_False_Easting;               /* False easting, in meters, at projection center */

      double OMerc_Delta_Northing;
      double OMerc_Delta_Easting;


      double omercT( double lat, double e_sinlat, double e_over_2 );

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

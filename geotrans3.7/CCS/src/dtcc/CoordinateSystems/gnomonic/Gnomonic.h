// CLASSIFICATION: UNCLASSIFIED

#ifndef Gnomonic_H
#define Gnomonic_H

/***************************************************************************/
/* RSC IDENTIFIER: GNOMONIC
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates
 *    (latitude and longitude in radians) and Gnomonic
 *    projection coordinates (easting and northing in meters).  This projection 
 *    employs a spherical Earth model.  The spherical radius used is the radius 
 *    of the sphere having the same area as the ellipsoid.
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values.  If an invalid value
 *    is found the error code is combined with the current error code using
 *    the bitwise or.  This combining allows multiple error codes to be
 *    returned. The possible error codes are:
 *
 *       GNOM_NO_ERROR           : No errors occurred in function
 *       GNOM_LAT_ERROR          : Latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       GNOM_LON_ERROR          : Longitude outside of valid range
 *                                     (-180 to 360 degrees)
 *       GNOM_EASTING_ERROR      : Easting outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       GNOM_NORTHING_ERROR     : Northing outside of valid range
 *                                     (depends on ellipsoid and projection
 *                                     parameters)
 *       GNOM_ORIGIN_LAT_ERROR   : Origin latitude outside of valid range
 *                                     (-90 to 90 degrees)
 *       GNOM_CENT_MER_ERROR     : Central meridian outside of valid range
 *                                     (-180 to 360 degrees)
 *       GNOM_A_ERROR            : Semi-major axis less than or equal to zero
 *       GNOM_INV_F_ERROR        : Inverse flattening outside of valid range
 *									                   (250 to 350)
 *
 *
 * REUSE NOTES
 *
 *    GNOMONIC is intended for reuse by any application that 
 *    performs a Gnomonic projection or its inverse.
 *
 * REFERENCES
 *
 *    Further information on GNOMONIC can be found in the Reuse Manual.
 *
 *    GNOMONIC originated from:     U.S. Army Topographic Engineering Center
 *                                  Geospatial Information Division
 *                                  7701 Telegraph Road
 *                                  Alexandria, VA  22310-3864
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    GNOMONIC has no restrictions.
 *
 * ENVIRONMENT
 *
 *    GNOMONIC was tested and certified in the following environments:
 *
 *    1. Solaris 2.5 with GCC, version 2.8.1
 *    2. MSDOS with MS Visual C++, version 6
 *
 * MODIFICATIONS
 *
 *    Date              Description
 *    ----              -----------
 *    05-22-00          Original Code
 *    03-05-07          Original C++ Code
 *    
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

    class Gnomonic : public CoordinateSystem
    {
    public:

      /*
       * The constructor receives the ellipsoid parameters and
       * projection parameters as inputs, and sets the corresponding state
       * variables.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (input)
       *    centralMeridian         : Longitude in radians at the center of     (input)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (input)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (input)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (input)
       */

	    Gnomonic( double ellipsoidSemiMajorAxis, double ellipsoidFlattening, double centralMeridian, double originLatitude, double falseEasting, double falseNorthing );


      Gnomonic( const Gnomonic &g );


	    ~Gnomonic( void );


      Gnomonic& operator=( const Gnomonic &g );


      /*
       * The function getParameters returns the current ellipsoid
       * parameters and Gnomonic projection parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid						        (output)
       *    centralMeridian         : Longitude in radians at the center of     (output)
       *                              the projection
       *    originLatitude          : Latitude in radians at which the          (output)
       *                              point scale factor is 1.0
       *    falseEasting            : A coordinate value in meters assigned to the
       *                              central meridian of the projection.       (output)
       *    falseNorthing           : A coordinate value in meters assigned to the
       *                              origin latitude of the projection         (output)
       */

      MapProjection4Parameters* getParameters() const;


      /*
       * The function convertFromGeodetic converts geodetic (latitude and
       * longitude) coordinates to Gnomonic projection (easting and northing)
       * coordinates, according to the current ellipsoid and Gnomonic projection
       * parameters.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    longitude         : Longitude (lambda) in radians       (input)
       *    latitude          : Latitude (phi) in radians           (input)
       *    easting           : Easting (X) in meters               (output)
       *    northing          : Northing (Y) in meters              (output)
       */

      MSP::CCS::MapProjectionCoordinates* convertFromGeodetic( MSP::CCS::GeodeticCoordinates* geodeticCoordinates );


      /*
       * The function convertToGeodetic converts Gnomonic projection
       * (easting and northing) coordinates to geodetic (latitude and longitude)
       * coordinates, according to the current ellipsoid and Gnomonic projection
       * coordinates.  If any errors occur, an exception is thrown with a description 
       * of the error.
       *
       *    easting           : Easting (X) in meters                  (input)
       *    northing          : Northing (Y) in meters                 (input)
       *    longitude         : Longitude (lambda) in radians          (output)
       *    latitude          : Latitude (phi) in radians              (output)
       */

      MSP::CCS::GeodeticCoordinates* convertToGeodetic( MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

    private:
    
      /* Ellipsoid Parameters, default to WGS 84 */
      double Ra;                /* Spherical Radius */
      double Sin_Gnom_Origin_Lat;
      double Cos_Gnom_Origin_Lat;

      /* Gnomonic projection Parameters */
      double Gnom_Origin_Lat;               /* Latitude of origin in radians */
      double Gnom_Origin_Long;              /* Longitude of origin in radians */
      double Gnom_False_Northing;           /* False northing in meters */
      double Gnom_False_Easting;            /* False easting in meters */
      double abs_Gnom_Origin_Lat;

      double Gnom_Delta_Northing;
      double Gnom_Delta_Easting;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

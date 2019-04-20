// CLASSIFICATION: UNCLASSIFIED

#ifndef TransverseMercator_H
#define TransverseMercator_H

/***************************************************************************/
/* RSC IDENTIFIER: TRANSVERSE MERCATOR
 *
 * ABSTRACT
 *
 *    This component provides conversions between Geodetic coordinates 
 *    (latitude and longitude) and Transverse Mercator projection coordinates
 *    (easting and northing).
 *
 * ERROR HANDLING
 *
 *    This component checks parameters for valid values. An exception is 
 *    returned for invalid parameters.
 *
 * REFERENCES
 *
 *    Based on NGA.SIG.0012_2.0.0_UTMUPS  25MAR2014
 *    "The Universal Grids and the Transverse Mercator and
 *     Polar Stereographic Map Projections"
 *
 * LICENSES
 *
 *    None apply to this component.
 *
 * RESTRICTIONS
 *
 *    TRANSVERSE MERCATOR has no restrictions.
 *
 * MODIFICATIONS
 *
 *    Date         Description
 *    ----         -----------
 *    2-26-07      Original C++ Code
 *    7-01-14      Updated algorithm in NGA.SIG.0012_2.0.0_UTMUPS.
 *
 */

#include "DtccApi.h"
#include "CoordinateSystem.h"

namespace MSP
{
   namespace CCS
   {
      class MapProjection5Parameters;
      class MapProjectionCoordinates;
      class GeodeticCoordinates;

      /**
       *  This class provides conversions between Geodetic coordinates 
       *  (latitude and longitude) and Transverse Mercator projection
       *  coordinates (easting and northing).
       */
      class MSP_DTCC_API TransverseMercator : public CoordinateSystem
      {
      public:

         /**
          * The constructor receives the ellipsoid parameters and 
          * Tranverse Mercator projection parameters as inputs, and
          * sets the corresponding state variables. If any errors occur,
          * an exception is thrown with a description of the error.
          *
          * 
          * @param ellipsoidSemiMajorAxis Semi-major ellipsoid axis (meters) 
          * @param  ellipsoidFlattening   Flattening of ellipsoid
          * @param  centralMeridian       Projection origin Longitude (radians)
          * @param  latitudeOfTrueScale   Projection origin Latitude (radians)
          * @param  falseEasting          Easting/X at projection center
          * @param  falseNorthing         Northing/Y at projection center
          * @param  scaleFactor           Projection scale factor
          */
         TransverseMercator(
            double ellipsoidSemiMajorAxis,
            double ellipsoidFlattening,
            double centralMeridian,
            double latitudeOfTrueScale,
            double falseEasting,
            double falseNorthing,
            double scaleFactor,
            char  *ellipsoidCode);

         /**
          * Copy Constructor
          */
         TransverseMercator( const TransverseMercator &tm );

         /**
          * Destructor
          */
         ~TransverseMercator( void );

         /**
          * Equals operator
          */
         TransverseMercator& operator=( const TransverseMercator &tm );

         /**
          * The function getParameters returns the current ellipsoid 
          * and Transverse Mercator projection parameters.
          * @return Transverse Mercator projection parameters.
          */
         MapProjection5Parameters* getParameters() const;

         /**
          * Converts geodetic (latitude and longitude) coordinates to 
          * Transverse Mercator projection (easting and northing) coordinates,
          * according to the current ellipsoid and Transverse Mercator
          * projection coordinates.
          *
          * @param   longitude   Longitude in radians    (input)
          * @param   latitude    Latitude in radians     (input)
          * @param   easting     Easting/X in meters     (output)
          * @param   northing    Northing/Y in meters    (output)
          */
         MSP::CCS::MapProjectionCoordinates* convertFromGeodetic(
            MSP::CCS::GeodeticCoordinates* geodeticCoordinates );

         /*
          * Converts Transverse Mercator projection (easting and northing)
          * coordinates to geodetic (latitude and longitude)
          * coordinates, according to the current ellipsoid and Transverse
          * Mercator projection parameters.
          *
          *  @param   easting     Easting/X in meters         (input)
          *  @param   northing    Northing/Y in meters        (input)
          *  @param   longitude   Longitude in radians        (output)
          *  @param   latitude    Latitude in radians         (output)
          */
         MSP::CCS::GeodeticCoordinates* convertToGeodetic(
            MSP::CCS::MapProjectionCoordinates* mapProjectionCoordinates );

      private:
    
         /* Ellipsoid Parameters */
         char   ellipsCode[3];            // 2 Letter ellipsoid code
         
         double TranMerc_eps;             // Eccentricity

         double TranMerc_K0R4;            // SCALE_FACTOR*R4
         double TranMerc_K0R4inv;         // 1/(SCALE_FACTOR*R4)

         double TranMerc_aCoeff[8];
         double TranMerc_bCoeff[8];

         /* Transverse_Mercator projection Parameters */
         double TranMerc_Origin_Lat;       // Latitude of origin in radians
         double TranMerc_Origin_Long;      // Longitude of origin in radians
         double TranMerc_False_Northing;   // False northing in meters
         double TranMerc_False_Easting;    // False easting in meters
         double TranMerc_Scale_Factor;     // Scale factor 

         /* Maximum variance for easting and northing values */
         double TranMerc_Delta_Easting;
         double TranMerc_Delta_Northing;

         /**
          * Basic conversion without regard to false easting/northing or origin
          */
         void latLonToNorthingEasting( 
            const double &lat,
            const double &lon,
            double       &northing,
            double       &easting );

         /**
          * Basic conversion without regard to false easting/northing or origin
          */
         void northingEastingToLatLon( 
            const double &northing,
            const double &easting,
            double       &latitude,
            double       &longitude );

         /**
          * Generate coefficients for trig series.
          * 
          */
         static void generateCoefficients(
            double  invfla,
            double &n1,
            double  Acoeff[8],
            double  Bcoeff[8],
            double &R4oa,
            char *ellipsoidCode);

         /**
          * Check if latitude and longitude are in valid range.
          * Note that deltaLon is longitude - longitude of central meridian
          */
         static void checkLatLon( double latitude, double deltaLon );

         /**
          * Hyperbolic arc tangent.
          */
         static double aTanH( double x );
         
         /**
          * Convert conformal latitude to geodetic latitude.
          */
         static double geodeticLat(
            double sinChi,
            double e );

         /**
          * Use trig identities to compute
          * c2kx[k] = cosh(2kX), s2kx[k] = sinh(2kX)   for k = 0 .. 8
          */
         static void computeHyperbolicSeries(
            double twoX,
            double c2kx[],
            double s2kx[]);

         /**
          * Use trig identities to compute
          * c2ky[k] = cos(2kY), s2ky[k] = sin(2kY)   for k = 0 .. 8
          */
         void computeTrigSeries(
            double twoY,
            double c2ky[],
            double s2ky[]);
 
      };
   }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED

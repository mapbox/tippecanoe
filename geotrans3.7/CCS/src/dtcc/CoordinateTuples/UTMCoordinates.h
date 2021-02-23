// CLASSIFICATION: UNCLASSIFIED

#ifndef UTMCoordinates_H
#define UTMCoordinates_H

#include "CoordinateTuple.h"

#include "DtccApi.h"


namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API UTMCoordinates : public CoordinateTuple
    {
    public:

      UTMCoordinates();
      UTMCoordinates( CoordinateType::Enum _coordinateType );
      UTMCoordinates( CoordinateType::Enum _coordinateType, long __zone, char __hemisphere, double __easting, double __northing );
      UTMCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, long __zone, char __hemisphere, double __easting, double __northing );
      UTMCoordinates( const UTMCoordinates& c );

      ~UTMCoordinates();

      UTMCoordinates& operator=( const UTMCoordinates &c );

      void set( long __zone, char __hemisphere, double __easting, double __northing );

      void setZone( long __zone );
      long zone() const;
      void setHemisphere( char __hemisphere );
      char hemisphere() const;
      void setEasting( double __easting );
      double easting() const;
      void setNorthing( double __northing );
      double northing() const;

    private:

      long _zone;
      char _hemisphere;
      double _easting;
      double _northing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED

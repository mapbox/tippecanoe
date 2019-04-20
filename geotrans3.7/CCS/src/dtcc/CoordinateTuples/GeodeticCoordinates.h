// CLASSIFICATION: UNCLASSIFIED

#ifndef GeodeticCoordinates_H
#define GeodeticCoordinates_H

#include "CoordinateTuple.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API GeodeticCoordinates : public CoordinateTuple
    {
    public:

      GeodeticCoordinates();
      GeodeticCoordinates( CoordinateType::Enum _coordinateType );
      GeodeticCoordinates( CoordinateType::Enum _coordinateType, double __longitude, double __latitude, double __height = 0 );
      GeodeticCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, double __longitude, double __latitude, double __height = 0 );
      GeodeticCoordinates( const GeodeticCoordinates& gc );

      ~GeodeticCoordinates();

      GeodeticCoordinates& operator=( const GeodeticCoordinates &gc );

      void set( double __longitude, double __latitude, double __height = 0 );
      void setLongitude( double __longitude );
      void setLatitude( double __latitude );
      void setHeight( double __height );

      double longitude() const;
      double latitude() const;
      double height() const;

    private:

      double _longitude;
      double _latitude;
      double _height;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED

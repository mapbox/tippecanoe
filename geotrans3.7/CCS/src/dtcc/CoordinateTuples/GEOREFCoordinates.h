// CLASSIFICATION: UNCLASSIFIED

#ifndef GEOREFCoordinates_H
#define GEOREFCoordinates_H

#include "CoordinateTuple.h"
#include "Precision.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API GEOREFCoordinates : public CoordinateTuple
    {
    public:

      GEOREFCoordinates();
      GEOREFCoordinates( CoordinateType::Enum _coordinateType );
      GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __GEOREFString );
      GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __GEOREFString, Precision::Enum __precision );
      GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __GEOREFString, Precision::Enum __precision );
      GEOREFCoordinates( const GEOREFCoordinates& gc );

      ~GEOREFCoordinates();

      GEOREFCoordinates& operator=( const GEOREFCoordinates &gc );

      void set( char __GEOREFString[21] );

      char* GEOREFString();

      Precision::Enum precision() const;

    private:

      char _GEOREFString[21];
      Precision::Enum _precision;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED

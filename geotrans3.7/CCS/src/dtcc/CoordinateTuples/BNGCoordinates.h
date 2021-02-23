// CLASSIFICATION: UNCLASSIFIED

#ifndef BNGCoordinates_H
#define BNGCoordinates_H

#include "CoordinateTuple.h"
#include "Precision.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API BNGCoordinates : public CoordinateTuple
    {
    public:

      BNGCoordinates();
      BNGCoordinates( CoordinateType::Enum _coordinateType );
      BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __BNGString );
      BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __BNGString, Precision::Enum __precision );
      BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __BNGString, Precision::Enum __precision );
      BNGCoordinates( const BNGCoordinates &b );

      ~BNGCoordinates();

      BNGCoordinates& operator=( const BNGCoordinates &b );

      void set( char __BNGString[21] );

      char* BNGString();

      Precision::Enum precision() const;

    private:

      char _BNGString[21];
      Precision::Enum _precision;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED

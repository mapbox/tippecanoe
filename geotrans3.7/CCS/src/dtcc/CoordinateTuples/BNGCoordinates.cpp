// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "BNGCoordinates.h"


using namespace MSP::CCS;


BNGCoordinates::BNGCoordinates() :
  CoordinateTuple( CoordinateType::britishNationalGrid ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _BNGString, "SV 0000000000", 20 );
}


BNGCoordinates::BNGCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _BNGString, "SV 0000000000", 20 );
}


BNGCoordinates::BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __BNGString ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _BNGString, __BNGString, 20 );
  _BNGString[20] = '\0';
}


BNGCoordinates::BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __BNGString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _BNGString, __BNGString, 20 );
  _BNGString[20] = '\0';
}


BNGCoordinates::BNGCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __BNGString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _BNGString, __BNGString, 20 );
  _BNGString[20] = '\0';

  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


BNGCoordinates::BNGCoordinates( const BNGCoordinates &c )
{
  _coordinateType = c._coordinateType;

  strncpy( _BNGString, c._BNGString, 20 );
  _BNGString[20] = '\0';

  _precision = c._precision;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


BNGCoordinates::~BNGCoordinates()
{
}


BNGCoordinates& BNGCoordinates::operator=( const BNGCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    strncpy( _BNGString, c._BNGString, 20 );
    _BNGString[20] = '\0';

    _precision = c._precision;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void BNGCoordinates::set( char __BNGString[21] )
{
  strncpy( _BNGString, __BNGString, 20 );
  _BNGString[20] = '\0';
}

  
char* BNGCoordinates::BNGString()
{
  return _BNGString;
}


Precision::Enum BNGCoordinates::precision() const
{
   Precision::Enum temp_precision = _precision;

   if (temp_precision < 0)
      temp_precision = Precision::degree;
   if (temp_precision > 5)
      temp_precision = Precision::tenthOfSecond;

  return temp_precision;
}

// CLASSIFICATION: UNCLASSIFIED

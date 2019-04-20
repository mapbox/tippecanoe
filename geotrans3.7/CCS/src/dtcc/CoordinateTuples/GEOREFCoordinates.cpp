// CLASSIFICATION: UNCLASSIFIED

#include <string.h>
#include "GEOREFCoordinates.h"


using namespace MSP::CCS;


GEOREFCoordinates::GEOREFCoordinates() :
  CoordinateTuple( CoordinateType::georef ),
  _precision( Precision::tenthOfSecond )
{
  strncpy(_GEOREFString, "NGAA0000000000", 20);
}


GEOREFCoordinates::GEOREFCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy(_GEOREFString, "NGAA0000000000", 20);
}


GEOREFCoordinates::GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __GEOREFString ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy(_GEOREFString, __GEOREFString, 20);
  _GEOREFString[20] = '\0';
}


GEOREFCoordinates::GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __GEOREFString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy(_GEOREFString, __GEOREFString, 20);
  _GEOREFString[20] = '\0';
}


GEOREFCoordinates::GEOREFCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __GEOREFString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy(_GEOREFString, __GEOREFString, 20);
  _GEOREFString[20] = '\0';

  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GEOREFCoordinates::GEOREFCoordinates( const GEOREFCoordinates &c )
{
  _coordinateType = c._coordinateType;

  strncpy(_GEOREFString, c._GEOREFString, 20);
  _GEOREFString[20] = '\0';

  _precision = c._precision;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GEOREFCoordinates::~GEOREFCoordinates()
{
}


GEOREFCoordinates& GEOREFCoordinates::operator=( const GEOREFCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    strncpy(_GEOREFString, c._GEOREFString, 20);
    _GEOREFString[20] = '\0';

    _precision = c._precision;
    
    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void GEOREFCoordinates::set( char __GEOREFString[21] )
{
  strncpy(_GEOREFString, __GEOREFString, 20);
  _GEOREFString[20] = '\0';
}


char* GEOREFCoordinates::GEOREFString()
{
  return _GEOREFString;
}


Precision::Enum GEOREFCoordinates::precision() const
{
   Precision::Enum temp_precision = _precision;

   if (temp_precision < 0)
      temp_precision = Precision::degree;
   if (temp_precision > 5)
      temp_precision = Precision::tenthOfSecond;

  return temp_precision;
}

// CLASSIFICATION: UNCLASSIFIED

// CLASSIFICATION: UNCLASSIFIED

#include <stdio.h>
#include <string.h>
#include "GARSCoordinates.h"


using namespace MSP::CCS;


GARSCoordinates::GARSCoordinates() :
  CoordinateTuple( CoordinateType::globalAreaReferenceSystem ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _GARSString, "361HN37", 7 );
}


GARSCoordinates::GARSCoordinates( CoordinateType::Enum _coordinateType ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _GARSString, "361HN37", 7 );
}


GARSCoordinates::GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __GARSString ) :
  CoordinateTuple( _coordinateType ),
  _precision( Precision::tenthOfSecond )
{
  strncpy( _GARSString, __GARSString, 7 );
  _GARSString[7] = '\0';
}


GARSCoordinates::GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __GARSString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _GARSString, __GARSString, 7 );
  _GARSString[7] = '\0';
}


GARSCoordinates::GARSCoordinates( CoordinateType::Enum _coordinateType, const char* __warningMessage, const char* __GARSString, Precision::Enum __precision ) :
  CoordinateTuple( _coordinateType ),
  _precision( __precision )
{
  strncpy( _GARSString, __GARSString, 7 );
  _GARSString[7] = '\0';

  int length = strlen( __warningMessage );
  strncpy( _warningMessage, __warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GARSCoordinates::GARSCoordinates( const GARSCoordinates &c )
{
  _coordinateType = c._coordinateType;

  strncpy( _GARSString, c._GARSString, 7 );
  _GARSString[7] = '\0';

  _precision = c._precision;

  int length = strlen( c._warningMessage );
  strncpy( _warningMessage, c._warningMessage, length );
  _warningMessage[ length ] = '\0';
}


GARSCoordinates::~GARSCoordinates()
{
}


GARSCoordinates& GARSCoordinates::operator=( const GARSCoordinates &c )
{
  if( this != &c )
  {
    _coordinateType = c._coordinateType;

    strncpy( _GARSString, c._GARSString, 7 );
    _GARSString[7] = '\0';

    _precision = c._precision;

    int length = strlen( c._warningMessage );
    strncpy( _warningMessage, c._warningMessage, length );
    _warningMessage[ length ] = '\0';
  }

  return *this;
}


void GARSCoordinates::set( char __GARSString[8] )
{
  strncpy( _GARSString, __GARSString, 7 );
  _GARSString[7] = '\0';
}


char* GARSCoordinates::GARSString()
{
  return _GARSString;
}


Precision::Enum GARSCoordinates::precision() const
{
   Precision::Enum temp_precision = _precision;

   if (temp_precision < 0)
      temp_precision = Precision::degree;
   if (temp_precision > 5)
      temp_precision = Precision::tenthOfSecond;

  return temp_precision;
}

// CLASSIFICATION: UNCLASSIFIED

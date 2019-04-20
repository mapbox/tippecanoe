// CLASSIFICATION: UNCLASSIFIED

#include <stdio.h>
#include <string.h>
#include "CoordinateTuple.h"
#include "CoordinateType.h"


using namespace MSP::CCS;


CoordinateTuple::CoordinateTuple() :
  _coordinateType( CoordinateType::geodetic )
{
  strcpy( _errorMessage, "");
  strcpy( _warningMessage, "");
//  _warningMessage[0] = '\0';
}


CoordinateTuple::CoordinateTuple( CoordinateType::Enum __coordinateType ) :
  _coordinateType( __coordinateType )
{
  strcpy( _errorMessage, "");
  strcpy( _warningMessage, "");
 // _warningMessage[0] = '\0';
}


CoordinateTuple::CoordinateTuple( MSP::CCS::CoordinateType::Enum __coordinateType, const char* __warningMessage ) :
  _coordinateType( __coordinateType )
{
  strcpy( _errorMessage, "");

 // _warningMessage[0] = '\0';
  strcpy( _warningMessage, __warningMessage ); 
}


CoordinateTuple::CoordinateTuple( const CoordinateTuple &ct )
{
  _coordinateType = ct._coordinateType;

 // _errorMessage[0] = '\0';
  strcpy( _errorMessage, ct._errorMessage ); 

 // _warningMessage[0] = '\0';
  strcpy( _warningMessage, ct._warningMessage ); 
}


CoordinateTuple::~CoordinateTuple()
{
 // if( strlen( _warningMessage ) > 0 )
 //   delete [] _warningMessage;
}


CoordinateTuple& CoordinateTuple::operator=( const CoordinateTuple &ct )
{
  if( this != &ct )
  {
    _coordinateType = ct._coordinateType;

  //  _errorMessage[0] = '\0';
    strcpy( _errorMessage, ct._errorMessage ); 

  // _warningMessage[0] = '\0';
    strcpy( _warningMessage, ct._warningMessage ); 
  }

  return *this;
}


void CoordinateTuple::setCoordinateType( MSP::CCS::CoordinateType::Enum __coordinateType )
{
  _coordinateType = __coordinateType;
}


CoordinateType::Enum CoordinateTuple::coordinateType() const
{
  return _coordinateType;
}


void CoordinateTuple::set(MSP::CCS::CoordinateType::Enum __coordinateType, const char* __warningMessage, const char* __errorMessage)
{
  _coordinateType = __coordinateType;
  strcpy( _warningMessage, __warningMessage ); 
  strcpy( _errorMessage, __errorMessage ); 
}


void CoordinateTuple::setErrorMessage( const char* __errorMessage )
{
 // _errorMessage[0] = '\0';
  strcpy( _errorMessage, __errorMessage ); 
}


const char* CoordinateTuple::errorMessage() const
{
  return _errorMessage;
}


void CoordinateTuple::setWarningMessage( const char* __warningMessage )
{
 // _warningMessage[0] = '\0';
  strcpy( _warningMessage, __warningMessage ); 
}


const char* CoordinateTuple::warningMessage() const
{
  return _warningMessage;
}



// CLASSIFICATION: UNCLASSIFIED

// CLASSIFICATION: UNCLASSIFIED
#include "ErrorMessages.h"

using namespace MSP::CCS;

const char* ErrorMessages::geoidFileOpenError = "Unable to locate geoid data file\n";
const char* ErrorMessages::geoidFileParseError = "Unable to read geoid file\n";

const char* ErrorMessages::ellipsoidFileOpenError = "Unable to locate ellipsoid data file: ellips.dat\n";
const char* ErrorMessages::ellipsoidFileCloseError = "Unable to close ellipsoid file: ellips.dat\n";
const char* ErrorMessages::ellipsoidFileParseError = "Unable to read ellipsoid file: ellips.dat\n";
const char* ErrorMessages::ellipse = "Ellipsoid library not initialized\n";
const char* ErrorMessages::invalidEllipsoidCode = "Invalid ellipsoid code\n";

const char* ErrorMessages::datumFileOpenError = "Unable to locate datum data file\n";
const char* ErrorMessages::datumFileCloseError = "Unable to close datum file\n";
const char* ErrorMessages::datumFileParseError = "Unable to read datum file\n";
const char* ErrorMessages::datumDomain = "Invalid local datum domain of validity\n";
const char* ErrorMessages::datumRotation = "Rotation values must be between -60.0 and 60.0";
const char* ErrorMessages::datumSigma = "Standard error values must be positive, or -1 if unknown\n";
const char* ErrorMessages::datumType = "Invalid datum type\n";
const char* ErrorMessages::invalidDatumCode = "Invalid datum code\n";

const char* ErrorMessages::notUserDefined = "Specified code not user defined\n";
const char* ErrorMessages::ellipseInUse = "Ellipsoid is in use by a datum\n";

// Parameter error messages
const char* ErrorMessages::semiMajorAxis     = "Ellipsoid semi-major axis must be greater than zero\n";
const char* ErrorMessages::ellipsoidFlattening = "Inverse flattening must be between 250 and 350\n";
const char* ErrorMessages::orientation       = "Orientation out of range\n";
const char* ErrorMessages::originLatitude    = "Origin Latitude (or Standard Parallel or Latitude of True Scale) out of range\n";
const char* ErrorMessages::originLongitude   = "Origin Longitude (or Longitude Down from Pole) out of range\n";
const char* ErrorMessages::centralMeridian   = "Central Meridian out of range\n";
const char* ErrorMessages::scaleFactor       = "Scale Factor out of range\n";
const char* ErrorMessages::zone              = "Invalid Zone\n";
const char* ErrorMessages::zoneOverride      = "Invalid Zone Override\n";
const char* ErrorMessages::standardParallel1 = "Invalid 1st Standard Parallel\n";
const char* ErrorMessages::standardParallel2 = "Invalid 2nd Standard Parallel\n";
const char* ErrorMessages::standardParallel1_2 = "1st & 2nd Standard Parallels cannot both be zero\n";
const char* ErrorMessages::standardParallelHemisphere = "Standard Parallels cannot be equal and opposite latitudes\n";
const char* ErrorMessages::precision        = "Precision must be between 0 and 5\n";
const char* ErrorMessages::bngEllipsoid     = "British National Grid ellipsoid must be Airy\n";
const char* ErrorMessages::nzmgEllipsoid    = "New Zealand Map Grid ellipsoid must be International\n";
const char* ErrorMessages::webmEllipsoid    = "Web Mercator ellipsoid must be WGS84\n";
const char* ErrorMessages::webmConversionTo = "GeoTrans does not allow conversion to Web Mercator\n";
const char* ErrorMessages::webmInvalidTargetCS = "Web Mercator can only be converted to Geodetic.\n";
const char* ErrorMessages::latitude1        = "Latitude 1 out of range\n";
const char* ErrorMessages::latitude2        = "Latitude 2 out of range\n";
const char* ErrorMessages::latitude1_2      = "Latitude 1 and Latitude 2 cannot be equal\n";
const char* ErrorMessages::longitude1       = "Longitude 1 out of range\n";
const char* ErrorMessages::longitude2       = "Longitude 2 out of range\n";
const char* ErrorMessages::omercHemisphere  = "Point 1 and Point 2 cannot be in different hemispheres\n";
const char* ErrorMessages::hemisphere       = "Invalid Hemisphere\n";
const char* ErrorMessages::radius           = "Easting/Northing too far from center of projection\n";


// Coordinate error messages
const char* ErrorMessages::latitude     = "Latitude out of range\n";
const char* ErrorMessages::longitude    = "Longitude out of range\n";
const char* ErrorMessages::easting      = "Easting/X out of range\n";
const char* ErrorMessages::northing     = "Northing/Y out of range\n";
const char* ErrorMessages::projection   = "Point projects into a circle\n";
const char* ErrorMessages::invalidArea  = "Coordinates are outside valid area\n";
const char* ErrorMessages::bngString    = "Invalid British National Grid String\n";
const char* ErrorMessages::garsString   = "Invalid GARS String\n";
const char* ErrorMessages::georefString = "Invalid GEOREF String\n";
const char* ErrorMessages::mgrsString   = "Invalid MGRS String\n";
const char* ErrorMessages::usngString   = "Invalid USNG String\n";

const char* ErrorMessages::invalidIndex = "Index value outside of valid range\n";
const char* ErrorMessages::invalidName  = "Invalid name\n";
const char* ErrorMessages::invalidType  = "Invalid coordinate system type\n";

const char* ErrorMessages::longitude_min = "The longitude minute part of the string is greater than 60\n";
const char* ErrorMessages::latitude_min  = "The latitude minute part of the string is greater than 60\n";

// CLASSIFICATION: UNCLASSIFIED

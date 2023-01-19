#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sqlite3.h>
#include <time.h>

// Defintion for PI.
#define PI 3.1415926535897932384626433832795

// Sun's zenith in degrees.
// For sunrise and sunset calculations a 
// degree of 0.833 is assumed which 
// accounts for atmospheric refraction.
#define ZENITH -.83

// Defined latitude and longitude.
// I only want my own personal position.
#define LATITUDE 50.000000
#define LONGITUDE 11.00000

// First and last month for date calculation.
#define FIRSTMONTH 1
#define LASTMONTH 12


/*
** Calculate the sunrise or sunset for given date
** and location provided by latitude and longitude.
**
** INFO: The longitude is positive for East and negative for West.
** INFO: The trigonemtry functions are using degree instead of radian.
**       Factor is 180/PI.
** INFO: 
*/
static double calc_sun_time(
  int year, int month, int day, 
  double lat, double lng, 
  int localOffset, 
  int daylightSavings, 
  int settingTime) 
{

  // Calculate day of the year.
  double N1 = floor(275*month/9);
  double N2 = floor((month+9)/12);
  double N3 = (1+floor((year-4*floor(year/4)+2)/3));
  double N = N1-(N2*N3)+day-30;

  // Convert longitude to hour value and approximate time.
  double lngHour = lng/15.0;
  double t = 0.0;
  if (settingTime) {
    t = N+((6-lngHour)/24);
  } /* calculate rising time.  */
  else {
    t = N+((18-lngHour)/24);
  } /* calculate setting time. */

  // Calculate sun's mean anomaly.
  double M = (0.9856*t)-3.289;

  // Calculate sun's true longitude.
  double L = fmod(M+(1.916*sin((PI/180)*M)) + (0.020 * sin(2*(PI/180)*M)) + 282.634, 360.0);

  // Calculate sun's right ascension.
  double RA = fmod(180/PI*atan(0.91764*tan((PI/180)*L)), 360.0);

  // Right ascension value need to be in the quadrant as L.
  double Lquadrant = floor(L/90)*90;
  double RAquadrant = floor(RA/90)*90;
  RA = RA + (Lquadrant-RAquadrant);

  // Right ascension value needs to be converted to hours.
  RA = RA / 15;

  // Calculate the sun's declination.
  double sinDec = 0.39782*sin((PI/180)*L);
  double cosDec = cos(asin(sinDec));

  // Calculate the sun's local hour angle.
  double cosH = (sin((PI/180)*ZENITH)-(sinDec*sin((PI/180)*lat))) / (cosDec*cos((PI/180)*lat));
  
  double H = .0f;
  if (settingTime) {
    H = 360-(180/PI)*acos(cosH);
  } /* rising */
  else {
    H = (180/PI)*acos(cosH);
  } /* setting */
  H = H/15;

  // Calculate local mean time of rising/setting.
  double T = H+RA-(0.06571*t)-6.622;

  // Adjust back to UTC;
  double UT = fmod(T-lngHour, 24.0);

  return UT + localOffset + daylightSavings;
}


/*
** Get the amount of days for a given month.
** The year parameter is used to check for leap years
** and the amount of days in february.
*/
static int days_in_month(int year, int month) {

  if (month < 1 || month > 12) {
    return 0;
  }

  if (month == 1 || month == 3 
  || month == 5 || month == 7 
  || month == 8 || month == 10 
  || month == 12)   
  {
    return 31;
  }
  else if (month == 2) {
    if ((year %   4 == 0)
    &&  (year % 100 != 0)
    &&  (year % 400 == 0)) 
    {
      return 29;
    } /* Leap year. */
    else {
      return 28;
    } /* Not a leap year. */
  }
  else if (month == 4 || month == 6 
  || month == 9 || month == 11) 
  {
    return 30;
  }
  return 0;
}


/*
** Get the day of the week.
** Values range from 0 to 6;
*/
static int day_of_week(int year, int month, int day) {

  int dow = (day \
  + ((153 * (month+12*((14-month)/12)-3)+2)/5) \
  + (365*(year+4800-((14-month)/12)))          \
  + ((year+4800-((14-month)/12))/  4)          \
  - ((year+4800-((14-month)/12))/100)          \
  + ((year+4800-((14-month)/12))/400)          \
  - 32045
  ) % 7;
  return dow;
}


/*
** Check if provided date falls into the range
** of the central european daylight savings time.
*/
static int is_central_europe_dst(int year, int month, int day) {

  if (month < 3 || month > 10) {
    return 0;
  }

  if (month > 3 && month < 10) {
    return 1;
  }

  int ps = day - day_of_week(year, month, day);
  if (month == 3) {
    return (int)(ps >= 25);
  }

  if (month == 10) {
    return (int)(ps < 25);
  }
  return 0;
}


/*
** Gets the time of the provided day and location
** when the sun rises.
*/
static struct tm get_sunrise(
  int year, int month, int day, 
  double lat, double lng, 
  int offset, 
  int dst) 
{
  double localtime = fmod(24+calc_sun_time(year, month, day, lat, lng, offset, dst, -1), 24);
  // Precision of double is required
  // to successfully convert to hours
  // and minutes. 
  // Hours and minutes are explicitly converted
  // to int later on.
  double hours;
  double minutes = modf(localtime, &hours)*60;
  struct tm sunrise = {
    .tm_year= year-1900,
    .tm_mday = day,
    .tm_mon = month-1,
    .tm_min = (int)minutes,
    .tm_hour = (int)hours,
    .tm_isdst = dst
  };
  return sunrise;
}


/*
** Gets the time of the provided day and location
** when the sun sets.
*/
static struct tm get_sunset(
  int year, int month, int day, 
  double lat, double lng, 
  int offset, 
  int dst) 
{
  double localtime = fmod(24+calc_sun_time(year, month, day, lat, lng, offset, dst, 0), 24);
  // Precision of double is required
  // to successfully convert to hours
  // and minutes. 
  // Hours and minutes are explicitly converted
  // to int later on.
  double hours;
  double minutes = modf(localtime, &hours)*60;
  struct tm sunset = {
    .tm_year= year-1900,
    .tm_mday = day,
    .tm_mon = month-1,
    .tm_min = (int)minutes,
    .tm_hour = (int)hours,
    .tm_isdst = dst
  };
  return sunset;
}


/*
** Creates a tm struct for the provided date.
*/
static struct tm get_date(int year, int month, int day, int dst) {
  struct tm tmday = {
    .tm_year = year-1900,
    .tm_mday = day,
    .tm_mon = month-1,
    .tm_min = 0,
    .tm_hour = 0,
    .tm_isdst = dst
  };
  return tmday;
}


/*
** Prepare new SQL statement.
*/
static void format_insert(
  sqlite3_stmt *stmt,
  struct tm *date, 
  struct tm *sunrise, 
  struct tm *sunset, 
  double lat, 
  double lng, 
  int localOffset, 
  int daylightSavings) 
{
  // Length of date formatted (yyyy-MM-dd\n);
  const size_t LEN_DATEFORMAT = 11;
  // Length of time formatted (HH:mm\n);
  const size_t LEN_TIMEFORMAT =  6;

  // Write date into statement.
  char date_buff[LEN_DATEFORMAT];
  strftime(date_buff, LEN_DATEFORMAT, "%Y-%m-%d", date);
  sqlite3_bind_text(stmt, 1, date_buff, (int)strlen(date_buff), 0);
  // Write time of sunrise into statement.
  char sunrise_buff[LEN_TIMEFORMAT];
  strftime(sunrise_buff, LEN_TIMEFORMAT, "%H:%M", sunrise);
  sqlite3_bind_text(stmt, 2, sunrise_buff, (int)strlen(sunrise_buff), 0);
  // Write time of sunset into statement.
  char sunset_buff[LEN_TIMEFORMAT];
  strftime(sunset_buff, LEN_TIMEFORMAT, "%H:%M", sunset);
  sqlite3_bind_text(stmt, 3, sunset_buff, (int)strlen(sunset_buff), 0);
  // Write latitute, longitude, utc offset 
  // and dst into statement.
  sqlite3_bind_double(stmt, 4, lat);
  sqlite3_bind_double(stmt, 5, lng);
  sqlite3_bind_int(stmt, 6, localOffset);
  sqlite3_bind_int(stmt, 7, daylightSavings);
}


/*
** Calculates the time of the sunrise and sunset
** for each day of the year and for each year 
** within the range 'from' and 'until'.
**
** NOTE: insert_template must be a 
**       generic SQLite compliant insert command.
** NOTE: code does not yet check/account for dates
**       below the year 1900.
** NOTE: timezone is the local offset to UTC.
** NOTE: longitude and latitude are defined and 
**       need to be adjusted.
*/
static int insert_entries(
  char *db_path, 
  char *insert_template, 
  int from, int until, 
  int timezone) 
{

  sqlite3 *db;
  char *zErrMsg = 0;
  int rc;

  rc = sqlite3_open(db_path, &db);
  if (rc) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      sqlite3_close(db);
      return (0);
  }

  // Prepare insert for sqlite3_step().
  sqlite3_stmt *stmt;
  rc = sqlite3_prepare(db, insert_template, -1, &stmt, NULL);
  if (SQLITE_OK != rc) {
    fprintf(stderr, "Unable to prepare insert statement.");
    sqlite3_close(db);
    return (0);
  }

  for (int y=from; y<=until; y++) {
    rc = sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, &zErrMsg);
    if ( rc ) {
      fprintf(stderr, "Unable to start transaction: %s\n", sqlite3_errmsg(db));
      // Free error message memory.
      sqlite3_free(zErrMsg);
      // Finalize and close.
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return (0);
    }
    for (int m=FIRSTMONTH; m<=LASTMONTH; m++) {
      int days = days_in_month(y, m);
      if ( !days ) {
        fprintf(stderr, "Unable to get days for month %d.\n", m);
        continue;
      }
      for (int d=1; d<=days; d++) {
        int ds = is_central_europe_dst(y, m, d);
        struct tm current = get_date(y, m, d, ds);
        struct tm sunrise = get_sunrise(y, m, d, LATITUDE, LONGITUDE, timezone, ds);
        struct tm sunset = get_sunset(y, m, d, LATITUDE, LONGITUDE, timezone, ds);
        format_insert(stmt, &current, &sunrise, &sunset, LATITUDE, LONGITUDE, timezone, ds);
        int retVal = sqlite3_step(stmt);
        if (SQLITE_DONE != retVal) {
          fprintf(stderr, "Failed to execute insert. %d\n", retVal);
        }        
        sqlite3_reset(stmt);
      }
    }
    rc = sqlite3_exec(db, "COMMIT TRANSACTION;", NULL, NULL, &zErrMsg);
    if ( rc ) {
      fprintf(stderr, "Unable to commit transaction: %s\n", sqlite3_errmsg(db));
      // Free error message memory.
      sqlite3_free(zErrMsg);
      // FInalize and close.
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return (0);
    }
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return (1);
}


/*
** Read the files contents and return them.
*/
static char * read_file(const char *fname) {

  char *buffer = 0;
  FILE * fp = fopen(fname, "rb");

  if ( NULL==fp ) {    
    return 0;
  }
  else {
    long len;
    // Go to end of file to get the length
    // of the file using ftell().
    int success = fseek(fp, 0L, SEEK_END);
    if (0 != success) {
      return 0;
    }
    len = ftell(fp);
    if (0 >= len) {
      return 0;
    }
    // Return to start of file.
    fseek(fp, 0L, SEEK_SET);
    // Initialze null-terminated buffer.
    size_t bufLen = (size_t)(len + 1);
    buffer = calloc(bufLen, sizeof(char));
    if (buffer) {
      fread(buffer, sizeof(char), bufLen, fp);
      int err = ferror(fp);
      // Check if read error occured.
      if (err) {
        fprintf(stderr, "Error [%d] while reading file %s in %s\n", err, fname, __func__);
        fclose(fp);
        return 0;
      }
    }    
    fclose(fp);
  }
  return buffer;
}


/*
** Tries to open a SQLite database connection
** with the given database path.
** Returns 0 on error and 1 on success.
*/
static int test_database(char * db_path) {
  sqlite3 *db;
  int rc;

  rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
  if ( rc ) {
    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    return 0;
  }
  sqlite3_close(db);
  return 1;
}


/*
** Get n-days and store the sunrise and sunset time for these 
** days, for a specific location, within a SQLite database.
*/
int main(int argc, char *argv[]) {

  if (argc < 5) {
    fprintf(stderr, "Insufficient amount of arguments..\n");
    return 0;    
  }

  if (argc > 6) {
    fprintf(stderr, "Too many arguments (required %i-%i)", 5, 6);
    return 0;
  }
  
  char *db_path = argv[1];
  if ( !test_database(db_path) ) {
    fprintf(stderr, "Unable to open or locate SQLite database with path %s\n", db_path);
    return 0;
  }
  
  char *fname = argv[2];
  char *template = read_file(fname);
  if ( !template ) {
    fprintf(stderr, "Unable to load INSERT template from file %s.\n", fname);
    return 0;
  }

  int from;
  if ( sscanf(argv[3], "%i", &from)!=1 ) {
    fprintf(stderr, "Expected an integer for the start date.\n");
  }

  int until;
  if ( sscanf(argv[4], "%i", &until)!=1 ) {
    fprintf(stderr, "Expected an integer for the end date.\n");
  }

  if (0 == from || 0 == until) {
    fprintf(stderr, "Start year or end year is invalid - start: %i end: %i\n", from, until);
    return 0;
  }

  int utc_offset = 1;
  if ( 6==argc ) {
    if ( sscanf(argv[5], "%i", &utc_offset)!=1 ) {
      fprintf(stderr, "Expected an integer for the UTC offset in hours.\n");
      return 0;
    }
  }

  insert_entries(db_path, template, from, until, utc_offset);
}

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <istream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace gtfs
{
// Helper classes ----------------------------------------------------------------------------------
struct InvalidFieldFormat : public std::exception
{
public:
  explicit InvalidFieldFormat(const std::string & msg) : message(prefix + msg) {}

  const char * what() const noexcept
  {
    return message.c_str();
  }

private:
  const std::string prefix = "Invalid GTFS field format. ";
  std::string message;
};

enum ResultCode
{
  OK,
  END_OF_FILE,
  ERROR_INVALID_GTFS_PATH,
  ERROR_FILE_ABSENT,
  ERROR_REQUIRED_FIELD_ABSENT,
  ERROR_INVALID_FIELD_FORMAT
};

using Message = std::string;

struct Result
{
  ResultCode code = OK;
  Message message;

  bool operator==(ResultCode result_code) const { return code == result_code; }
  bool operator!=(ResultCode result_code) const { return !(code == result_code); }
};

// Csv parser  -------------------------------------------------------------------------------------
class CsvParser
{
public:
  CsvParser() = default;
  inline explicit CsvParser(const std::string & gtfs_directory);

  inline Result read_header(const std::string & csv_filename);
  inline Result read_row(std::map<std::string, std::string> & obj);

  inline static std::vector<std::string> split_record(const std::string & record,
                                                      bool is_header = false);

private:
  std::vector<std::string> field_sequence;
  std::filesystem::path gtfs_path;
  std::ifstream csv_stream;
  static const char delimiter = ',';
};

inline CsvParser::CsvParser(const std::string & gtfs_directory) : gtfs_path(gtfs_directory) {}

inline void trim_spaces(std::string & token)
{
  while (!token.empty() && token.back() == ' ')
    token.pop_back();
}

inline std::vector<std::string> CsvParser::split_record(const std::string & record, bool is_header)
{
  std::string const delims = "\r\t";
  size_t start_index = 0;
  if (is_header)
  {
    // ignore UTF-8 BOM prefix:
    if (record.size() > 2 && record[0] == '\xef' && record[1] == '\xbb' && record[2] == '\xbf')
      start_index = 3;
  }
  std::vector<std::string> fields;
  fields.reserve(20);

  std::string token;
  token.reserve(record.size());

  size_t token_start_index = start_index;
  bool is_inside_quotes = false;

  for (size_t i = start_index; i < record.size(); ++i)
  {
    if (record[i] == '"')
    {
      is_inside_quotes = !is_inside_quotes;
      continue;
    }

    if (record[i] == ' ')
    {
      if (token_start_index == i)
        token_start_index = i + 1;
      else
        token += record[i];
      continue;
    }

    if (record[i] == delimiter)
    {
      if (is_inside_quotes)
      {
        token += record[i];
        continue;
      }
      token_start_index = i + 1;
      trim_spaces(token);
      fields.push_back(token);
      token.erase();
      continue;
    }

    if (delims.find(record[i]) == std::string::npos)
      token += record[i];
  }
  trim_spaces(token);
  fields.push_back(token);
  return fields;
}

inline Result CsvParser::read_header(const std::string & csv_filename)
{
  if (csv_stream.is_open())
    csv_stream.close();

  csv_stream.open(gtfs_path / csv_filename);
  if (!csv_stream.is_open())
    return {ResultCode::ERROR_FILE_ABSENT, {}};

  std::string header;
  if (!getline(csv_stream, header) || header.empty())
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, {}};

  field_sequence = split_record(header, true);
  return {ResultCode::OK, {}};
}

inline Result CsvParser::read_row(std::map<std::string, std::string> & obj)
{
  std::string row;
  if (!getline(csv_stream, row))
  {
    obj = {};
    return {ResultCode::END_OF_FILE, {}};
  }

  if (row == "\r")
  {
    obj = {};
    return {ResultCode::OK, {}};
  }

  std::vector<std::string> fields_values = split_record(row);

  // Different count of fields in row and in the header of csv.
  // Typical approach to skip not required fields.
  if (fields_values.size() != field_sequence.size())
    obj = {};

  for (size_t i = 0; i < field_sequence.size(); ++i)
    obj[field_sequence[i]] = fields_values[i];

  return {ResultCode::OK, {}};
}

// Custom types for GTFS fields --------------------------------------------------------------------
// Id of GTFS entity, a sequence of any UTF-8 characters. Used as type for ID GTFS fields.
using Id = std::string;
// A string of UTF-8 characters. Used as type for Text GTFS fields.
using Text = std::string;

// Time in GTFS is in the HH:MM:SS format (H:MM:SS is also accepted)
// Time within a service day can be above 24:00:00, e.g. 28:41:30
class Time
{
public:
  inline Time() = default;
  inline explicit Time(const std::string & raw_time_str);
  inline Time(uint16_t hours, uint16_t minutes, uint16_t seconds);
  inline bool is_provided() const;
  inline size_t get_total_seconds() const;
  inline std::tuple<uint16_t, uint16_t, uint16_t> get_hh_mm_ss() const;
  inline std::string get_raw_time() const;
  inline bool limit_hours_to_24max();

private:
  inline void set_total_seconds();
  inline void set_raw_time();
  bool time_is_provided = false;
  std::string raw_time;
  size_t total_seconds = 0;
  uint16_t hh = 0;
  uint16_t mm = 0;
  uint16_t ss = 0;
};

inline bool operator==(const Time & lhs, const Time & rhs)
{
  return lhs.get_hh_mm_ss() == rhs.get_hh_mm_ss() && lhs.is_provided() == rhs.is_provided();
}

inline bool Time::limit_hours_to_24max()
{
  if (hh < 24)
    return false;

  hh = hh % 24;
  set_total_seconds();
  set_raw_time();
  return true;
}

inline void Time::set_total_seconds()
{
  total_seconds = hh * 60 * 60 + mm * 60 + ss;
}

inline std::string append_leading_zero(const std::string & s, bool check = true)
{
  if (check && s.size() > 2)
    throw InvalidFieldFormat("The string for appending zero is too long: " + s);

  if (s.size() == 2)
    return s;
  return "0" + s;
}

inline void Time::set_raw_time()
{
  const std::string hh_str = append_leading_zero(std::to_string(hh), false);
  const std::string mm_str = append_leading_zero(std::to_string(mm));
  const std::string ss_str = append_leading_zero(std::to_string(ss));

  raw_time = hh_str + ":" + mm_str + ":" + ss_str;
}

// Time in the HH:MM:SS format (H:MM:SS is also accepted). Used as type for Time GTFS fields.
inline Time::Time(const std::string & raw_time_str) : raw_time(raw_time_str)
{
  if (raw_time_str.empty())
    return;

  const size_t len = raw_time.size();
  if (!(len == 7 || len == 8) || (raw_time[len - 3] != ':' && raw_time[len - 6] != ':'))
    throw InvalidFieldFormat("Time is not in [H]H:MM:SS format: " + raw_time_str);

  hh = static_cast<uint16_t>(std::stoi(raw_time.substr(0, len - 6)));
  mm = static_cast<uint16_t>(std::stoi(raw_time.substr(len - 5, 2)));
  ss = static_cast<uint16_t>(std::stoi(raw_time.substr(len - 2)));

  if (mm > 60 || ss > 60)
    throw InvalidFieldFormat("Time minutes/seconds wrong value: " + std::to_string(mm) +
                             " minutes, " + std::to_string(ss) + " seconds");

  set_total_seconds();
  time_is_provided = true;
}

inline Time::Time(uint16_t hours, uint16_t minutes, uint16_t seconds)
    : hh(hours), mm(minutes), ss(seconds)
{
  if (mm > 60 || ss > 60)
    throw InvalidFieldFormat("Time is out of range: " + std::to_string(mm) + "minutes " +
                             std::to_string(ss) + "seconds");

  set_total_seconds();
  set_raw_time();
  time_is_provided = true;
}

inline bool Time::is_provided() const { return time_is_provided; }

inline size_t Time::get_total_seconds() const { return total_seconds; }

inline std::tuple<uint16_t, uint16_t, uint16_t> Time::get_hh_mm_ss() const { return {hh, mm, ss}; }

inline std::string Time::get_raw_time() const { return raw_time; }

// Service day in the YYYYMMDD format.
class Date
{
public:
  inline Date() = default;
  inline Date(uint16_t year, uint16_t month, uint16_t day);
  inline explicit Date(const std::string & raw_date_str);
  inline bool is_provided() const;
  inline std::tuple<uint16_t, uint16_t, uint16_t> get_yyyy_mm_dd() const;
  inline std::string get_raw_date() const;

private:
  inline void check_valid() const;

  std::string raw_date;
  uint16_t yyyy = 0;
  uint16_t mm = 0;
  uint16_t dd = 0;
  bool date_is_provided = false;
};

inline bool operator==(const Date & lhs, const Date & rhs)
{
  return lhs.get_yyyy_mm_dd() == rhs.get_yyyy_mm_dd() && lhs.is_provided() == rhs.is_provided();
}

inline void Date::check_valid() const
{
  if (yyyy < 1000 || yyyy > 9999 || mm < 1 || mm > 12 || dd < 1 || dd > 31)
    throw InvalidFieldFormat("Date check failed: out of range. " + std::to_string(yyyy) +
                             " year, " + std::to_string(mm) + " month, " + std::to_string(dd) +
                             " day");

  if (mm == 2 && dd > 28)
  {
    // The year is not leap. Days count should be 28.
    if (yyyy % 4 != 0 || (yyyy % 100 == 0 && yyyy % 400 != 0))
      throw InvalidFieldFormat("Invalid days count in February of non-leap year: " +
                               std::to_string(dd) + " year" + std::to_string(yyyy));

    // The year is leap. Days count should be 29.
    if (dd > 29)
      throw InvalidFieldFormat("Invalid days count in February of leap year: " +
                               std::to_string(dd) + " year" + std::to_string(yyyy));
  }

  if (dd > 30 && (mm == 4 || mm == 6 || mm == 9 || mm == 11))
    throw InvalidFieldFormat("Invalid days count in month: " + std::to_string(dd) + " days in " +
                             std::to_string(mm));
}

inline Date::Date(uint16_t year, uint16_t month, uint16_t day) : yyyy(year), mm(month), dd(day)
{
  check_valid();
  const std::string mm_str = append_leading_zero(std::to_string(mm));
  const std::string dd_str = append_leading_zero(std::to_string(dd));

  raw_date = std::to_string(yyyy) + mm_str + dd_str;
  date_is_provided = true;
}

inline Date::Date(const std::string & raw_date_str) : raw_date(raw_date_str)
{
  if (raw_date.empty())
    return;

  if (raw_date.size() != 8)
    throw InvalidFieldFormat("Date is not in YYYY:MM::DD format: " + raw_date_str);

  yyyy = static_cast<uint16_t>(std::stoi(raw_date.substr(0, 4)));
  mm = static_cast<uint16_t>(std::stoi(raw_date.substr(4, 2)));
  dd = static_cast<uint16_t>(std::stoi(raw_date.substr(6, 2)));

  check_valid();

  date_is_provided = true;
}

inline bool Date::is_provided() const { return date_is_provided; }

inline std::tuple<uint16_t, uint16_t, uint16_t> Date::get_yyyy_mm_dd() const
{
  return {yyyy, mm, dd};
}

inline std::string Date::get_raw_date() const { return raw_date; }

// An ISO 4217 alphabetical currency code. Used as type for Currency Code GTFS fields.
using CurrencyCode = std::string;
// An IETF BCP 47 language code. Used as type for Language Code GTFS fields.
using LanguageCode = std::string;

// Helper enums for some GTFS fields ---------------------------------------------------------------
enum class StopLocationType
{
  StopOrPlatform = 0,
  Station = 1,
  EntranceExit = 2,
  GenericNode = 3,
  BoardingArea = 4
};

// The type of transportation used on a route.
enum class RouteType
{
  // GTFS route types
  Tram = 0,         // Tram, Streetcar, Light rail
  Subway = 1,       // Any underground rail system within a metropolitan area
  Rail = 2,         // Intercity or long-distance travel
  Bus = 3,          // Short- and long-distance bus routes
  Ferry = 4,        // Boat service
  CableTram = 5,    // Street-level rail cars where the cable runs beneath the vehicle
  AerialLift = 6,   // Aerial lift, suspended cable car (gondola lift, aerial tramway)
  Funicular = 7,    // Any rail system designed for steep inclines
  Trolleybus = 11,  // Electric buses that draw power from overhead wires using poles
  Monorail = 12,    // Railway in which the track consists of a single rail or a beam

  // Extended route types
  // https://developers.google.com/transit/gtfs/reference/extended-route-types
  RailwayService = 100,
  HighSpeedRailService = 101,
  LongDistanceTrains = 102,
  InterRegionalRailService = 103,
  CarTransportRailService = 104,
  SleeperRailService = 105,
  RegionalRailService = 106,
  TouristRailwayService = 107,
  RailShuttleWithinComplex = 108,
  SuburbanRailway = 109,
  ReplacementRailService = 110,
  SpecialRailService = 111,
  LorryTransportRailService = 112,
  AllRailServices = 113,
  CrossCountryRailService = 114,
  VehicleTransportRailService = 115,
  RackAndPinionRailway = 116,
  AdditionalRailService = 117,

  CoachService = 200,
  InternationalCoachService = 201,
  NationalCoachService = 202,
  ShuttleCoachService = 203,
  RegionalCoachService = 204,
  SpecialCoachService = 205,
  SightseeingCoachService = 206,
  TouristCoachService = 207,
  CommuterCoachService = 208,
  AllCoachServices = 209,

  UrbanRailwayService400 = 400,
  MetroService = 401,
  UndergroundService = 402,
  UrbanRailwayService403 = 403,
  AllUrbanRailwayServices = 404,
  Monorail405 = 405,

  BusService = 700,
  RegionalBusService = 701,
  ExpressBusService = 702,
  StoppingBusService = 703,
  LocalBusService = 704,
  NightBusService = 705,
  PostBusService = 706,
  SpecialNeedsBus = 707,
  MobilityBusService = 708,
  MobilityBusForRegisteredDisabled = 709,
  SightseeingBus = 710,
  ShuttleBus = 711,
  SchoolBus = 712,
  SchoolAndPublicServiceBus = 713,
  RailReplacementBusService = 714,
  DemandAndResponseBusService = 715,
  AllBusServices = 716,

  TrolleybusService = 800,

  TramService = 900,
  CityTramService = 901,
  LocalTramService = 902,
  RegionalTramService = 903,
  SightseeingTramService = 904,
  ShuttleTramService = 905,
  AllTramServices = 906,

  WaterTransportService = 1000,
  AirService = 1100,
  FerryService = 1200,
  AerialLiftService = 1300,
  FunicularService = 1400,
  TaxiService = 1500,
  CommunalTaxiService = 1501,
  WaterTaxiService = 1502,
  RailTaxiService = 1503,
  BikeTaxiService = 1504,
  LicensedTaxiService = 1505,
  PrivateHireServiceVehicle = 1506,
  AllTaxiServices = 1507,
  MiscellaneousService = 1700,
  HorseDrawnCarriage = 1702
};

enum class TripDirectionId
{
  DefaultDirection = 0,  // e.g. outbound
  OppositeDirection = 1  // e.g. inbound
};

enum class TripAccess
{
  NoInfo = 0,
  Yes = 1,
  No = 2
};

enum class StopTimeBoarding
{
  RegularlyScheduled = 0,
  No = 1,                   // Not available
  Phone = 2,                // Must phone agency to arrange
  CoordinateWithDriver = 3  // Must coordinate with driver to arrange
};

enum class StopTimePoint
{
  Approximate = 0,
  Exact = 1
};

enum class CalendarAvailability
{
  NotAvailable = 0,
  Available = 1
};

enum class CalendarDateException
{
  Added = 1,  // Service has been added for the specified date
  Removed = 2
};

enum class FarePayment
{
  OnBoard = 0,
  BeforeBoarding = 1  // Fare must be paid before boarding
};

enum class FareTransfers
{
  No = 0,  // No transfers permitted on this fare
  Once = 1,
  Twice = 2,
  Unlimited = 3
};

enum class FrequencyTripService
{
  FrequencyBased = 0,  // Frequency-based trips
  ScheduleBased = 1    // Schedule-based trips with the exact same headway throughout the day
};

enum class TransferType
{
  Recommended = 0,
  Timed = 1,
  MinimumTime = 2,
  NotPossible = 3
};

enum class PathwayMode
{
  Walkway = 1,
  Stairs = 2,
  MovingSidewalk = 3,  // Moving sidewalk/travelator
  Escalator = 4,
  Elevator = 5,
  FareGate = 6,  // Payment gate
  ExitGate = 7
};

enum class PathwayDirection
{
  Unidirectional = 0,
  Bidirectional = 1
};

enum class TranslationTable
{
  Agency = 0,
  Stops,
  Routes,
  Trips,
  StopTimes,
  FeedInfo
};

enum class AttributionRole
{
  No = 0,  // Organization doesn’t have this role
  Yes = 1  // Organization does have this role
};

// Structures representing GTFS entities -----------------------------------------------------------
// Required dataset file
struct Agency
{
  // Conditionally optional:
  Id agency_id;

  // Required:
  Text agency_name;
  Text agency_url;
  Text agency_timezone;

  // Optional:
  Text agency_lang;
  Text agency_phone;
  Text agency_fare_url;
  Text agency_email;
};

// Required dataset file
struct Stop
{
  // Required:
  Id stop_id;

  // Conditionally required:
  Text stop_name;

  bool coordinates_present = true;
  double stop_lat = 0.0;
  double stop_lon = 0.0;
  Id zone_id;
  Id parent_station;

  // Optional:
  Text stop_code;
  Text stop_desc;
  Text stop_url;
  StopLocationType location_type = StopLocationType::GenericNode;
  Text stop_timezone;
  Text wheelchair_boarding;
  Id level_id;
  Text platform_code;
};

// Required dataset file
struct Route
{
  // Required:
  Id route_id;
  RouteType route_type = RouteType::Tram;

  // Conditionally required:
  Id agency_id;
  Text route_short_name;
  Text route_long_name;

  // Optional
  Text route_desc;
  Text route_url;
  Text route_color;
  Text route_text_color;
  size_t route_sort_order = 0;  // Routes with smaller value values should be displayed first
};

// Required dataset file
struct Trip
{
  // Required:
  Id route_id;
  Id service_id;
  Id trip_id;

  // Optional:
  Text trip_headsign;
  Text trip_short_name;
  TripDirectionId direction_id = TripDirectionId::DefaultDirection;
  Id block_id;
  Id shape_id;
  TripAccess wheelchair_accessible = TripAccess::NoInfo;
  TripAccess bikes_allowed = TripAccess::NoInfo;
};

// Required dataset file
struct StopTime
{
  // Required:
  Id trip_id;
  Id stop_id;
  size_t stop_sequence = 0;

  // Conditionally required:
  Time arrival_time;

  Time departure_time;

  // Optional:
  Text stop_headsign;
  StopTimeBoarding pickup_type = StopTimeBoarding::RegularlyScheduled;
  StopTimeBoarding drop_off_type = StopTimeBoarding::RegularlyScheduled;

  double shape_dist_traveled = 0.0;
  StopTimePoint timepoint = StopTimePoint::Exact;
};

// Conditionally required dataset file:
struct CalendarItem
{
  // Required:
  Id service_id;

  CalendarAvailability monday = CalendarAvailability::NotAvailable;
  CalendarAvailability tuesday = CalendarAvailability::NotAvailable;
  CalendarAvailability wednesday = CalendarAvailability::NotAvailable;
  CalendarAvailability thursday = CalendarAvailability::NotAvailable;
  CalendarAvailability friday = CalendarAvailability::NotAvailable;
  CalendarAvailability saturday = CalendarAvailability::NotAvailable;
  CalendarAvailability sunday = CalendarAvailability::NotAvailable;

  Date start_date;
  Date end_date;
};

// Conditionally required dataset file
struct CalendarDate
{
  // Required:
  Id service_id;
  Date date;
  CalendarDateException exception_type = CalendarDateException::Added;
};

// Optional dataset file
struct FareAttribute
{
  // Required:
  Id fare_id;
  double price = 0.0;
  CurrencyCode currency_code;
  FarePayment payment_method = FarePayment::BeforeBoarding;
  FareTransfers transfers = FareTransfers::Unlimited;

  // Conditionally required:
  Id agency_id;

  // Optional:
  size_t transfer_duration = 0;  // Length of time in seconds before a transfer expires
};

// Optional dataset file
struct FareRule
{
  // Required:
  Id fare_id;

  // Optional:
  Id route_id;
  Id origin_id;
  Id destination_id;
  Id contains_id;
};

// Optional dataset file
struct ShapePoint
{
  // Required:
  Id shape_id;
  double shape_pt_lat = 0.0;
  double shape_pt_lon = 0.0;
  size_t shape_pt_sequence = 0;

  // Optional:
  double shape_dist_traveled = 0;
};

// Optional dataset file
struct Frequency
{
  // Required:
  Id trip_id;
  Time start_time;
  Time end_time;
  size_t headway_secs = 0;

  // Optional:
  FrequencyTripService exact_times = FrequencyTripService::FrequencyBased;
};

// Optional dataset file
struct Transfer
{
  // Required:
  Id from_stop_id;
  Id to_stop_id;
  TransferType transfer_type = TransferType::Recommended;

  // Optional:
  size_t min_transfer_time = 0;
};

// Optional dataset file for the GTFS-Pathways extension
struct Pathway
{
  // Required:
  Id pathway_d;
  Id from_stop_id;
  Id to_stop_id;
  PathwayMode pathway_mode = PathwayMode::Walkway;
  PathwayDirection is_bidirectional = PathwayDirection::Unidirectional;

  // Optional fields:
  // Horizontal length in meters of the pathway from the origin location
  double length = 0.0;
  // Average time in seconds needed to walk through the pathway from the origin location
  size_t traversal_time = 0;
  // Number of stairs of the pathway
  size_t stair_count = 0;
  // Maximum slope ratio of the pathway
  double max_slope = 0.0;
  // Minimum width of the pathway in meters
  double min_width = 0.0;
  // Text from physical signage visible to transit riders
  Text signposted_as;
  // Same as signposted_as, but when the pathways is used backward
  Text reversed_signposted_as;
};

// Optional dataset file
struct Level
{
  // Required:
  Id level_id;

  // Numeric index of the level that indicates relative position of this level in relation to other
  // levels (levels with higher indices are assumed to be located above levels with lower indices).
  // Ground level should have index 0, with levels above ground indicated by positive indices and
  // levels below ground by negative indices
  double level_index = 0.0;

  // Optional:
  Text level_name;
};

// Optional dataset file
struct FeedInfo
{
  // Required:
  Text feed_publisher_name;
  Text feed_publisher_url;
  LanguageCode feed_lang;

  // Optional:
  Date feed_start_date;
  Date feed_end_date;
  Text feed_version;
  Text feed_contact_email;
  Text feed_contact_url;
};

// Optional dataset file
struct Translation
{
  // Required:
  TranslationTable table_name = TranslationTable::Agency;
  Text field_name;
  LanguageCode language;
  Text translation;

  // Conditionally required:
  Id record_id;
  Id record_sub_id;
  Text field_value;
};

// Optional dataset file
struct Attribution
{
  // Required:
  Text organization_name;

  // Optional:
  Id attribution_id;  // Useful for translations
  Id agency_id;
  Id route_id;
  Id trip_id;

  AttributionRole is_producer = AttributionRole::No;
  AttributionRole is_operator = AttributionRole::No;
  AttributionRole is_authority = AttributionRole::No;

  Text attribution_url;
  Text attribution_email;
  Text attribution_phone;
};

// Main classes for working with GTFS feeds
using Agencies = std::vector<Agency>;
using Stops = std::vector<Stop>;
using Routes = std::vector<Route>;
using Trips = std::vector<Trip>;
using StopTimes = std::vector<StopTime>;
using Calendar = std::vector<CalendarItem>;
using CalendarDates = std::vector<CalendarDate>;

using FareRules = std::vector<FareRule>;
using Shapes = std::vector<ShapePoint>;
using Shape = std::vector<ShapePoint>;
using Frequencies = std::vector<Frequency>;
using Transfers = std::vector<Transfer>;
using Pathways = std::vector<Pathway>;
using Levels = std::vector<Level>;
// FeedInfo is a unique object and doesn't need a container.
using Translations = std::vector<Translation>;
using Attributions = std::vector<Attribution>;

using ParsedCsvRow = std::map<std::string, std::string>;

class Feed
{
public:
  inline Feed() = default;
  inline explicit Feed(const std::string & gtfs_path);

  inline Result read_feed();

  inline Result write_feed(const std::string & gtfs_path = {}) const;

  inline Result read_agencies();
  inline const Agencies & get_agencies() const;
  inline std::optional<Agency> get_agency(const Id & agency_id) const;
  inline void add_agency(const Agency & agency);

  inline Result read_stops();
  inline const Stops & get_stops() const;
  inline std::optional<Stop> get_stop(const Id & stop_id) const;
  inline void add_stop(const Stop & stop);

  inline Result read_routes();
  inline const Routes & get_routes() const;
  inline std::optional<Route> get_route(const Id & route_id) const;
  inline void add_route(const Route & route);

  inline Result read_trips();
  inline const Trips & get_trips() const;
  inline std::optional<Trip> get_trip(const Id & trip_id) const;
  inline void add_trip(const Trip & trip);

  inline Result read_stop_times();
  inline const StopTimes & get_stop_times() const;
  inline StopTimes get_stop_times_for_stop(const Id & stop_id) const;
  inline StopTimes get_stop_times_for_trip(const Id & trip_id, bool sort_by_sequence = true) const;
  inline void add_stop_time(const StopTime & stop_time);

  inline Result read_calendar();
  inline const Calendar & get_calendar() const;
  inline std::optional<CalendarItem> get_calendar(const Id & service_id) const;
  inline void add_calendar_item(const CalendarItem & calendar_item);

  inline Result read_calendar_dates();
  inline const CalendarDates & get_calendar_dates() const;
  inline CalendarDates get_calendar_dates(const Id & service_id, bool sort_by_date = true) const;
  inline void add_calendar_date(const CalendarDate & calendar_date);

  inline Result read_fare_rules();
  inline const FareRules & get_fare_rules() const;
  inline std::optional<FareRule> get_fare_rule(const Id & fare_id) const;
  inline void add_fare_rule(const FareRule & fare_rule);

  inline Result read_shapes();
  inline const Shapes & get_shapes() const;
  inline Shape get_shape(const Id & shape_id, bool sort_by_sequence = true) const;
  inline void add_shape(const ShapePoint & shape);

  inline Result read_frequencies();
  inline const Frequencies & get_frequencies() const;
  inline Frequencies get_frequencies(const Id & trip_id) const;
  inline void add_frequency(const Frequency & frequency);

  inline Result read_transfers();
  inline const Transfers & get_transfers() const;
  inline std::optional<Transfer> get_transfer(const Id & from_stop_id, const Id & to_stop_id) const;
  inline void add_transfer(const Transfer & transfer);

  inline Result read_pathways();
  inline const Pathways & get_pathways() const;
  inline std::optional<Pathway> get_pathway(const Id & pathway_id) const;
  inline std::optional<Pathway> get_pathway(const Id & from_stop_id, const Id & to_stop_id) const;
  inline void add_pathway(const Pathway & pathway);

  inline Result read_levels();
  inline const Levels & get_levels() const;
  inline std::optional<Level> get_level(const Id & level_id) const;
  inline void add_level(const Level & level);

  inline Result read_feed_info();
  inline FeedInfo get_feed_info() const;
  inline void set_feed_info(const FeedInfo & feed_info);

  inline Result read_translations();
  inline const Translations & get_translations() const;
  inline std::optional<Translation> get_translation(const TranslationTable & table_name) const;
  inline void add_translation(const Translation & translation);

  inline Result read_attributions();
  inline const Attributions & get_attributions() const;
  inline void add_attribution(const Attribution & attribution);

private:
  inline Result parse_csv(const std::string & filename,
                          const std::function<Result(const ParsedCsvRow & record)> & add_entity);

  inline Result add_agency(ParsedCsvRow const & row);
  inline Result add_route(ParsedCsvRow const & row);
  inline Result add_shape(ParsedCsvRow const & row);
  inline Result add_trip(ParsedCsvRow const & row);
  inline Result add_stop(ParsedCsvRow const & row);
  inline Result add_stop_time(ParsedCsvRow const & row);
  inline Result add_calendar_item(ParsedCsvRow const & row);
  inline Result add_calendar_date(ParsedCsvRow const & row);
  inline Result add_transfer(ParsedCsvRow const & row);
  inline Result add_frequency(ParsedCsvRow const & row);

  std::string gtfs_directory;

  Agencies agencies;
  Stops stops;
  Routes routes;
  Trips trips;
  StopTimes stop_times;

  Calendar calendar;
  CalendarDates calendar_dates;
  FareRules fare_rules;
  Shape shapes;
  Frequencies frequencies;
  Transfers transfers;
  Pathways pathways;
  Levels levels;
  Translations translations;
  Attributions attributions;
  FeedInfo feed_info;
};

inline Feed::Feed(const std::string & gtfs_path) : gtfs_directory(gtfs_path) {}

inline Result Feed::read_feed()
{
  if (!std::filesystem::exists(gtfs_directory))
    return {ResultCode::ERROR_INVALID_GTFS_PATH, "Invalid path " + gtfs_directory};

  // Read required files
  if (auto const res = read_agencies(); res.code != ResultCode::OK)
    return res;

  if (auto const res = read_stops(); res.code != ResultCode::OK)
    return res;

  if (auto const res = read_routes(); res.code != ResultCode::OK)
    return res;

  if (auto const res = read_trips(); res.code != ResultCode::OK)
    return res;

  if (auto const res = read_stop_times(); res.code != ResultCode::OK)
    return res;

  // Conditionally required:
  if (auto const res = read_calendar(); res.code != ResultCode::OK)
  {
    if (res != ResultCode::ERROR_FILE_ABSENT)
      return res;
  }

  if (auto const res = read_calendar_dates(); res.code != ResultCode::OK)
  {
    if (res != ResultCode::ERROR_FILE_ABSENT)
      return res;
  }

  // Optional files:
  if (auto const res = read_shapes(); res.code != ResultCode::OK)
  {
    if (res != ResultCode::ERROR_FILE_ABSENT)
      return res;
  }

  if (auto const res = read_transfers(); res.code != ResultCode::OK)
  {
    if (res != ResultCode::ERROR_FILE_ABSENT)
      return res;
  }

  if (auto const res = read_frequencies(); res.code != ResultCode::OK)
  {
    if (res != ResultCode::ERROR_FILE_ABSENT)
      return res;
  }

  // TODO Read other conditionally optional and optional files

  return {ResultCode::OK, {}};
}

inline Result Feed::write_feed(const std::string & gtfs_path) const
{
  if (gtfs_path.empty())
    return {ResultCode::ERROR_INVALID_GTFS_PATH, "Empty output path for writing feed"};
  // TODO Write feed to csv files
  return {};
}

inline std::string get_value_or_default(ParsedCsvRow const & container, const std::string & key,
                                        const std::string & default_value = "")
{
  const auto it = container.find(key);
  if (it == container.end())
    return default_value;

  return it->second;
}

template <class T>
inline void set_field(T & field, ParsedCsvRow const & container, const std::string & key,
                      bool is_optional = true)
{
  const std::string key_str = get_value_or_default(container, key);
  if (!key_str.empty() || !is_optional)
    field = static_cast<T>(std::stoi(key_str));
}

inline bool set_fractional(double & field, ParsedCsvRow const & container, const std::string & key,
                           bool is_optional = true)
{
  const std::string key_str = get_value_or_default(container, key);
  if (!key_str.empty() || !is_optional)
  {
    field = std::stod(key_str);
    return true;
  }
  return false;
}

// Throw if not valid WGS84 decimal degrees.
inline void check_coordinates(double latitude, double longitude)
{
  if (latitude < -90.0 || latitude > 90.0)
    throw std::out_of_range("Latitude");

  if (longitude < -180.0 || longitude > 180.0)
    throw std::out_of_range("Longitude");
}

inline Result Feed::add_agency(ParsedCsvRow const & row)
{
  Agency agency;

  // Conditionally required id:
  agency.agency_id = get_value_or_default(row, "agency_id");

  // Required fields:
  try
  {
    agency.agency_name = row.at("agency_name");
    agency.agency_url = row.at("agency_url");
    agency.agency_timezone = row.at("agency_timezone");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }

  // Optional fields:
  agency.agency_lang = get_value_or_default(row, "agency_lang");
  agency.agency_phone = get_value_or_default(row, "agency_phone");
  agency.agency_fare_url = get_value_or_default(row, "agency_fare_url");
  agency.agency_email = get_value_or_default(row, "agency_email");

  agencies.push_back(agency);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_route(ParsedCsvRow const & row)
{
  Route route;

  try
  {
    // Required fields:
    route.route_id = row.at("route_id");
    set_field(route.route_type, row, "route_type", false);

    // Optional:
    set_field(route.route_sort_order, row, "route_sort_order");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  // Conditionally required:
  route.agency_id = get_value_or_default(row, "agency_id");

  route.route_short_name = get_value_or_default(row, "route_short_name");
  route.route_long_name = get_value_or_default(row, "route_long_name");

  if (route.route_short_name.empty() && route.route_long_name.empty())
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT,
            "'route_short_name' or 'route_long_name' must be specified"};
  }

  route.route_color = get_value_or_default(row, "route_color");
  route.route_text_color = get_value_or_default(row, "route_text_color");
  route.route_desc = get_value_or_default(row, "route_desc");
  route.route_url = get_value_or_default(row, "route_url");

  routes.push_back(route);

  return {ResultCode::OK, {}};
}

inline Result Feed::add_shape(ParsedCsvRow const & row)
{
  ShapePoint point;
  try
  {
    // Required:
    point.shape_id = row.at("shape_id");
    point.shape_pt_sequence = std::stoi(row.at("shape_pt_sequence"));

    point.shape_pt_lon = std::stod(row.at("shape_pt_lon"));
    point.shape_pt_lat = std::stod(row.at("shape_pt_lat"));
    check_coordinates(point.shape_pt_lat, point.shape_pt_lon);

    // Optional:
    set_fractional(point.shape_dist_traveled, row, "shape_dist_traveled");
    if (point.shape_dist_traveled < 0.0)
      throw std::invalid_argument("Invalid shape_dist_traveled");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  shapes.push_back(point);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_trip(ParsedCsvRow const & row)
{
  Trip trip;
  try
  {
    // Required:
    trip.route_id = row.at("route_id");
    trip.service_id = row.at("service_id");
    trip.trip_id = row.at("trip_id");

    // Optional:
    set_field(trip.direction_id, row, "direction_id");
    set_field(trip.wheelchair_accessible, row, "wheelchair_accessible");
    set_field(trip.bikes_allowed, row, "bikes_allowed");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  // Optional:
  trip.shape_id = get_value_or_default(row, "shape_id");
  trip.trip_headsign = get_value_or_default(row, "trip_headsign");
  trip.trip_short_name = get_value_or_default(row, "trip_short_name");
  trip.block_id = get_value_or_default(row, "block_id");

  trips.push_back(trip);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_stop(ParsedCsvRow const & row)
{
  Stop stop;

  try
  {
    stop.stop_id = row.at("stop_id");

    // Optional:
    bool const set_lon = set_fractional(stop.stop_lon, row, "stop_lon");
    bool const set_lat = set_fractional(stop.stop_lat, row, "stop_lat");

    if (!set_lon || !set_lat)
      stop.coordinates_present = false;
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  // Conditionally required:
  stop.stop_name = get_value_or_default(row, "stop_name");
  stop.parent_station = get_value_or_default(row, "parent_station");
  stop.zone_id = get_value_or_default(row, "zone_id");

  // Optional:
  stop.stop_code = get_value_or_default(row, "stop_code");
  stop.stop_desc = get_value_or_default(row, "stop_desc");
  stop.stop_url = get_value_or_default(row, "stop_url");
  set_field(stop.location_type, row, "location_type");
  stop.stop_timezone = get_value_or_default(row, "stop_timezone");
  stop.wheelchair_boarding = get_value_or_default(row, "wheelchair_boarding");
  stop.level_id = get_value_or_default(row, "level_id");
  stop.platform_code = get_value_or_default(row, "platform_code");

  stops.push_back(stop);

  return {ResultCode::OK, {}};
}

inline Result Feed::add_stop_time(ParsedCsvRow const & row)
{
  StopTime stop_time;

  try
  {
    // Required:
    stop_time.trip_id = row.at("trip_id");
    stop_time.stop_id = row.at("stop_id");
    stop_time.stop_sequence = std::stoi(row.at("stop_sequence"));

    // Conditionally required:
    stop_time.departure_time = Time(row.at("departure_time"));
    stop_time.arrival_time = Time(row.at("arrival_time"));

    // Optional:
    set_field(stop_time.pickup_type, row, "pickup_type");
    set_field(stop_time.drop_off_type, row, "drop_off_type");

    set_fractional(stop_time.shape_dist_traveled, row, "shape_dist_traveled");
    if (stop_time.shape_dist_traveled < 0.0)
      throw std::invalid_argument("Invalid shape_dist_traveled");

    set_field(stop_time.timepoint, row, "timepoint");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }
  catch (const InvalidFieldFormat & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  // Optional:
  stop_time.stop_headsign = get_value_or_default(row, "stop_headsign");

  stop_times.push_back(stop_time);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_calendar_item(ParsedCsvRow const & row)
{
  CalendarItem calendar_item;
  try
  {
    // Required fields:
    calendar_item.service_id = row.at("service_id");

    set_field(calendar_item.monday, row, "monday", false);
    set_field(calendar_item.tuesday, row, "tuesday", false);
    set_field(calendar_item.wednesday, row, "wednesday", false);
    set_field(calendar_item.thursday, row, "thursday", false);
    set_field(calendar_item.friday, row, "friday", false);
    set_field(calendar_item.saturday, row, "saturday", false);
    set_field(calendar_item.sunday, row, "sunday", false);

    calendar_item.start_date = Date(row.at("start_date"));
    calendar_item.end_date = Date(row.at("end_date"));
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }
  catch (const InvalidFieldFormat & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  calendar.push_back(calendar_item);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_calendar_date(ParsedCsvRow const & row)
{
  CalendarDate calendar_date;
  try
  {
    // Required fields:
    calendar_date.service_id = row.at("service_id");

    set_field(calendar_date.exception_type, row, "exception_type", false);
    calendar_date.date = Date(row.at("date"));
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }
  catch (const InvalidFieldFormat & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  calendar_dates.push_back(calendar_date);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_transfer(ParsedCsvRow const & row)
{
  Transfer transfer;
  try
  {
    // Required fields:
    transfer.from_stop_id = row.at("from_stop_id");
    transfer.to_stop_id = row.at("to_stop_id");
    set_field(transfer.transfer_type, row, "transfer_type", false);

    // Optional:
    set_field(transfer.min_transfer_time, row, "min_transfer_time");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }
  catch (const InvalidFieldFormat & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  transfers.push_back(transfer);
  return {ResultCode::OK, {}};
}

inline Result Feed::add_frequency(ParsedCsvRow const & row)
{
  Frequency frequency;
  try
  {
    // Required fields:
    frequency.trip_id = row.at("trip_id");
    frequency.start_time = Time(row.at("start_time"));
    frequency.end_time = Time(row.at("end_time"));
    set_field(frequency.headway_secs, row, "headway_secs", false);

    // Optional:
    set_field(frequency.exact_times, row, "exact_times");
  }
  catch (const std::out_of_range & ex)
  {
    return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
  }
  catch (const std::invalid_argument & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }
  catch (const InvalidFieldFormat & ex)
  {
    return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
  }

  frequencies.push_back(frequency);
  return {ResultCode::OK, {}};
}

inline Result Feed::parse_csv(const std::string & filename,
                              const std::function<Result(const ParsedCsvRow & record)> & add_entity)
{
  CsvParser parser(gtfs_directory);
  auto res_header = parser.read_header(filename);
  if (res_header.code != ResultCode::OK)
    return res_header;

  ParsedCsvRow record;
  Result res_row;
  while ((res_row = parser.read_row(record)) != ResultCode::END_OF_FILE)
  {
    if (res_row != ResultCode::OK)
      return res_row;

    if (record.empty())
      continue;

    Result res = add_entity(record);
    if (res != ResultCode::OK)
      return res;
  }

  return {ResultCode::OK, {"Parsed " + filename}};
}

inline Result Feed::read_agencies()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_agency(record); };
  return parse_csv("agency.txt", handler);
}

inline const Agencies & Feed::get_agencies() const { return agencies; }

inline std::optional<Agency> Feed::get_agency(const Id & agency_id) const
{
  // agency id is required when the dataset contains data for multiple agencies,
  // otherwise it is optional:
  if (agency_id.empty() && agencies.size() == 1)
    return agencies[0];

  const auto it =
      std::find_if(agencies.begin(), agencies.end(),
                   [&agency_id](const Agency & agency) { return agency.agency_id == agency_id; });

  if (it == agencies.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_agency(const Agency & agency) { agencies.push_back(agency); }

inline Result Feed::read_stops()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_stop(record); };
  return parse_csv("stops.txt", handler);
}

inline const Stops & Feed::get_stops() const { return stops; }

inline std::optional<Stop> Feed::get_stop(const Id & stop_id) const
{
  const auto it = std::find_if(stops.begin(), stops.end(),
                               [&stop_id](const Stop & stop) { return stop.stop_id == stop_id; });

  if (it == stops.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_stop(const Stop & stop) { stops.push_back(stop); }

inline Result Feed::read_routes()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_route(record); };
  return parse_csv("routes.txt", handler);
}

inline const Routes & Feed::get_routes() const { return routes; }

inline std::optional<Route> Feed::get_route(const Id & route_id) const
{
  const auto it = std::find_if(routes.begin(), routes.end(), [&route_id](const Route & route) {
    return route.route_id == route_id;
  });

  if (it == routes.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_route(const Route & route) { routes.push_back(route); }

inline Result Feed::read_trips()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_trip(record); };
  return parse_csv("trips.txt", handler);
}

inline const Trips & Feed::get_trips() const { return trips; }

inline std::optional<Trip> Feed::get_trip(const Id & trip_id) const
{
  const auto it = std::find_if(trips.begin(), trips.end(),
                               [&trip_id](const Trip & trip) { return trip.trip_id == trip_id; });

  if (it == trips.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_trip(const Trip & trip) { trips.push_back(trip); }

inline Result Feed::read_stop_times()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_stop_time(record); };
  return parse_csv("stop_times.txt", handler);
}

inline const StopTimes & Feed::get_stop_times() const { return stop_times; }

inline StopTimes Feed::get_stop_times_for_stop(const Id & stop_id) const
{
  StopTimes res;
  for (const auto & stop_time : stop_times)
  {
    if (stop_time.stop_id == stop_id)
      res.push_back(stop_time);
  }
  return res;
}

inline StopTimes Feed::get_stop_times_for_trip(const Id & trip_id, bool sort_by_sequence) const
{
  StopTimes res;
  for (const auto & stop_time : stop_times)
  {
    if (stop_time.trip_id == trip_id)
      res.push_back(stop_time);
  }
  if (sort_by_sequence)
  {
    std::sort(res.begin(), res.end(), [](const StopTime & t1, const StopTime & t2) {
      return t1.stop_sequence < t2.stop_sequence;
    });
  }
  return res;
}

inline void Feed::add_stop_time(const StopTime & stop_time) { stop_times.push_back(stop_time); }

inline Result Feed::read_calendar()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_calendar_item(record); };
  return parse_csv("calendar.txt", handler);
}

inline const Calendar & Feed::get_calendar() const { return calendar; }

inline std::optional<CalendarItem> Feed::get_calendar(const Id & service_id) const
{
  const auto it = std::find_if(calendar.begin(), calendar.end(),
                               [&service_id](const CalendarItem & calendar_item) {
                                 return calendar_item.service_id == service_id;
                               });

  if (it == calendar.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_calendar_item(const CalendarItem & calendar_item)
{
  calendar.push_back(calendar_item);
}

inline Result Feed::read_calendar_dates()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_calendar_date(record); };
  return parse_csv("calendar_dates.txt", handler);
}

inline const CalendarDates & Feed::get_calendar_dates() const { return calendar_dates; }

inline CalendarDates Feed::get_calendar_dates(const Id & service_id, bool sort_by_date) const
{
  CalendarDates res;
  for (const auto & calendar_date : calendar_dates)
  {
    if (calendar_date.service_id == service_id)
      res.push_back(calendar_date);
  }

  if (sort_by_date)
  {
    std::sort(res.begin(), res.end(), [](const CalendarDate & d1, const CalendarDate & d2) {
      return d1.date.get_raw_date() < d2.date.get_raw_date();
    });
  }

  return res;
}

inline void Feed::add_calendar_date(const CalendarDate & calendar_date)
{
  calendar_dates.push_back(calendar_date);
}

inline Result Feed::read_fare_rules()
{
  // TODO Read csv
  return {};
}

inline const FareRules & Feed::get_fare_rules() const { return fare_rules; }

inline std::optional<FareRule> Feed::get_fare_rule(const Id & fare_id) const
{
  const auto it =
      std::find_if(fare_rules.begin(), fare_rules.end(),
                   [&fare_id](const FareRule & fare_rule) { return fare_rule.fare_id == fare_id; });

  if (it == fare_rules.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_fare_rule(const FareRule & fare_rule) { fare_rules.push_back(fare_rule); }

inline Result Feed::read_shapes()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_shape(record); };
  return parse_csv("shapes.txt", handler);
}

inline const Shapes & Feed::get_shapes() const { return shapes; }

inline Shape Feed::get_shape(const Id & shape_id, bool sort_by_sequence) const
{
  Shape res;
  for (const auto & shape : shapes)
  {
    if (shape.shape_id == shape_id)
      res.push_back(shape);
  }
  if (sort_by_sequence)
  {
    std::sort(res.begin(), res.end(), [](const ShapePoint & s1, const ShapePoint & s2) {
      return s1.shape_pt_sequence < s2.shape_pt_sequence;
    });
  }
  return res;
}

inline void Feed::add_shape(const ShapePoint & shape) { shapes.push_back(shape); }

inline Result Feed::read_frequencies()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_frequency(record); };
  return parse_csv("frequencies.txt", handler);
}

inline const Frequencies & Feed::get_frequencies() const { return frequencies; }

inline Frequencies Feed::get_frequencies(const Id & trip_id) const
{
  Frequencies res;
  for (const auto & frequency : frequencies)
  {
    if (frequency.trip_id == trip_id)
      res.push_back(frequency);
  }
  return res;
}

inline void Feed::add_frequency(const Frequency & frequency) { frequencies.push_back(frequency); }

inline Result Feed::read_transfers()
{
  auto handler = [this](const ParsedCsvRow & record) { return this->add_transfer(record); };
  return parse_csv("transfers.txt", handler);
}

inline const Transfers & Feed::get_transfers() const { return transfers; }

inline std::optional<Transfer> Feed::get_transfer(const Id & from_stop_id,
                                                  const Id & to_stop_id) const
{
  const auto it = std::find_if(
      transfers.begin(), transfers.end(), [&from_stop_id, &to_stop_id](const Transfer & transfer) {
        return transfer.from_stop_id == from_stop_id && transfer.to_stop_id == to_stop_id;
      });

  if (it == transfers.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_transfer(const Transfer & transfer) { transfers.push_back(transfer); }

inline Result Feed::read_pathways()
{
  // TODO Read csv
  return {};
}

inline const Pathways & Feed::get_pathways() const { return pathways; }

inline std::optional<Pathway> Feed::get_pathway(const Id & pathway_id) const
{
  const auto it = std::find_if(
      pathways.begin(), pathways.end(),
      [&pathway_id](const Pathway & pathway) { return pathway.pathway_d == pathway_id; });

  if (it == pathways.end())
    return std::nullopt;

  return *it;
}

inline std::optional<Pathway> Feed::get_pathway(const Id & from_stop_id,
                                                const Id & to_stop_id) const
{
  const auto it = std::find_if(
      pathways.begin(), pathways.end(), [&from_stop_id, &to_stop_id](const Pathway & pathway) {
        return pathway.from_stop_id == from_stop_id && pathway.to_stop_id == to_stop_id;
      });

  if (it == pathways.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_pathway(const Pathway & pathway) { pathways.push_back(pathway); }

inline Result Feed::read_levels()
{
  // TODO Read csv
  return {};
}

inline const Levels & Feed::get_levels() const { return levels; }

inline std::optional<Level> Feed::get_level(const Id & level_id) const
{
  const auto it = std::find_if(levels.begin(), levels.end(), [&level_id](const Level & level) {
    return level.level_id == level_id;
  });

  if (it == levels.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_level(const Level & level) { levels.push_back(level); }

inline Result Feed::read_feed_info()
{
  // TODO Read csv
  return {};
}

inline FeedInfo Feed::get_feed_info() const { return feed_info; }

inline void Feed::set_feed_info(const FeedInfo & info) { feed_info = info; }

inline Result Feed::read_translations()
{
  // TODO Read csv
  return {};
}

inline const Translations & Feed::get_translations() const { return translations; }

inline std::optional<Translation> Feed::get_translation(const TranslationTable & table_name) const
{
  const auto it = std::find_if(translations.begin(), translations.end(),
                               [&table_name](const Translation & translation) {
                                 return translation.table_name == table_name;
                               });

  if (it == translations.end())
    return std::nullopt;

  return *it;
}

inline void Feed::add_translation(const Translation & translation)
{
  translations.push_back(translation);
}

inline Result Feed::read_attributions()
{
  // TODO Read csv
  return {};
}

inline const Attributions & Feed::get_attributions() const { return attributions; }

inline void Feed::add_attribution(const Attribution & attribution)
{
  attributions.push_back(attribution);
}
}  // namespace gtfs

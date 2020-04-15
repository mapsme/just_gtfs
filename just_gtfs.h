#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <fstream>
#include <istream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace just_gtfs
{
// Helper classes ----------------------------------------------------------------------------------
struct InvalidFieldFormat : public std::exception
{
    const char * what () const throw ()
    {
        return "Invalid GTFS field format";
    }
};

enum ResultCode
{
    OK,
    END_OF_FILE,
    ERROR_INVALID_GTFS_PATH,
    ERROR_REQUIRED_FILE_ABSENT,
    ERROR_REQUIRED_FIELD_ABSENT,
    ERROR_INVALID_FIELD_FORMAT
};

using Message = std::string;

struct Result
{
    ResultCode code;
    Message message;
    bool operator==(ResultCode result_code) const
    {
        return code == result_code;
    }
    bool operator!=(ResultCode result_code) const
    {
        return code != result_code;
    }
};


// Csv parser  -------------------------------------------------------------------------------------
class CsvParser
{
  public:
    CsvParser() = default;
    inline explicit CsvParser(const std::string & gtfs_directory);

    inline Result read_header(const std::string & csv_filename);
    inline Result read_row(std::map<std::string, std::string> & obj);

    inline static std::vector<std::string> split_record(const std::string & record, bool is_header = false);
  private:


    std::vector<std::string> field_sequence;
    std::filesystem::path gtfs_path;
    std::ifstream csv_stream;
    static const char delimiter = ',';
};

inline CsvParser::CsvParser(const std::string & gtfs_directory) : gtfs_path(gtfs_directory)
{ }

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
            fields.push_back(token);
            token.erase();
            continue;
        }

        if (delims.find(record[i]) == std::string::npos)
            token += record[i];
    }

    fields.push_back(token);
    return fields;
}

inline Result CsvParser::read_header(const std::string & csv_filename)
{
    if (csv_stream.is_open())
        csv_stream.close();

    csv_stream.open(gtfs_path / csv_filename);
    if (!csv_stream.is_open())
        return {ResultCode::ERROR_REQUIRED_FILE_ABSENT, {}};

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

    std::vector<std::string> fields_values = split_record(row);
    if (fields_values.size() != field_sequence.size())
        return {ResultCode::ERROR_INVALID_FIELD_FORMAT, {}};

    for (size_t i = 0; i < field_sequence.size(); ++i)
    {
        obj[field_sequence[i]] = fields_values[i];
    }

    return {ResultCode::OK, {}};
}


// Custom types for GTFS fields --------------------------------------------------------------------
using Id = std::string;
using Text = std::string;

// Time in GTFS is in the HH:MM:SS format (H:MM:SS is also accepted)
// Time within a service day can be above 24:00:00, e.g. 28:41:30
class Time
{
  public:
    inline explicit Time(const std::string & raw_time_str);
    inline Time(uint16_t hours, uint16_t minutes, uint16_t seconds);
    inline bool is_provided() const;
    inline size_t get_total_seconds() const;
    inline std::tuple<uint16_t, uint16_t, uint16_t> get_hh_mm_ss() const;
    inline std::string get_raw_time() const;

  private:
    bool time_is_provided = true;
    std::string raw_time;
    size_t total_seconds = 0;
    uint16_t hh = 0;
    uint16_t mm = 0;
    uint16_t ss = 0;
};

inline Time::Time(const std::string & raw_time_str) : raw_time(raw_time_str)
{
    if (raw_time_str.empty())
    {
        time_is_provided = false;
        return;
    }

    size_t len = raw_time.size();
    if(!( len == 7 || len == 8) || (raw_time[len - 3] != ':' && raw_time[len - 6] != ':'))
        throw InvalidFieldFormat();

    hh = static_cast<uint16_t>(std::stoi(raw_time.substr(0, len - 6)));
    mm = static_cast<uint16_t>(std::stoi(raw_time.substr(len - 5, 2)));
    ss = static_cast<uint16_t>(std::stoi(raw_time.substr(len - 2)));

    if (mm > 60 || ss > 60)
        throw InvalidFieldFormat();

    total_seconds = hh * 60 * 60 + mm * 60 + ss;
}

inline Time::Time(uint16_t hours, uint16_t minutes, uint16_t seconds) : hh(hours), mm(minutes), ss(seconds)
{
    if( mm > 60 || ss > 60)
        throw InvalidFieldFormat();

    total_seconds = hh * 60 * 60 + mm * 60 + ss;
    std::string mm_str = std::to_string(mm);
    if (mm_str.size() == 1)
        mm_str = "0" + mm_str;

    std::string ss_str = std::to_string(ss);
    if (ss_str.size() == 1)
        ss_str = "0" + ss_str;
    raw_time = std::to_string(hh) + ":" + mm_str + ":" + ss_str;
}

inline bool Time::is_provided() const
{
    return time_is_provided;
}

inline size_t Time::get_total_seconds() const
{
    return total_seconds;
}

inline std::tuple<uint16_t, uint16_t, uint16_t> Time::get_hh_mm_ss() const
{
    return {hh, mm, ss};
}

inline std::string Time::get_raw_time() const
{
    return raw_time;
}

using Date = std::string;
using CurrencyCode = std::string;
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

enum class RouteType
{
    Tram = 0,    // Tram, Streetcar, Light rail
    Subway = 1,  // Any underground rail system within a metropolitan area
    Rail = 2,    // Intercity or long-distance travel
    Bus = 3,
    Ferry = 4,       // Boat service
    CableTram = 5,   // Street-level rail cars where the cable runs beneath the vehicle
    AerialLift = 6,  // Aerial lift, suspended cable car (gondola lift, aerial tramway)
    Funicular = 7,
    Trolleybus = 11,
    Monorail = 12
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
    Added = 1, // Service has been added for the specified date
    Removed = 2
};

enum class FarePayment
{
    OnBoard = 0,
    BeforeBoarding = 1 // Fare must be paid before boarding
};

enum class FareTransfers
{
    No = 0, // No transfers permitted on this fare
    Once = 1,
    Twice = 2,
    Unlimited = 3
};

enum class FrequencyTripService
{
    FrequencyBased = 0, // Frequency-based trips
    ScheduleBased = 1 // Schedule-based trips with the exact same headway throughout the day
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
    MovingSidewalk = 3, // Moving sidewalk/travelator
    Escalator = 4,
    Elevztor = 5,
    FareGate = 6, // Payment gate
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
    No = 0, // Organization doesn’t have this role
    Yes = 1 // Organization does have this role
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
    RouteType route_type;

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
    TripAccess wheelchair_accessible;
    TripAccess bikes_allowed;
};

// Required dataset file
struct StopTime
{
    // Required:
    Id trip_id;
    Id stop_id;
    size_t stop_sequence = 0;

    // Conditionally required:
    Time arrival_time = Time(0, 0, 0);

    Time departure_time = Time(0, 0, 0);

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

    CalendarAvailability monday;
    CalendarAvailability tuesday;
    CalendarAvailability wednesday;
    CalendarAvailability thursday;
    CalendarAvailability friday;
    CalendarAvailability saturday;
    CalendarAvailability sunday;

    Date start_date;
    Date end_date;
};

// Conditionally required dataset file
struct CalendarDate
{
    // Required:
    Id service_id;
    Date date;
    CalendarDateException exception_type;
};

// Optional dataset file
struct FareAttribute
{
    // Required:
    Id fare_id;
    double price = 0.0;
    CurrencyCode currency_code;
    FarePayment payment_method;
    FareTransfers transfers = FareTransfers::Unlimited;

    // Conditionally required:
    Id agency_id;

    // Optional:
    size_t transfer_duration = 0; // Length of time in seconds before a transfer expires
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
    TransferType  transfer_type = TransferType::Recommended;

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
    PathwayMode pathway_mode;
    PathwayDirection is_bidirectional;

    // Optional:
    double length = 0.0;
    size_t traversal_time = 0; // Seconds
    size_t stair_count = 0;
    double max_slope = 0.0;
    double min_width = 0.0;
    Text signposted_as;
    Text reversed_signposted_as;
};

// Optional dataset file
struct Level
{
    // Required:
    Id level_id;
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
    LanguageCode  feed_lang;

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
    TranslationTable table_name;
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
    Id attribution_id; // Useful for translations
    Id agency_id;
    Id route_id;
    Id trip_id;
    AttributionRole is_producer = AttributionRole::No;
    AttributionRole  is_operator = AttributionRole::No;
    AttributionRole  is_authority = AttributionRole::No;

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

    inline Result add_agency(ParsedCsvRow const & row);
    inline Result add_route(ParsedCsvRow const & row);
    inline Result add_shape(ParsedCsvRow const & row);
    inline Result add_trip(ParsedCsvRow const & row);
    inline Result add_stop(ParsedCsvRow const & row);
    inline Result add_stop_time(ParsedCsvRow const & row);

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
    inline std::optional<CalendarDate> get_calendar_date(const Id & service_id) const;
    inline void add_calendar_date(const CalendarDate & calendar_date);

    inline Result read_fare_rules();
    inline const FareRules & get_fare_rules() const;
    inline std::optional<FareRule> get_fare_rule(const Id & fare_id) const;
    inline void add_fare_rule(const FareRule & fare_rule);

    inline Result read_shapes();
    inline const Shapes & get_shapes() const;
    inline Shapes get_shapes(const Id & shape_id, bool sort_by_sequence = true) const;
    inline void add_shape(const ShapePoint & shape);

    inline Result read_frequencies();
    inline const Frequencies & get_frequencies() const;
    inline std::optional<Frequency> get_frequency(const Id & trip_id) const;
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
    inline std::optional<Translation> get_translation(TranslationTable table_name) const;
    inline void add_translation(const Translation & translation);

    inline Result read_attributions();
    inline const Attributions & get_attributions() const;
    inline void add_attribution(const Attribution & attribution);

  private:
    inline Result parse_csv(const std::string & filename,
                            const std::function<Result(const ParsedCsvRow & record)> & add_entity);
    std::string gtfs_directory;

    Agencies agencies;
    Stops stops;
    Routes routes;
    Trips trips;
    StopTimes stop_times;

    Calendar calendar;
    CalendarDates calendar_dates;
    FareRules fare_rules;
    Shapes shapes;
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
    if(!std::filesystem::exists(gtfs_directory))
        return {ResultCode::ERROR_INVALID_GTFS_PATH, gtfs_directory};

    // Read required files
    if (Result res = read_agencies(); res.code != ResultCode::OK)
        return res;

    if (Result res = read_stops(); res.code != ResultCode::OK)
        return res;

    if (Result res = read_routes(); res.code != ResultCode::OK)
        return res;

    if (Result res = read_trips(); res.code != ResultCode::OK)
        return res;

    if (Result res = read_stop_times(); res.code != ResultCode::OK)
        return res;

    if (Result res = read_shapes(); res.code != ResultCode::OK)
        return res;

    // TODO Read other conditionally optional and optional files

    return {ResultCode::OK, {}};
}

inline Result Feed::write_feed(const std::string & gtfs_path) const
{
    if (gtfs_path.empty())
        return {ResultCode::ERROR_INVALID_GTFS_PATH, "Empty path"};
    // TODO
    return {};
}

inline std::string get_value_or_default(ParsedCsvRow const & container,
                                        const std::string & key,
                                        const std::string & default_value = "")
{
    const auto it = container.find(key);
    if (it == container.end())
        return default_value;

    return it->second;
}

template<class T>
inline void set_field(T & field, ParsedCsvRow const & container,
                      const std::string & key, bool is_optional = true)
{
    const std::string key_str = get_value_or_default(container, key);
    if ((!key_str.empty() && is_optional) || !is_optional)
        field = static_cast<T>(std::stoi(key_str));
}

inline bool set_fractional(double & field, ParsedCsvRow const & container,
                           const std::string & key, bool is_optional = true)
{
    const std::string key_str = get_value_or_default(container, key);
    if ((!key_str.empty() && is_optional) || !is_optional)
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
    catch(std::out_of_range & ex)
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
    catch(std::out_of_range & ex)
    {
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
    }
    catch(std::invalid_argument & ex)
    {
        return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
    }

    // Conditionally required:
    route.agency_id = get_value_or_default(row, "agency_id");

    route.route_short_name = get_value_or_default(row, "route_short_name");
    route.route_long_name = get_value_or_default(row, "route_long_name");

    if (route.route_short_name.empty() && route.route_long_name.empty())
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT,
                "'route_short_name' or 'route_long_name' must be specified"};

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
    catch(std::out_of_range & ex)
    {
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
    }
    catch(std::invalid_argument & ex)
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
    catch(std::out_of_range & ex)
    {
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
    }
    catch(std::invalid_argument & ex)
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
    catch(std::out_of_range & ex)
    {
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
    }
    catch(std::invalid_argument & ex)
    {
        return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
    }

    // Conditionally required:
    stop.stop_name =  get_value_or_default(row, "stop_name");
    stop.parent_station = get_value_or_default(row, "parent_station");
    stop.zone_id = get_value_or_default(row, "zone_id");

    // Optional:
    stop.stop_code = get_value_or_default(row, "stop_code");
    stop.stop_desc =  get_value_or_default(row, "stop_desc");
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
    catch(std::out_of_range & ex)
    {
        return {ResultCode::ERROR_REQUIRED_FIELD_ABSENT, ex.what()};
    }
    catch(std::invalid_argument & ex)
    {
        return {ResultCode::ERROR_INVALID_FIELD_FORMAT, ex.what()};
    }

    // Optional:
    stop_time.stop_headsign = get_value_or_default(row, "stop_headsign");

    stop_times.push_back(stop_time);
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
    while (parser.read_row(record) != ResultCode::END_OF_FILE)
    {
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
    auto handler = [this](const ParsedCsvRow & record) { return this->add_agency(record);};
    return parse_csv("agency.txt", handler);
}

inline const Agencies & Feed::get_agencies() const
{
    return agencies;
}

inline std::optional<Agency> Feed::get_agency(const Id & agency_id) const
{
    // agency id is required when the dataset contains data for multiple agencies,
    // otherwise it is optional:
    if (agency_id.empty() && agencies.size() == 1)
        return agencies[0];

    const auto it = std::find_if(agencies.begin(), agencies.end(),
                                 [agency_id](const Agency & agency) {return agency.agency_id == agency_id;});

    if (it == agencies.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_agency(const Agency & agency)
{
    agencies.push_back(agency);
}

inline Result Feed::read_stops()
{
    auto handler = [this](const ParsedCsvRow & record) { return this->add_stop(record);};
    return parse_csv("stops.txt", handler);
}

inline const Stops & Feed::get_stops() const
{
    return stops;
}

inline std::optional<Stop> Feed::get_stop(const Id & stop_id) const
{
    const auto it = std::find_if(stops.begin(), stops.end(),
                                 [stop_id](const Stop & stop) {return stop.stop_id == stop_id;});

    if (it == stops.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_stop(const Stop & stop)
{
    stops.push_back(stop);
}

inline Result Feed::read_routes()
{
    auto handler = [this](const ParsedCsvRow & record) { return this->add_route(record);};
    return parse_csv("routes.txt", handler);
}

inline const Routes & Feed::get_routes() const
{
    return routes;
}

inline std::optional<Route> Feed::get_route(const Id & route_id) const
{
    const auto it = std::find_if(routes.begin(), routes.end(),
                                 [route_id](const Route & route) {return route.route_id == route_id;});

    if (it == routes.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_route(const Route & route)
{
    routes.push_back(route);
}

inline Result Feed::read_trips()
{
    auto handler = [this](const ParsedCsvRow & record) { return this->add_trip(record);};
    return parse_csv("trips.txt", handler);
}

inline const Trips & Feed::get_trips() const
{
    return trips;
}

inline std::optional<Trip> Feed::get_trip(const Id & trip_id) const
{
    const auto it = std::find_if(trips.begin(), trips.end(),
                                 [trip_id](const Trip & trip) {return trip.trip_id == trip_id;});

    if (it == trips.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_trip(const Trip & trip)
{
    trips.push_back(trip);
}

inline Result Feed::read_stop_times()
{
    auto handler = [this](const ParsedCsvRow & record) { return this->add_stop_time(record);};
    return parse_csv("stop_times.txt", handler);
}

inline const StopTimes & Feed::get_stop_times() const
{
    return stop_times;
}

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


inline void Feed::add_stop_time(const StopTime & stop_time)
{
    stop_times.push_back(stop_time);
}

inline Result Feed::read_calendar()
{
    // TODO
    return {};
}

inline const Calendar & Feed::get_calendar() const
{
    return calendar;
}

inline std::optional<CalendarItem> Feed::get_calendar(const Id & service_id) const
{
    const auto it = std::find_if(calendar.begin(), calendar.end(),
                                 [service_id](const CalendarItem & calendar_item)
                                 {return calendar_item.service_id == service_id;});

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
    // TODO
    return {};
}

inline const CalendarDates & Feed::get_calendar_dates() const
{
    return calendar_dates;
}

inline std::optional<CalendarDate> Feed::get_calendar_date(const Id & service_id) const
{
    const auto it = std::find_if(calendar_dates.begin(), calendar_dates.end(),
                                 [service_id](const CalendarDate & calendar_date) {return calendar_date.service_id == service_id;});

    if (it == calendar_dates.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_calendar_date(const CalendarDate & calendar_date)
{
    calendar_dates.push_back(calendar_date);
}

inline Result Feed::read_fare_rules()
{
    // TODO
    return {};
}

inline const FareRules & Feed::get_fare_rules() const
{
    return fare_rules;
}

inline std::optional<FareRule> Feed::get_fare_rule(const Id & fare_id) const
{
    const auto it = std::find_if(fare_rules.begin(), fare_rules.end(),
                                 [fare_id](const FareRule & fare_rule) {return fare_rule.fare_id == fare_id;});

    if (it == fare_rules.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_fare_rule(const FareRule & fare_rule)
{
    fare_rules.push_back(fare_rule);
}

inline Result Feed::read_shapes()
{
    auto handler = [this](const ParsedCsvRow & record) { return this->add_shape(record);};
    return parse_csv("shapes.txt", handler);
}

inline const Shapes & Feed::get_shapes() const
{
    return shapes;
}

inline Shapes Feed::get_shapes(const Id & shape_id, bool sort_by_sequence) const
{
    Shapes res;
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

inline void Feed::add_shape(const ShapePoint & shape)
{
    shapes.push_back(shape);
}

inline Result Feed::read_frequencies()
{
    // TODO
    return {};
}

inline const Frequencies & Feed::get_frequencies() const
{
    return frequencies;
}

inline std::optional<Frequency> Feed::get_frequency(const Id & trip_id) const
{
    const auto it = std::find_if(frequencies.begin(), frequencies.end(),
                                 [trip_id](const Frequency & frequency) {return frequency.trip_id == trip_id;});

    if (it == frequencies.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_frequency(const Frequency & frequency)
{
    frequencies.push_back(frequency);
}

inline Result Feed::read_transfers()
{
    // TODO
    return {};
}

inline const Transfers & Feed::get_transfers() const
{
    return transfers;
}

inline std::optional<Transfer> Feed::get_transfer(const Id & from_stop_id, const Id & to_stop_id) const
{
    const auto it = std::find_if(transfers.begin(), transfers.end(),
                                 [from_stop_id, to_stop_id](const Transfer & transfer)
                                 {return transfer.from_stop_id == from_stop_id &&
                                         transfer.to_stop_id == to_stop_id;});

    if (it == transfers.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_transfer(const Transfer & transfer)
{
    transfers.push_back(transfer);
}

inline Result Feed::read_pathways()
{
    // TODO
    return {};
}

inline const Pathways & Feed::get_pathways() const
{
    return pathways;
}

inline std::optional<Pathway> Feed::get_pathway(const Id & pathway_id) const
{
    const auto it = std::find_if(pathways.begin(), pathways.end(),
                                 [pathway_id](const Pathway & pathway) {return pathway.pathway_d == pathway_id;});

    if (it == pathways.end())
        return std::nullopt;

    return *it;
}

inline std::optional<Pathway> Feed::get_pathway(const Id & from_stop_id, const Id & to_stop_id) const
{
    const auto it = std::find_if(pathways.begin(), pathways.end(),
                                 [from_stop_id, to_stop_id](const Pathway & pathway)
                                 {return pathway.from_stop_id == from_stop_id &&
                                         pathway.to_stop_id == to_stop_id;});

    if (it == pathways.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_pathway(const Pathway & pathway)
{
    pathways.push_back(pathway);
}

inline Result Feed::read_levels()
{
    // TODO
    return {};
}

inline const Levels & Feed::get_levels() const
{
    return levels;
}

inline std::optional<Level> Feed::get_level(const Id & level_id) const
{
    const auto it = std::find_if(levels.begin(), levels.end(),
                                 [level_id](const Level & level) {return level.level_id == level_id;});

    if (it == levels.end())
        return std::nullopt;

    return *it;
}

inline void Feed::add_level(const Level & level)
{
    levels.push_back(level);
}

inline Result Feed::read_feed_info()
{
    // TODO
    return {};
}

inline FeedInfo Feed::get_feed_info() const
{
    return feed_info;
}

inline void Feed::set_feed_info(const FeedInfo & info)
{
    feed_info = info;
}

inline Result Feed::read_translations()
{
    // TODO
    return {};
}

inline const Translations & Feed::get_translations() const
{
    return translations;
}

inline std::optional<Translation> Feed::get_translation(TranslationTable table_name) const
{
    const auto it = std::find_if(translations.begin(), translations.end(),
                                 [table_name](const Translation & translation)
                                 {return translation.table_name == table_name;});

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
    // TODO
    return {};
}

inline const Attributions & Feed::get_attributions() const
{
    return attributions;
}

inline void Feed::add_attribution(const Attribution & attribution)
{
    attributions.push_back(attribution);
}
}  // namespace just_gtfs

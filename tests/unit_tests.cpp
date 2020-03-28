#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "just_gtfs/just_gtfs.h"

using namespace gtfs;

TEST_SUITE_BEGIN("Handling time GTFS fields");
TEST_CASE("Time in H:MM:SS format")
{
  Time stop_time("0:19:00");
  CHECK(stop_time.is_provided());
  CHECK_EQ(stop_time.get_hh_mm_ss(), std::make_tuple(0, 19, 0));
  CHECK_EQ(stop_time.get_raw_time(), "0:19:00");
  CHECK_EQ(stop_time.get_total_seconds(), 19 * 60);
}

TEST_CASE("Time in HH:MM:SS format")
{
  Time stop_time("39:45:30");
  CHECK_EQ(stop_time.get_hh_mm_ss(), std::make_tuple(39, 45, 30));
  CHECK_EQ(stop_time.get_raw_time(), "39:45:30");
  CHECK_EQ(stop_time.get_total_seconds(), 39 * 60 * 60 + 45 * 60 + 30);
}

TEST_CASE("Time from integers 1")
{
  Time stop_time(14, 30, 0);
  CHECK_EQ(stop_time.get_hh_mm_ss(), std::make_tuple(14, 30, 0));
  CHECK_EQ(stop_time.get_raw_time(), "14:30:00");
  CHECK_EQ(stop_time.get_total_seconds(), 14 * 60 * 60 + 30 * 60);
}

TEST_CASE("Time from integers 2")
{
  Time stop_time(3, 0, 0);
  CHECK_EQ(stop_time.get_hh_mm_ss(), std::make_tuple(3, 0, 0));
  CHECK_EQ(stop_time.get_raw_time(), "03:00:00");
  CHECK_EQ(stop_time.get_total_seconds(), 3 * 60 * 60);
}

TEST_CASE("Invalid time format")
{
  CHECK_THROWS_AS(Time("12/10/00"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Time("12:100:00"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Time("12:10:100"), const InvalidFieldFormat &);
}

TEST_CASE("Time not provided")
{
  Time stop_time("");
  CHECK(!stop_time.is_provided());
}

TEST_CASE("Convert to Time with 24 hours max")
{
  Time stop_time_near_midnight("24:05:00");
  CHECK(stop_time_near_midnight.limit_hours_to_24max());
  CHECK_EQ(stop_time_near_midnight.get_raw_time(), "00:05:00");

  Time stop_time_morning("27:05:00");
  stop_time_morning.limit_hours_to_24max();
  CHECK_EQ(stop_time_morning.get_raw_time(), "03:05:00");
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Handling date GTFS fields");
TEST_CASE("Date not provided")
{
  Date date("");
  CHECK(!date.is_provided());
}

TEST_CASE("Invalid date format")
{
  // Violation of the format YYYYMMDD:
  CHECK_THROWS_AS(Date("1999314"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Date("20081414"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Date("20170432"), const InvalidFieldFormat &);

  // Count of days in february (leap year):
  CHECK_THROWS_AS(Date("20200230"), const InvalidFieldFormat &);
  // Count of days in february (not leap year):
  CHECK_THROWS_AS(Date("20210229"), const InvalidFieldFormat &);

  // Count of days in months with 30 days:
  CHECK_THROWS_AS(Date("19980431"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Date("19980631"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Date("19980931"), const InvalidFieldFormat &);
  CHECK_THROWS_AS(Date("19981131"), const InvalidFieldFormat &);
}

TEST_CASE("Date from string 1")
{
  Date date("20230903");
  CHECK_EQ(date.get_yyyy_mm_dd(), std::make_tuple(2023, 9, 3));
  CHECK_EQ(date.get_raw_date(), "20230903");
  CHECK(date.is_provided());
}

TEST_CASE("Date from string 2")
{
  Date date("20161231");
  CHECK_EQ(date.get_yyyy_mm_dd(), std::make_tuple(2016, 12, 31));
  CHECK_EQ(date.get_raw_date(), "20161231");
  CHECK(date.is_provided());
}

TEST_CASE("Date from string 3")
{
  Date date("20200229");
  CHECK_EQ(date.get_yyyy_mm_dd(), std::make_tuple(2020, 2, 29));
  CHECK_EQ(date.get_raw_date(), "20200229");
  CHECK(date.is_provided());
}

TEST_CASE("Date from integers")
{
  Date date(2022, 8, 16);
  CHECK_EQ(date.get_yyyy_mm_dd(), std::make_tuple(2022, 8, 16));

  CHECK_EQ(date.get_raw_date(), "20220816");
  CHECK(date.is_provided());
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Csv parsing");
TEST_CASE("Record with empty values")
{
  const auto res = CsvParser::split_record(",, ,");
  CHECK_EQ(res.size(), 4);
  for (const auto & token : res)
    CHECK(token.empty());
}

TEST_CASE("Header with UTF BOM")
{
  const auto res = CsvParser::split_record("\xef\xbb\xbfroute_id, agency_id", true);
  CHECK_EQ(res.size(), 2);
  CHECK_EQ(res[0], "route_id");
  CHECK_EQ(res[1], "agency_id");
}

TEST_CASE("Quotation marks")
{
  const auto res = CsvParser::split_record(R"(27681 ,,"Sisters, OR",,"44.29124",1)");
  CHECK_EQ(res.size(), 6);
  CHECK_EQ(res[2], "Sisters, OR");
  CHECK_EQ(res[4], "44.29124");
  CHECK_EQ(res[5], "1");
}
TEST_SUITE_END();

TEST_SUITE_BEGIN("Read");
// Credits:
// https://www.sfmta.com/reports/gtfs-transit-data
TEST_CASE("Empty container before parsing")
{
  Feed feed("data/San Francisco Municipal Transportation Agency");
  CHECK(feed.get_agencies().empty());
  auto agency = feed.get_agency("10");
  CHECK(!agency);
}

TEST_CASE("Transfers")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_transfers();
  CHECK_EQ(res.code, ResultCode::ERROR_FILE_ABSENT);
  CHECK_EQ(feed.get_transfers().size(), 0);
}

TEST_CASE("Calendar")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_calendar();
  CHECK_EQ(res.code, ResultCode::OK);
  const auto & calendar = feed.get_calendar();
  CHECK_EQ(calendar.size(), 2);

  const auto calendar_record = feed.get_calendar("WE");
  CHECK(calendar_record);

  CHECK_EQ(calendar_record->start_date, Date(2007, 01, 01));
  CHECK_EQ(calendar_record->end_date, Date(2010, 12, 31));

  CHECK_EQ(calendar_record->monday, CalendarAvailability::NotAvailable);
  CHECK_EQ(calendar_record->tuesday, CalendarAvailability::NotAvailable);
  CHECK_EQ(calendar_record->wednesday, CalendarAvailability::NotAvailable);
  CHECK_EQ(calendar_record->thursday, CalendarAvailability::NotAvailable);
  CHECK_EQ(calendar_record->friday, CalendarAvailability::NotAvailable);
  CHECK_EQ(calendar_record->saturday, CalendarAvailability::Available);
  CHECK_EQ(calendar_record->sunday, CalendarAvailability::Available);
}

TEST_CASE("Calendar dates")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_calendar_dates();
  CHECK_EQ(res.code, ResultCode::OK);
  const auto & calendar_dates = feed.get_calendar_dates();
  CHECK_EQ(calendar_dates.size(), 1);

  const auto calendar_record = feed.get_calendar_dates("FULLW");
  CHECK(!calendar_record.empty());

  CHECK_EQ(calendar_record[0].date, Date(2007, 06, 04));
  CHECK_EQ(calendar_record[0].exception_type, CalendarDateException::Removed);
}

TEST_CASE("Read GTFS feed")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_feed();
  CHECK_EQ(res.code, ResultCode::OK);
  CHECK_EQ(feed.get_agencies().size(), 1);
  CHECK_EQ(feed.get_routes().size(), 5);
  CHECK_EQ(feed.get_trips().size(), 11);
  CHECK_EQ(feed.get_shapes().size(), 8);
  CHECK_EQ(feed.get_stops().size(), 9);
  CHECK_EQ(feed.get_stop_times().size(), 28);
}

TEST_CASE("Agencies")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_agencies();
  CHECK_EQ(res.code, ResultCode::OK);
  const auto & agencies = feed.get_agencies();
  CHECK_EQ(agencies.size(), 1);
  CHECK_EQ(agencies[0].agency_id, "DTA");
  CHECK_EQ(agencies[0].agency_name, "Demo Transit Authority");
  CHECK_EQ(agencies[0].agency_url, "http://google.com");
  CHECK(agencies[0].agency_lang.empty());
  CHECK_EQ(agencies[0].agency_timezone, "America/Los_Angeles");

  const auto agency = feed.get_agency("DTA");
  CHECK(agency);
}

TEST_CASE("Routes")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_routes();
  CHECK_EQ(res.code, ResultCode::OK);
  const auto & routes = feed.get_routes();
  CHECK_EQ(routes.size(), 5);
  CHECK_EQ(routes[0].route_id, "AB");
  CHECK_EQ(routes[0].agency_id, "DTA");
  CHECK_EQ(routes[0].route_short_name, "10");
  CHECK_EQ(routes[0].route_long_name, "Airport - Bullfrog");
  CHECK_EQ(routes[0].route_type, RouteType::Bus);
  CHECK(routes[0].route_text_color.empty());
  CHECK(routes[0].route_color.empty());
  CHECK(routes[0].route_desc.empty());

  auto const route = feed.get_route("AB");
  CHECK(route);
}

TEST_CASE("Trips")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_trips();
  CHECK_EQ(res.code, ResultCode::OK);
  const auto & trips = feed.get_trips();
  CHECK_EQ(trips.size(), 11);

  CHECK_EQ(trips[0].block_id, "1");
  CHECK_EQ(trips[0].route_id, "AB");
  CHECK_EQ(trips[0].direction_id, TripDirectionId::DefaultDirection);
  CHECK_EQ(trips[0].trip_headsign, "to Bullfrog");
  CHECK(trips[0].shape_id.empty());
  CHECK_EQ(trips[0].service_id, "FULLW");
  CHECK_EQ(trips[0].trip_id, "AB1");

  auto const trip = feed.get_trip("AB1");
  CHECK(trip);
  CHECK(trip.value().trip_short_name.empty());
}

TEST_CASE("Stops")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_stops();
  CHECK_EQ(res.code, ResultCode::OK);

  const auto & stops = feed.get_stops();
  CHECK_EQ(stops.size(), 9);
  CHECK_EQ(stops[0].stop_lat, 36.425288);
  CHECK_EQ(stops[0].stop_lon, -117.133162);
  CHECK(stops[0].stop_code.empty());
  CHECK(stops[0].stop_url.empty());
  CHECK_EQ(stops[0].stop_id, "FUR_CREEK_RES");
  CHECK(stops[0].stop_desc.empty());
  CHECK_EQ(stops[0].stop_name, "Furnace Creek Resort (Demo)");
  CHECK_EQ(stops[0].location_type, StopLocationType::GenericNode);
  CHECK(stops[0].zone_id.empty());

  auto const stop = feed.get_stop("FUR_CREEK_RES");
  CHECK(stop);
}

TEST_CASE("StopTimes")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_stop_times();
  CHECK_EQ(res.code, ResultCode::OK);

  const auto & stop_times = feed.get_stop_times();
  CHECK_EQ(stop_times.size(), 28);

  CHECK_EQ(stop_times[0].trip_id, "STBA");
  CHECK_EQ(stop_times[0].arrival_time, Time(06, 00, 00));
  CHECK_EQ(stop_times[0].departure_time, Time(06, 00, 00));
  CHECK_EQ(stop_times[0].stop_id, "STAGECOACH");
  CHECK_EQ(stop_times[0].stop_sequence, 1);
  CHECK(stop_times[0].stop_headsign.empty());
  CHECK_EQ(stop_times[0].pickup_type, StopTimeBoarding::RegularlyScheduled);
  CHECK_EQ(stop_times[0].drop_off_type, StopTimeBoarding::RegularlyScheduled);

  CHECK_EQ(feed.get_stop_times_for_stop("STAGECOACH").size(), 3);
  CHECK_EQ(feed.get_stop_times_for_trip("STBA").size(), 2);
}

TEST_CASE("Shapes")
{
  Feed feed("data/sample_feed");
  auto res = feed.read_shapes();
  CHECK_EQ(res.code, ResultCode::OK);

  const auto & shapes = feed.get_shapes();
  CHECK_EQ(shapes.size(), 8);
  CHECK_EQ(shapes[0].shape_id, "10237");
  CHECK_EQ(shapes[0].shape_pt_lat, 43.5176524709);
  CHECK_EQ(shapes[0].shape_pt_lon, -79.6906570431);
  CHECK_EQ(shapes[0].shape_pt_sequence, 50017);
  CHECK_EQ(shapes[0].shape_dist_traveled, 12669);

  auto const shape = feed.get_shape("10237");
  CHECK_EQ(shape.size(), 4);
}
TEST_SUITE_END();

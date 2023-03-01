#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "just_gtfs/just_gtfs.h"

using namespace gtfs;

TEST_SUITE_BEGIN("Handling time GTFS fields");
TEST_CASE("Time in H:MM:SS format")
{
  Time stop_time("0:19:00");
  REQUIRE(stop_time.is_provided());
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

TEST_CASE("Time in HHH:MM:SS format")
{
  Time stop_time("103:05:21");
  CHECK_EQ(stop_time.get_hh_mm_ss(), std::make_tuple(103, 5, 21));
  CHECK_EQ(stop_time.get_raw_time(), "103:05:21");
  CHECK_EQ(stop_time.get_total_seconds(), 103 * 60 * 60 + 5 * 60 + 21);
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
  CHECK_THROWS_AS(Time("12:10/10"), const InvalidFieldFormat &);
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
  REQUIRE_EQ(res.size(), 4);
  for (const auto & token : res)
    CHECK(token.empty());
}

TEST_CASE("Header with UTF BOM")
{
  const auto res = CsvParser::split_record("\xef\xbb\xbfroute_id, agency_id", true);
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "route_id");
  CHECK_EQ(res[1], "agency_id");
}

TEST_CASE("Quotation marks")
{
  const auto res = CsvParser::split_record(R"(27681 ,,"Sisters, OR",,"44.29124",1)");
  REQUIRE_EQ(res.size(), 6);
  CHECK_EQ(res[2], "Sisters, OR");
  CHECK_EQ(res[4], "44.29124");
  CHECK_EQ(res[5], "1");
}

TEST_CASE("Not wrapped quotation marks")
{
  const auto res = CsvParser::split_record(R"(Contains "quotes", commas and text)");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], R"(Contains "quotes")");
  CHECK_EQ(res[1], "commas and text");
}

TEST_CASE("Wrapped quotation marks")
{
  const auto res = CsvParser::split_record(R"("Contains ""quotes"", commas and text")");
  REQUIRE_EQ(res.size(), 1);
  CHECK_EQ(res[0], R"(Contains "quotes", commas and text)");
}

TEST_CASE("Double wrapped quotation marks")
{
  const auto res = CsvParser::split_record(R"(""Double quoted text"")");
  REQUIRE_EQ(res.size(), 1);
}

TEST_CASE("Read quoted empty values")
{
  const auto res = CsvParser::split_record(",\"\"");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "");
  CHECK_EQ(res[1], "");
}
TEST_CASE("Read quoted quote")
{
  const auto res = CsvParser::split_record(",\"\"\"\"");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "");
  CHECK_EQ(res[1], "\"");
}

TEST_CASE("Read quoted double quote")
{
  const auto res = CsvParser::split_record(",\"\"\"\"\"\"");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "");
  CHECK_EQ(res[1], "\"\"");
}

TEST_CASE("Read quoted values with quotes in begin")
{
  const auto res = CsvParser::split_record(",\"\"\"Name\"\" and some other\"");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "");
  CHECK_EQ(res[1], "\"Name\" and some other");
}

TEST_CASE("Read quoted values with quotes at end")
{
  const auto res = CsvParser::split_record(",\"Text and \"\"Name\"\"\"");
  REQUIRE_EQ(res.size(), 2);
  CHECK_EQ(res[0], "");
  CHECK_EQ(res[1], "Text and \"Name\"");
}
TEST_SUITE_END();

TEST_SUITE_BEGIN("Read & write");
// Credits:
// https://developers.google.com/transit/gtfs/examples/gtfs-feed
TEST_CASE("Empty container before parsing")
{
  Feed feed("data/non_existing_dir");
  REQUIRE(feed.get_agencies().empty());
  auto agency = feed.get_agency("agency_10");
  CHECK(!agency);
}

TEST_CASE("Non existend directory")
{
  Feed feed("data/non_existing_dir");
  REQUIRE_EQ(feed.read_transfers(), ResultCode::ERROR_FILE_ABSENT);
  CHECK_EQ(feed.get_transfers().size(), 0);
}

TEST_CASE("Transfers")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_transfers(), ResultCode::OK);
  const auto & transfers = feed.get_transfers();
  CHECK_EQ(transfers.size(), 4);

  CHECK_EQ(transfers[0].from_stop_id, "130");
  CHECK_EQ(transfers[0].to_stop_id, "4");
  CHECK_EQ(transfers[0].transfer_type, TransferType::MinimumTime);
  CHECK_EQ(transfers[0].min_transfer_time, 70);

  const auto & transfer = feed.get_transfer("314", "11");
  REQUIRE(transfer);
  CHECK_EQ(transfer.value().transfer_type, TransferType::Timed);
  CHECK_EQ(transfer.value().min_transfer_time, 0);
}

TEST_CASE("Calendar")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_calendar(), ResultCode::OK);
  const auto & calendar = feed.get_calendar();
  REQUIRE_EQ(calendar.size(), 2);

  const auto & calendar_record = feed.get_calendar("WE");
  REQUIRE(calendar_record);

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
  REQUIRE_EQ(feed.read_calendar_dates(), ResultCode::OK);
  const auto & calendar_dates = feed.get_calendar_dates();
  REQUIRE_EQ(calendar_dates.size(), 1);

  const auto & calendar_record = feed.get_calendar_dates("FULLW");
  REQUIRE(!calendar_record.empty());

  CHECK_EQ(calendar_record[0].date, Date(2007, 06, 04));
  CHECK_EQ(calendar_record[0].exception_type, CalendarDateException::Removed);
}

TEST_CASE("Read GTFS feed")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_feed(), ResultCode::OK);

  CHECK_EQ(feed.get_agencies().size(), 1);
  CHECK_EQ(feed.get_routes().size(), 5);
  CHECK_EQ(feed.get_trips().size(), 11);
  CHECK_EQ(feed.get_shapes().size(), 8);
  CHECK_EQ(feed.get_stops().size(), 9);
  CHECK_EQ(feed.get_stop_times().size(), 28);
  CHECK_EQ(feed.get_transfers().size(), 4);
  CHECK_EQ(feed.get_frequencies().size(), 11);
  CHECK_EQ(feed.get_attributions().size(), 1);
  CHECK_EQ(feed.get_calendar().size(), 2);
  CHECK_EQ(feed.get_calendar_dates().size(), 1);
  CHECK_EQ(feed.get_fare_attributes().size(), 3);
  CHECK_EQ(feed.get_fare_rules().size(), 4);
  CHECK(!feed.get_feed_info().feed_publisher_name.empty());
  CHECK_EQ(feed.get_levels().size(), 3);
  CHECK_EQ(feed.get_pathways().size(), 3);
  CHECK_EQ(feed.get_translations().size(), 1);
}

TEST_CASE("Agency")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_agencies(), ResultCode::OK);

  const auto & agencies = feed.get_agencies();
  REQUIRE_EQ(agencies.size(), 1);
  CHECK_EQ(agencies[0].agency_id, "DTA");
  CHECK_EQ(agencies[0].agency_name, "Demo Transit Authority");
  CHECK_EQ(agencies[0].agency_url, "http://google.com");
  CHECK(agencies[0].agency_lang.empty());
  CHECK_EQ(agencies[0].agency_timezone, "America/Los_Angeles");

  const auto agency = feed.get_agency("DTA");
  CHECK(agency);

  REQUIRE_EQ(feed.write_agencies("data/output_feed"), ResultCode::OK);
  Feed feed_copy("data/output_feed");
  REQUIRE_EQ(feed_copy.read_agencies(), ResultCode::OK);
  CHECK_EQ(agencies, feed_copy.get_agencies());
}

TEST_CASE("Routes")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_routes(), ResultCode::OK);

  const auto & routes = feed.get_routes();
  REQUIRE_EQ(routes.size(), 5);
  CHECK_EQ(routes[0].route_id, "AB");
  CHECK_EQ(routes[0].agency_id, "DTA");
  CHECK_EQ(routes[0].route_short_name, "10");
  CHECK_EQ(routes[0].route_long_name, "Airport - Bullfrog");
  CHECK_EQ(routes[0].route_type, RouteType::Bus);
  CHECK(routes[0].route_text_color.empty());
  CHECK(routes[0].route_color.empty());
  CHECK(routes[0].route_desc.empty());

  const auto & route = feed.get_route("AB");
  CHECK(route);
}

TEST_CASE("Trips")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_trips(), ResultCode::OK);

  const auto & trips = feed.get_trips();
  REQUIRE_EQ(trips.size(), 11);

  CHECK_EQ(trips[0].block_id, "1");
  CHECK_EQ(trips[0].route_id, "AB");
  CHECK_EQ(trips[0].direction_id, TripDirectionId::DefaultDirection);
  CHECK_EQ(trips[0].trip_headsign, "to Bullfrog");
  CHECK(trips[0].shape_id.empty());
  CHECK_EQ(trips[0].service_id, "FULLW");
  CHECK_EQ(trips[0].trip_id, "AB1");

  const auto & trip = feed.get_trip("AB1");
  REQUIRE(trip);
  CHECK(trip.value().trip_short_name.empty());
}

TEST_CASE("Stops")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_stops(), ResultCode::OK);

  const auto & stops = feed.get_stops();
  REQUIRE_EQ(stops.size(), 9);
  CHECK_EQ(stops[0].stop_lat, 36.425288);
  CHECK_EQ(stops[0].stop_lon, -117.133162);
  CHECK(stops[0].stop_code.empty());
  CHECK(stops[0].stop_url.empty());
  CHECK_EQ(stops[0].stop_id, "FUR_CREEK_RES");
  CHECK(stops[0].stop_desc.empty());
  CHECK_EQ(stops[0].stop_name, "Furnace Creek Resort (Demo)");
  CHECK_EQ(stops[0].location_type, StopLocationType::StopOrPlatform);
  CHECK(stops[0].zone_id.empty());

  auto const & stop = feed.get_stop("FUR_CREEK_RES");
  REQUIRE(stop);
}

TEST_CASE("StopTimes")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_stop_times(), ResultCode::OK);

  const auto & stop_times = feed.get_stop_times();
  REQUIRE_EQ(stop_times.size(), 28);

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
  REQUIRE_EQ(feed.read_shapes(), ResultCode::OK);

  const auto & shapes = feed.get_shapes();
  REQUIRE_EQ(shapes.size(), 8);
  CHECK_EQ(shapes[0].shape_id, "10237");
  CHECK_EQ(shapes[0].shape_pt_lat, 43.5176524709);
  CHECK_EQ(shapes[0].shape_pt_lon, -79.6906570431);
  CHECK_EQ(shapes[0].shape_pt_sequence, 50017);
  CHECK_EQ(shapes[0].shape_dist_traveled, 12669);

  const auto & shape = feed.get_shape("10237");
  CHECK_EQ(shape.size(), 4);
}

TEST_CASE("Calendar")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_calendar(), ResultCode::OK);

  const auto & calendar = feed.get_calendar();
  REQUIRE_EQ(calendar.size(), 2);
  CHECK_EQ(calendar[0].service_id, "FULLW");
  CHECK_EQ(calendar[0].start_date, Date(2007, 01, 01));
  CHECK_EQ(calendar[0].end_date, Date(2010, 12, 31));
  CHECK_EQ(calendar[0].monday, CalendarAvailability::Available);
  CHECK_EQ(calendar[0].sunday, CalendarAvailability::Available);

  const auto & calendar_for_service = feed.get_calendar("FULLW");
  CHECK(calendar_for_service);
}

TEST_CASE("Calendar dates")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_calendar_dates(), ResultCode::OK);

  const auto & calendar_dates = feed.get_calendar_dates();
  REQUIRE_EQ(calendar_dates.size(), 1);
  CHECK_EQ(calendar_dates[0].service_id, "FULLW");
  CHECK_EQ(calendar_dates[0].date, Date(2007, 06, 04));
  CHECK_EQ(calendar_dates[0].exception_type, CalendarDateException::Removed);

  const auto & calendar_dates_for_service = feed.get_calendar_dates("FULLW");
  CHECK_EQ(calendar_dates_for_service.size(), 1);
}

TEST_CASE("Frequencies")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_frequencies(), ResultCode::OK);

  const auto & frequencies = feed.get_frequencies();
  REQUIRE_EQ(frequencies.size(), 11);
  CHECK_EQ(frequencies[0].trip_id, "STBA");
  CHECK_EQ(frequencies[0].start_time, Time(6, 00, 00));
  CHECK_EQ(frequencies[0].end_time, Time(22, 00, 00));
  CHECK_EQ(frequencies[0].headway_secs, 1800);

  const auto & frequencies_for_trip = feed.get_frequencies("CITY1");
  CHECK_EQ(frequencies_for_trip.size(), 5);
}

TEST_CASE("Fare attributes")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_fare_attributes(), ResultCode::OK);

  const auto & attributes = feed.get_fare_attributes();
  REQUIRE_EQ(attributes.size(), 3);
  CHECK_EQ(attributes[0].fare_id, "p");
  CHECK_EQ(attributes[0].price, 1.25);
  CHECK_EQ(attributes[0].currency_type, "USD");
  CHECK_EQ(attributes[0].payment_method, FarePayment::OnBoard);
  CHECK_EQ(attributes[0].transfers, FareTransfers::No);
  CHECK_EQ(attributes[0].transfer_duration, 0);

  CHECK_EQ(attributes[1].fare_id, "a");
  CHECK_EQ(attributes[1].price, 5.25);
  CHECK_EQ(attributes[1].currency_type, "USD");
  CHECK_EQ(attributes[1].payment_method, FarePayment::BeforeBoarding);
  CHECK_EQ(attributes[1].transfers, FareTransfers::Once);
  CHECK_EQ(attributes[1].transfer_duration, 0);

  CHECK_EQ(attributes[2].fare_id, "x");
  CHECK_EQ(attributes[2].price, 20);
  CHECK_EQ(attributes[2].currency_type, "USD");
  CHECK_EQ(attributes[2].payment_method, FarePayment::OnBoard);
  CHECK_EQ(attributes[2].transfers, FareTransfers::Unlimited);
  CHECK_EQ(attributes[2].transfer_duration, 60);

  const auto & attributes_for_id = feed.get_fare_attributes("a");
  REQUIRE_EQ(attributes_for_id.size(), 1);
  CHECK_EQ(attributes_for_id[0].price, 5.25);

  REQUIRE_EQ(feed.write_fare_attributes("data/output_feed"), ResultCode::OK);
  Feed feed_copy("data/output_feed");
  REQUIRE_EQ(feed_copy.read_fare_attributes(), ResultCode::OK);
  CHECK_EQ(attributes, feed_copy.get_fare_attributes());
}

TEST_CASE("Fare rules")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_fare_rules(), ResultCode::OK);

  const auto & fare_rules = feed.get_fare_rules();
  REQUIRE_EQ(fare_rules.size(), 4);
  CHECK_EQ(fare_rules[0].fare_id, "p");
  CHECK_EQ(fare_rules[0].route_id, "AB");

  const auto & rules_for_id = feed.get_fare_rules("p");
  REQUIRE_EQ(rules_for_id.size(), 3);
  CHECK_EQ(rules_for_id[1].route_id, "STBA");
}

TEST_CASE("Levels")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_levels(), ResultCode::OK);

  const auto & levels = feed.get_levels();
  REQUIRE_EQ(levels.size(), 3);
  CHECK_EQ(levels[0].level_id, "U321L1");
  CHECK_EQ(levels[0].level_index, -1.5);

  const auto & level = feed.get_level("U321L2");
  REQUIRE(level);

  CHECK_EQ(level.value().level_index, -2);
  CHECK_EQ(level.value().level_name, "Vestibul2");
}

TEST_CASE("Pathways")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_pathways(), ResultCode::OK);

  const auto & pathways = feed.get_pathways();
  REQUIRE_EQ(pathways.size(), 3);
  CHECK_EQ(pathways[0].pathway_id, "T-A01C01");
  CHECK_EQ(pathways[0].from_stop_id, "1073S");
  CHECK_EQ(pathways[0].to_stop_id, "1098E");
  CHECK_EQ(pathways[0].pathway_mode, PathwayMode::Stairs);
  CHECK_EQ(pathways[0].signposted_as, "Sign1");
  CHECK_EQ(pathways[0].reversed_signposted_as, "Sign2");
  CHECK_EQ(pathways[0].is_bidirectional, PathwayDirection::Bidirectional);

  const auto & pathways_by_id = feed.get_pathways("T-A01D01");
  REQUIRE_EQ(pathways_by_id.size(), 2);
  CHECK_EQ(pathways_by_id[0].is_bidirectional, PathwayDirection::Unidirectional);
  CHECK(pathways_by_id[0].reversed_signposted_as.empty());
}

TEST_CASE("Translations")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_translations(), ResultCode::OK);

  const auto & translations = feed.get_translations();
  REQUIRE_EQ(translations.size(), 1);
  CHECK_EQ(translations[0].table_name, "stop_times");
  CHECK_EQ(translations[0].field_name, "stop_headsign");
  CHECK_EQ(translations[0].language, "en");
  CHECK_EQ(translations[0].translation, "Downtown");
  CHECK(translations[0].record_id.empty());
  CHECK(translations[0].record_sub_id.empty());
  CHECK(translations[0].field_value.empty());

  CHECK_EQ(feed.get_translations("stop_times").size(), 1);
}

TEST_CASE("Attributions")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_attributions(), ResultCode::OK);

  const auto & attributions = feed.get_attributions();
  REQUIRE_EQ(attributions.size(), 1);
  CHECK_EQ(attributions[0].attribution_id, "0");
  CHECK_EQ(attributions[0].organization_name, "Test inc");
  CHECK_EQ(attributions[0].is_producer, AttributionRole::Yes);
  CHECK_EQ(attributions[0].is_operator, AttributionRole::No);
  CHECK_EQ(attributions[0].is_authority, AttributionRole::No);
  CHECK_EQ(attributions[0].attribution_url, "https://test.pl/gtfs/");
  CHECK(attributions[0].attribution_email.empty());
  CHECK(attributions[0].attribution_phone.empty());
}

TEST_CASE("Feed info")
{
  Feed feed("data/sample_feed");
  REQUIRE_EQ(feed.read_feed_info(), ResultCode::OK);

  const auto & info = feed.get_feed_info();

  CHECK_EQ(info.feed_publisher_name, "Test Solutions, Inc.");
  CHECK_EQ(info.feed_publisher_url, "http://test");
  CHECK_EQ(info.feed_lang, "en");
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Simple pipelines");

TEST_CASE("Agencies create & save")
{
  Feed feed_for_writing;

  Agency agency1;
  agency1.agency_id = "0Id_b^3 Company";
  agency1.agency_name = R"(Big Big "Bus Company")";
  agency1.agency_email = "b3c@gtfs.com";
  agency1.agency_fare_url = "b3c.no";

  Agency agency2;
  agency2.agency_id = "kwf";
  agency2.agency_name = R"("killer whale ferries")";
  agency2.agency_lang = "en";
  agency2.agency_phone = "842";
  agency2.agency_timezone = "Asia/Tokyo";
  agency2.agency_fare_url = "f@mail.com";

  feed_for_writing.add_agency(agency1);
  feed_for_writing.add_agency(agency2);

  REQUIRE_EQ(feed_for_writing.write_agencies("data/output_feed"), ResultCode::OK);
  Feed feed_for_testing("data/output_feed");

  REQUIRE_EQ(feed_for_testing.read_agencies(), ResultCode::OK);
  CHECK_EQ(feed_for_writing.get_agencies(), feed_for_testing.get_agencies());
}

TEST_CASE("Shapes create & save")
{
  Feed feed_for_writing;

  feed_for_writing.add_shape(ShapePoint{"id1", 61.197843, -149.867731, 0, 0});
  feed_for_writing.add_shape(ShapePoint{"id1", 61.199419, -149.867680, 1, 178});
  feed_for_writing.add_shape(ShapePoint{"id2", 61.199972, -149.867731, 2, 416});

  REQUIRE_EQ(feed_for_writing.write_shapes("data/output_feed"), ResultCode::OK);
  Feed feed_for_testing("data/output_feed");

  REQUIRE_EQ(feed_for_testing.read_shapes(), ResultCode::OK);
  CHECK_EQ(feed_for_testing.get_shapes().size(), 3);
}
TEST_SUITE_END();

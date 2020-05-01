# just_gtfs - header-only modern C++ GTFS parsing library

[![GTFS parser for C++](https://github.com/mapsme/just_gtfs/blob/add-the-most-important-readers/docs/logo.jpeg)](https://github.com/mapsme/just_gtfs)

[![C++](https://img.shields.io/badge/c%2B%2B-17-informational.svg)](https://shields.io/)
[![MIT license](https://img.shields.io/badge/License-MIT-blue.svg)](https://lbesson.mit-license.org/)
![](https://github.com/mapsme/just_gtfs/workflows/C%2FC%2B%2B%20CI/badge.svg)
[![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](https://github.com/mapsme/just_gtfs/issues)

 - Header-only
 - C++17
 - Tested on GCC and Clang
 - STL-compatible containers
 - Fast reading and parsing of GTFS feeds

## Table of Contents
- [Working with GTFS feeds](#working-with-gtfs-feeds)
- [How to use just_library](#how-to-use-it)
- [Used third-party tools](#used-third-party-tools)

## Working with GTFS feeds
The library implements reading static transit data in GTFS - [General Transit Feed Specification](https://developers.google.com/transit/gtfs/reference).
It provides class for working with GTFS feeds: `gtfs::Feed`.
GTFS csv files are mapped to the corresponding C++ classes. Every GTFS entity can be accessed through `gtfs::Feed`.

:pushpin: Example of providing `gtfs::Feed` the feed path, reading it and working with GTFS entities such as stops and routes:
```c++
Feed feed("~/data/SFMTA/");
if (feed.read_feed() == ResultCode::OK)
{
	Stops stops = feed.get_stops();
	std::cout << stops.size() << std::endl;

	Route route = feed.get_route("route_id_1009");
	if (route)
	{
		std::cout << route->route_long_name << std::endl;
	}
}
```

GTFS feed can be wholly read from directory as in the example above or you can read GTFS files separately. E.g., if you need only shapes data, you can avoid parsing all other files and just work with the shapes.

:pushpin: Example of reading only `shapes.txt` from the feed and working with shapes:
```c++
Feed feed("~/data/SFMTA/");
if (feed.read_shapes() == ResultCode::OK)
{
	Shapes all_shapes = feed.get_shapes();
	Shape shape = feed.get_shape("9367");
}
```


## Methods for reading and writing GTFS entities
Methods of the `Feed` class for working with agencies:

Read agencies from the corresponding csv file.
```c++
Result read_agencies()
```

Get reference to `Agencies` - `std::vector` of `Agency` objects.
```c++
const Agencies & get_agencies()
```

Find agency by its id. This method returns `std::optional` so you should check if the result is `std::nullopt`.
```c++
std::optional<Agency> get_agency(const Id & agency_id)
``` 

Add agency to the feed.
```c++
void add_agency(const Agency & agency)
```

Add agency to the feed by filling agency object fields with parsed csv values. `row` is `std::map` with csv column titles as keys ans csv row items as values. 
```c++
Result add_agency(ParsedCsvRow const & row)
```


:pushpin: **There are similar methods for all other GTFS entities** for getting the list of entities, finding and adding them.
For some of them additional methods are provided. 
For example, you can find all the stop times for current stop by its id:
```c++
StopTimes get_stop_times_for_stop(const Id & stop_id)
```

Or you can find stop times for the particular trip:
```c++
StopTimes get_stop_times_for_trip(const Id & trip_id, bool sort_by_sequence = true)
```

## How to use library
- For including the library in your own project: just_gtfs is completely contained inside a single header and therefore it is sufficient to copy include/just_gtfs/just_gtfs.h to your include pathes. The library does not have to be explicitly build.
- For running library tests:
Clone just_gtfs with `git clone --recursive` or run `git submodule update --init --recursive --remote` after cloning.
In the just_gtfs project directory build the project and run unit tests: 
```
cmake .
make
ctest --output-on-failure --verbose
```
The library makes use of the C++17 features and therefore you have to use appropriate compiler version.
- For including as a submodule: use branch "for-usage-as-submodule" which consists of a single header.

## Used third-party tools
- [**doctest**](https://github.com/onqtam/doctest) for unit testing.

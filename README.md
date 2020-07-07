# just_gtfs - header-only modern C++ library for reading and writing GTFS feeds

[![GTFS reader and writer for C++](https://github.com/mapsme/just_gtfs/blob/add-the-most-important-readers/docs/logo.jpeg)](https://github.com/mapsme/just_gtfs)

[![C++](https://img.shields.io/badge/c%2B%2B-17-informational.svg)](https://shields.io/)
[![MIT license](https://img.shields.io/badge/License-MIT-blue.svg)](https://lbesson.mit-license.org/)
![](https://github.com/mapsme/just_gtfs/workflows/C%2FC%2B%2B%20CI/badge.svg)
[![](https://github.com/sindresorhus/awesome/blob/main/media/mentioned-badge.svg)](https://github.com/CUTR-at-USF/awesome-transit)
[![contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg?style=flat)](https://github.com/mapsme/just_gtfs/issues)


## Table of Contents
- [Description](#description)
- [Reading and writing GTFS feeds](#reading-and-writing-gtfs-feeds)
- [How to add library to your project](#how-to-add-library-to-your-project)
- [Used third-party tools](#used-third-party-tools)
- [Contributing](#contributing)
- [Resources](#resources)

## Description
The just_gtfs library implements reading and writing static transit data in GTFS - [General Transit Feed Specification](https://developers.google.com/transit/gtfs/reference).

Its main features:
 - Fast reading and writing of GTFS feeds
 - Support for [extended GTFS route types](https://developers.google.com/transit/gtfs/reference/extended-route-types)
 - Simple working with GTFS `Date` and `Time` formats
 - Header-only
 - Written in C++17
 - Tested on GCC and Clang


## Reading and writing GTFS feeds
Library provides main class for working with GTFS feeds: `gtfs::Feed`. It also provides classes for each of the 17 GTFS entities: `Route`, `Stop`, `Pathway`, `Translation` and others.
GTFS csv files are mapped to the corresponding C++ classes. Every GTFS entity can be accessed through `gtfs::Feed` corresponding getters & setters.

:pushpin: All GTFS entities are managed in the same way. So here is the example for working with `agencies`.

Method of the `Feed` class for reading `agency.txt`:
```c++
Result read_agencies()
```

Method for reading reading not only agencies but all GTFS entities. Path to the feed is specified in the `Feed` constructor:
```c++
Result read_feed()
```

Method for getting reference to the `Agencies` - `std::vector` of all `Agency` objects of the feed:
```c++
const Agencies & get_agencies()
```

Method for finding agency by its id. Returns `std::optional` so you should check if the result is `std::nullopt`:
```c++
std::optional<Agency> get_agency(const Id & agency_id)
``` 

Method for adding agency to the feed:
```c++
void add_agency(const Agency & agency)
```

Method for writing agencies to the `agency.txt` file to `gtfs_path`.
```c++
Result write_agencies(const std::string & gtfs_path)
```

Method for writing all GTFS entities (not only agencies, but stops, stop times, calendar etc):
```c++
Result write_feed(const std::string & gtfs_path)
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

### Example of reading GTFS feed and working with its stops and routes
:pushpin: Provide `gtfs::Feed` the feed path, read it and work with GTFS entities such as stops and routes:
```c++
Feed feed("~/data/SFMTA/");
if (feed.read_feed() == ResultCode::OK)
{
  Stops stops = feed.get_stops();
  std::cout << "Stops count in feed: " << stops.size() << std::endl;

  for (const Stop & stop: stops)
  {
    std::cout << stop.stop_id << std::endl;
  }

  Route route = feed.get_route("route_id_1009");
  if (route)
  {
    std::cout << route->route_long_name << std::endl;
  }
}
```

### Example of parsing shapes.txt and working with its contents
GTFS feed can be wholly read from directory as in the example above or you can read GTFS files separately. E.g., if you need only shapes data, you can avoid parsing all other files and just work with the shapes.

:pushpin: Read only `shapes.txt` from the feed and work with shapes:
```c++
Feed feed("~/data/SFMTA/");
if (feed.read_shapes() == ResultCode::OK)
{
  Shapes all_shapes = feed.get_shapes();
  Shape shape = feed.get_shape("9367");

  for (const ShapePoint & point: shape)
  {
    std::cout << point.shape_pt_lat << " " << point.shape_pt_lon << std::endl;
  }
}
```

### Example of writing GTFS:
:pushpin: If you already filled the `feed` object with data that suits you, you can write it to the corresponding path:
```c++
Feed feed;

// Fill feed with agencies, stops, routes and other required data:

feed.add_trip(some_trip);
feed.add_attribution(attr);

feed.write_feed("~/data/custom_feed/");
```

## How to add library to your project
- For including just_gtfs to your own project **as a submodule:** use branch "for-usage-as-submodule" which consists of a single header.
- Another way of including just_gtfs to your project: just_gtfs is completely contained inside a single header and therefore it is sufficient to copy include/just_gtfs/just_gtfs.h to your **include paths.** The library does not have to be explicitly build.
- For building library and **running tests:**
Clone just_gtfs with `git clone --recursive` or run `git submodule update --init --recursive --remote` after cloning.
In the just_gtfs project directory build the project and run unit tests: 
```
cmake .
make
ctest --output-on-failure --verbose
```
The library makes use of the C++17 features and therefore you have to use the appropriate compiler version.

## Used third-party tools
- [**doctest**](https://github.com/onqtam/doctest) for unit testing.

## Contributing
Please open a [Github issue](https://github.com/mapsme/just_gtfs/issues/new) with as much of the information as you're able to specify, or create a [pull request](https://github.com/mapsme/just_gtfs/pulls) according to our [guidelines](https://github.com/mapsme/just_gtfs/blob/master/docs/CPP_STYLE.md).

## Resources
[GTFS reference in Google GitHub repository](https://github.com/google/transit/blob/master/gtfs/spec/en/reference.md)

[GTFS reference on Google Transit API](https://developers.google.com/transit/gtfs/reference?csw=1)

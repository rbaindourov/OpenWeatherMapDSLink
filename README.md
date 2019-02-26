# OpenWeatherMapDSLink
DSLink for Cisco Kinetic to integrate EFM with OpenWeatherMap.org

## Development Requirements

Written on Ubuntu 16.04 using the efm-cpp-sdk-1.0.15-Ubuntu16.04-dslink-dev

Will need to make sure you have libcurl installed.

```
sudo apt-get install libcurl4-openssl-dev
```

Copied over from EFM SDK: 
* include/ - API header files
* lib/ - API static libraries

Copied over from RapidJSON:
* rapidjson/ - Header Only JSON parser

## Testing Requirements

I am testing using [Cisco Kinetic Edge and Fog Processing Module 1.5.0](https://www.cisco.com/c/en/us/support/cloud-systems-management/edge-fog-fabric/products-installation-guides-list.html)

To build just invoke `make` in the cloned directory.

To start the application you have to supply the broker url by using the `-b` command line parameter:

    prompt> ./open_weather_data_link -b http://localhost:8080/conn

## GNU public license
My modifications are free software.



CFLAGS = -std=c++11 -Wall -Wextra -I/usr/include/curl -I ./include -g -O2 -D_FORTIFY_SOURCE=2 -fPIE -fstack-protector
LDFLAGS = -L ./lib -pie -Wl,-z,now
LIBS = -lboost_log -lboost_date_time -lboost_program_options -lboost_system -lboost_thread -lboost_filesystem -lboost_regex -lssl -lcrypto -ldl -pthread -lcurl

.PHONY: all clean
all: open_weather_data_link

DEPS = error_code.h
OBJ = error_code.o main.o

%.o: %.cpp $(DEPS)
	$(CXX) -c -o $@ $< $(CFLAGS)

open_weather_data_link: $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS) -ldslink-sdk-cpp-static $(LIBS)


run: open_weather_data_link
	./open_weather_data_link

clean:
	$(RM) open_weather_data_link $(OBJ)



#include "rapidjson/document.h"

#include <iostream>

using namespace rapidjson;
using namespace std;

int main(int argc, char* argv[])
{
    const char* json = "{\"coord\":{\"lon\":-0.13,\"lat\":51.51},\"weather\":[{\"id\":800,\"main\":\"Clear\",\"description\":\"clear sky\",\"icon\":\"01d\"}],\"base\":\"stations\",\"main\":{\"temp\":288.05,\"pressure\":1031,\"humidity\":55,\"temp_min\":284.15,\"temp_max\":291.15},\"visibility\":10000,\"wind\":{\"speed\":3.6,\"deg\":90},\"clouds\":{\"all\":0},\"dt\":1551201544,\"sys\":{\"type\":1,\"id\":1414,\"message\":0.0102,\"country\":\"GB\",\"sunrise\":1551163881,\"sunset\":1551202569},\"id\":2643743,\"name\":\"London\",\"cod\":200}";
    Document d;
    d.Parse(json);
    assert(d.HasMember("main")); 
    assert(d["main"].IsObject());
   for (auto& m : d["main"].GetObject()){
    printf("\n--------\nName of member %s ", m.name.GetString());
    
    if(m.value.IsString() ) 
        printf("\nValue of member %s ", m.value.GetString() );
    if(m.value.IsInt() ) 
        printf("\nValue of member %i ", m.value.GetInt() );

    if(m.value.IsDouble() ) 
        printf("\nValue of member %f ", m.value.GetDouble() );
    
    printf("\n");

   }
    
}

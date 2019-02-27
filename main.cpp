#include <efm_link.h>
#include <efm_link_options.h>
#include <efm_logging.h>
#include <curl/curl.h>
#include "error_code.h"
#include "rapidjson/document.h"

#include <iostream>
#include <random>
#include <sstream>

using namespace rapidjson;
using namespace cisco::efm_sdk;
using namespace std;

/*

{"coord":{"lon":-0.13,"lat":51.51},"weather":[{"id":800,"main":"Clear","description":"clear sky","icon":"01n"}],"base":"stations","main":{"temp":285.35,"pressure":1021,"humidity":58,"temp_min":282.59,"temp_max":288.15},"visibility":10000,"wind":{"speed":2.1,"deg":270},"clouds":{"all":0},"dt":1551291685,"sys":{"type":1,"id":1502,"message":0.0079,"country":"GB","sunrise":1551250147,"sunset":1551289080},"id":2643743,"name":"London","cod":200}


--------
Name of member temp 
Value of member 285.350000 

--------
Name of member pressure 
Value of member 1021 

--------
Name of member humidity 
Value of member 58 

--------
Name of member temp_min 
Value of member 282.590000 

--------
Name of member temp_max 
Value of member 288.150000 
[2019-02-27 10:45:07.824364] INFO SDK(1004): Unsubscribed from /text
[2019-02-27 10:45:10.187111] INFO SDK(1003): Subscribed to /text
[2019-02-27 10:45:15.106446] INFO SDK(1004): Unsubscribed from /text
{"coord":{"lon":-0.13,"lat":51.51},"weather":[{"id":800,"main":"Clear","description":"clear sky","icon":"01n"}],"base":"stations","main":{"temp":285.35,"pressure":1021,"humidity":58,"temp_min":282.59,"temp_max":288.15},"visibility":10000,"wind":{"speed":2.1,"deg":270},"clouds":{"all":0},"dt":1551291685,"sys":{"type":1,"id":1502,"message":0.0079,"country":"GB","sunrise":1551250147,"sunset":1551289080},"id":2643743,"name":"London","cod":200}{"coord":{"lon":-0.13,"lat":51.51},"weather":[{"id":800,"main":"Clear","description":"clear sky","icon":"01n"}],"base":"stations","main":{"temp":285.35,"pressure":1021,"humidity":58,"temp_min":282.59,"temp_max":288.15},"visibility":10000,"wind":{"speed":2.1,"deg":270},"clouds":{"all":0},"dt":1551291685,"sys":{"type":1,"id":1502,"message":0.0079,"country":"GB","sunrise":1551250147,"sunset":1551289080},"id":2643743,"name":"London","cod":200}


*/

static char errorBuffer[CURL_ERROR_SIZE];
static string buffer;

static int writer(char *data, size_t size, size_t nmemb, std::string *writerData){
  if(writerData == NULL) return 0;
  writerData->append(data, size*nmemb);
  return size * nmemb;
}

static bool initCURL(CURL *&conn, const string& url){
  CURLcode code;
  conn = curl_easy_init();
  


  if(conn == NULL) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to create CURL connection");
    exit(EXIT_FAILURE);
  }

  code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
  if(code != CURLE_OK) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to set error buffer");
    return false;
  }

  code = curl_easy_setopt(conn, CURLOPT_URL, url.c_str());
  if(code != CURLE_OK) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to set URL" );
    return false;
  }

  code = curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L);
  if(code != CURLE_OK) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to set redirect option ");
    return false;
  }

  code = curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, writer);
  if(code != CURLE_OK) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to set writer ");
    return false;
  }

  code = curl_easy_setopt(conn, CURLOPT_WRITEDATA, &buffer);
  if(code != CURLE_OK) {
    LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to set write data ");
    return false;
  }

  return true;
}


class OpenWeatherDataLink
{
public:
 
  OpenWeatherDataLink(Link& link) : link_(link) , responder_(link.responder()) { }

  void initialize(const std::string& link_name, const std::error_code& ec)
  {
    if (!ec) 
      LOG_EFM_DEBUG( "OpenWeatherDataLink", DebugLevel::l1, "Responder link '" << link_name << "' initialized");
    else 
      LOG_EFM_ERROR(ec, "could not initialize responder link");
    

    NodeBuilder builder{"/"};

    builder.make_node("sdk version")
      .display_name("SDK Version")
      .type(ValueType::String)
      .value(link_.get_version_info());

  

    builder.make_node("OpenWeatherData")
      .display_name("OpenWeatherData")
      .type(ValueType::String)
      .writable( Writable::Write, bind( &::OpenWeatherDataLink::set_text, this, placeholders::_1 ) )
      .on_subscribe( bind( &::OpenWeatherDataLink::on_subscribe_json, this, placeholders::_1 ) );

    builder.make_node("set_text")
      .display_name("Set Text")
      .action(Action( PermissionLevel::Read,
                bind( &OpenWeatherDataLink::set_text_called, this,
                 placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4
                ))
                .add_param(ActionParameter{"String", ValueType::String})
                .add_column({"Success", ValueType::Bool})
                .add_column({"Message", ValueType::String}));

    responder_.add_node( move(builder),
      bind(&OpenWeatherDataLink::nodes_created, this, placeholders::_1, placeholders::_2)
    );
  }


  void nodes_created(const vector<NodePath>& paths, const std::error_code& ec)
  {
    if (!ec) {
      LOG_EFM_DEBUG("OpenWeatherDataLink", DebugLevel::l1, "created nodes");
      for (const auto& path : paths) {
        LOG_EFM_DEBUG("OpenWeatherDataLink", DebugLevel::l2, "created path - " << path);
      }
    }
  }


  

  void getWeatherData() {
    if (!disconnected_) { //defensive programming, do nothing if not connected to EFM
      
      CURL *conn = NULL;
      CURLcode code;
      
      if(!initCURL(conn, "http://api.openweathermap.org/data/2.5/weather?q=London,uk&APPID=8fdc9a1f1fb74ac9dfed4803a57b02c6")) {
        LOG_EFM_ERROR(responder_error_code::curl_error, " curl initializion failed ");
        exit(EXIT_FAILURE);
      }

      code = curl_easy_perform(conn);
      curl_easy_cleanup(conn);

      if(code != CURLE_OK) {
        LOG_EFM_ERROR(responder_error_code::curl_error, "Failed to GET" << errorBuffer);
        exit(EXIT_FAILURE);
      }

      responder_.set_value(OWDPath, Variant{buffer}, std::chrono::system_clock::now(), [](const std::error_code&) {});
      Document d;
      cout<< buffer << "\n\n";
      d.Parse(buffer.c_str());
      buffer.clear();
      
      if( d["main"].IsObject() ){
        NodeBuilder builder{"/"};
        for (auto& m : d["main"].GetObject()){
               
          builder.make_node(m.name.GetString())
            .display_name(m.name.GetString());
            
                 
          printf("\n--------\nName of member %s ", m.name.GetString());
          
          if(m.value.IsString() ) {
            printf("\nValue of member %s ", m.value.GetString() );
            builder.type(ValueType::String)
                   .value(string(m.value.GetString()));
          }
              
          if(m.value.IsInt() ) {
              printf("\nValue of member %i ", m.value.GetInt() );
              builder.type(ValueType::Int)
                   .value(m.value.GetInt());
          }
             
          if(m.value.IsDouble() ) {
            printf("\nValue of member %f ", m.value.GetDouble() );
            builder.type(ValueType::Number)
                   .value(m.value.GetDouble());
          }
              
          printf("\n");

        }
        responder_.add_node( move(builder),
          bind(&OpenWeatherDataLink::nodes_created, this, placeholders::_1, placeholders::_2)
        );

      }
     

      link_.schedule_timed_task(std::chrono::seconds(60), [&]() { this->getWeatherData(); });
    }
  }

  void connected(const std::error_code& ec)
  {
    if (!ec) {
      disconnected_ = false;
      LOG_EFM_INFO(responder_error_code::connected);
      link_.schedule_timed_task(std::chrono::seconds(1), [&]() { this->getWeatherData(); });  
    }
  }


  void disconnected(const std::error_code& ec)
  {
    LOG_EFM_INFO(responder_error_code::disconnected, ec.message());
    disconnected_ = true;
  }

  void set_text(const Variant& value)
  {
    LOG_EFM_INFO(responder_error_code::set_text, value);
  }


  void set_text_called(
    const MutableActionResultStreamPtr& stream,
    const NodePath& parent_path,
    const Variant& params,
    const std::error_code& ec)
  {
    (void)parent_path;
    if (!ec) {
      LOG_EFM_DEBUG("OpenWeatherDataLink", DebugLevel::l3, "set_text_called");
      const auto* input = params.get("String");
      if (input) {
        auto text = *input;
        responder_.set_value(OWDPath, text, [stream, text](const std::error_code& ec) {
          if (!ec) {
            stream->set_result(UniqueActionResultPtr{new ActionValuesResult{
              ActionValuesResult(ActionSuccess).add_value(true).add_value(text)}});
          } else {
            stream->set_result(UniqueActionResultPtr{
              new ActionValuesResult{ActionValuesResult(ActionError)
                                                       .add_value(false)
                                                       .add_value("Could not set value")}});
          }
        });
        return;
      }
    }
    stream->set_result(
      UniqueActionResultPtr{ 
      new ActionValuesResult{ActionValuesResult(ActionError)
                                               .add_value(false)
                                               .add_value("Could not set value")}
      });
  }

 
  void on_subscribe_json(bool subscribe) {
    if (subscribe) 
      LOG_EFM_INFO(responder_error_code::subscribed_text);
    else 
      LOG_EFM_INFO(responder_error_code::unsubscribed_text);
  }

private:
  Link& link_;
  Responder& responder_;
  NodePath text_path_{"/text"};
  NodePath OWDPath{"/OpenWeatherData"};
  bool disconnected_{true};
};


int main(int argc, char* argv[])
{
  FileConfigLoader loader;
  LinkOptions options("OpenWeatherData-Link", loader);
  
  if (!options.parse(argc, argv, std::cerr)) return EXIT_FAILURE;
  
  curl_global_init(CURL_GLOBAL_DEFAULT);
  Link link(move(options), LinkType::Responder);
  LOG_EFM_INFO(::responder_error_code::build_with_version, link.get_version_info());

  OpenWeatherDataLink responder_link(link);

  link.set_on_initialized_handler( bind(&OpenWeatherDataLink::initialize, &responder_link, placeholders::_1, placeholders::_2 ) );
  link.set_on_connected_handler( bind(&OpenWeatherDataLink::connected, &responder_link, placeholders::_1 ) );
  link.set_on_disconnected_handler( bind(&OpenWeatherDataLink::disconnected, &responder_link, placeholders::_1 ) );

  link.run();

  return EXIT_SUCCESS;
}

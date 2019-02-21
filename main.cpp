// @copyright_start
// Copyright (c) 2019 Cisco and/or its affiliates. All rights reserved.
// @copyright_end

#include <efm_link.h>
#include <efm_link_options.h>
#include <efm_logging.h>
#include <curl/curl.h>
#include "error_code.h"

#include <iostream>
#include <random>
#include <sstream>

using namespace cisco::efm_sdk;
using namespace std;


static char errorBuffer[CURL_ERROR_SIZE];
static std::string buffer;

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

    builder.make_node("text")
      .display_name("String")
      .type(ValueType::String)
      .value("Hello, World!")
      .writable( Writable::Write, bind( &::OpenWeatherDataLink::set_text, this, placeholders::_1 ) )
      .on_subscribe( bind( &::OpenWeatherDataLink::on_subscribe_text, this, placeholders::_1 ) );

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

  void getWeatherData()
  {
    
    //responder_.set_value(OWDPath, Variant{val}, std::chrono::system_clock::now(), [](const std::error_code&) {});

    if (!disconnected_) {
      link_.schedule_timed_task(std::chrono::seconds(60), [&]() { this->getWeatherData(); });
    }
  }

  void connected(const std::error_code& ec)
  {
    if (!ec) {
      disconnected_ = false;
      LOG_EFM_INFO(responder_error_code::connected);
      link_.schedule_timed_task(std::chrono::seconds(60), [&]() { this->getWeatherData(); });
      responder_.set_value(text_path_, Variant{"Hello, World!"}, [](const std::error_code&) {});
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
        responder_.set_value(text_path_, text, [stream, text](const std::error_code& ec) {
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

 
  void on_subscribe_text(bool subscribe)
  {
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

static int writer(char *data, size_t size, size_t nmemb, std::string *writerData){
  if(writerData == NULL) return 0;
  writerData->append(data, size*nmemb);
  return size * nmemb;
}

static bool initCURL(CURL *&conn, char *url){
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

  code = curl_easy_setopt(conn, CURLOPT_URL, url);
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


int main(int argc, char* argv[])
{
  FileConfigLoader loader;
  LinkOptions options("OpenWeatherData-Link", loader);
  
  CURL *conn = NULL;
  CURLcode code;
  
  if (!options.parse(argc, argv, std::cerr)) 
    return EXIT_FAILURE;

  
  if(!initCURL(conn, "")) {
    LOG_EFM_ERROR(responder_error_code::curl_error, " curl initializion failed ");
    exit(EXIT_FAILURE);
  }
  
  Link link(move(options), LinkType::Responder);
  LOG_EFM_INFO(::responder_error_code::build_with_version, link.get_version_info());

  OpenWeatherDataLink responder_link(link);

  link.set_on_initialized_handler( bind(&OpenWeatherDataLink::initialize, &responder_link, placeholders::_1, placeholders::_2 ) );
  link.set_on_connected_handler( bind(&OpenWeatherDataLink::connected, &responder_link, placeholders::_1 ) );
  link.set_on_disconnected_handler( bind(&OpenWeatherDataLink::disconnected, &responder_link, placeholders::_1 ) );

  link.run();

  return EXIT_SUCCESS;
}

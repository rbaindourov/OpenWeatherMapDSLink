// @copyright_start
// Copyright (c) 2019 Cisco and/or its affiliates. All rights reserved.
// @copyright_end

#include <efm_link.h>
#include <efm_link_options.h>
#include <efm_logging.h>

#include "error_code.h"

#include <iostream>
#include <random>
#include <sstream>

using namespace cisco::efm_sdk;
/// @brief The simple responder link example demonstrates the EFM SDK API for responder implementations. Shows node,
/// action creation, and stream handling.
class SimpleResponderLink
{
public:
  /// Constructs the responder link implementation.
  /// @param link The link to work with.
  SimpleResponderLink(Link& link)
    : link_(link)
    , responder_(link.responder())
  {
  }

  /// The initialize callback that will be called as soon as the initialization including serialization is complete.
  /// Will create the first level node hierarchy. Only nodes not created by the deserialization will actually be
  /// created.
  /// @param link_name The name of the link.
  /// @param ec The error code will be set to an error if the initialization failed.
  void initialize(const std::string& link_name, const std::error_code& ec)
  {
    if (!ec) {
      LOG_EFM_DEBUG(
        "SimpleResponderLink", DebugLevel::l1, "Responder link '" << link_name << "' initialized");
    } else {
      LOG_EFM_ERROR(ec, "could not initialize responder link");
    }

    NodeBuilder builder{"/"};

    builder.make_node("sdk version")
      .display_name("SDK Version")
      .type(ValueType::String)
      .value(link_.get_version_info());
    builder.make_node("text")
      .display_name("String")
      .type(ValueType::String)
      .value("Hello, World!")
      .writable(
        Writable::Write, std::bind(&::SimpleResponderLink::set_text, this, std::placeholders::_1))
      .on_subscribe(std::bind(&::SimpleResponderLink::on_subscribe_text, this, std::placeholders::_1));

    builder.make_node("set_text")
      .display_name("Set Text")
      .action(Action(
                PermissionLevel::Read,
                std::bind(
                  &SimpleResponderLink::set_text_called,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2,
                  std::placeholders::_3,
                  std::placeholders::_4))
                .add_param(ActionParameter{"String", ValueType::String})
                .add_column({"Success", ValueType::Bool})
                .add_column({"Message", ValueType::String}));

    responder_.add_node(
      std::move(builder),
      std::bind(&SimpleResponderLink::nodes_created, this, std::placeholders::_1, std::placeholders::_2));
  }

  /// Callback that will be called upon construction of the first level nodes.
  /// @param paths The paths of the nodes that were actually created. A path that was added to the NodeBuilder is
  /// not part of the paths vector means that the node was already created. Normally, there is no need to check for
  /// the presence of a path. If the error code signals no error, just continue with your work.
  /// @param ec The error code will be set to an error if the node creation failed.
  void nodes_created(const std::vector<NodePath>& paths, const std::error_code& ec)
  {
    if (!ec) {
      LOG_EFM_DEBUG("SimpleResponderLink", DebugLevel::l1, "created nodes");
      for (const auto& path : paths) {
        LOG_EFM_DEBUG("SimpleResponderLink", DebugLevel::l2, "created path - " << path);
      }
    }
  }

  /// Called every time the link connects to the broker.
  /// Will set the value on the '/text' path.
  /// @param ec The error code will be set to an error if the connect failed.
  void connected(const std::error_code& ec)
  {
    if (!ec) {
      disconnected_ = false;
      LOG_EFM_INFO(responder_error_code::connected);

      responder_.set_value(text_path_, Variant{"Hello, World!"}, [](const std::error_code&) {});
    }
  }

  /// Called every time the link is disconnected from the broker.
  /// Will set a flag to signal the disconnected status.
  /// @param ec The error code will be set to an error if the disconnect failed.
  void disconnected(const std::error_code& ec)
  {
    LOG_EFM_INFO(responder_error_code::disconnected, ec.message());
    disconnected_ = true;
  }

  /// Will be called the node '/text' is set via an \@set action.
  /// @param value The value that was set.
  void set_text(const Variant& value)
  {
    LOG_EFM_INFO(responder_error_code::set_text, value);
  }

  /// Action callback for the '/set_text' action. Will set the value of the path '/text' to the given one. It will also
  /// echo back the set parameter.
  /// The stream will be closed automatically by the link.
  /// @param stream The stream to add a result to.
  /// @param parent_path The path of the node the action was called for.
  /// @param params The parameters set by the peer.
  /// @param ec The error code will be set to an error if the action failed.
  void set_text_called(
    const MutableActionResultStreamPtr& stream,
    const NodePath& parent_path,
    const Variant& params,
    const std::error_code& ec)
  {
    (void)parent_path;
    if (!ec) {
      LOG_EFM_DEBUG("SimpleResponderLink", DebugLevel::l3, "set_text_called");
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
    stream->set_result(UniqueActionResultPtr{
      new ActionValuesResult{ActionValuesResult(ActionError)
                                               .add_value(false)
                                               .add_value("Could not set value")}});
  }

  /// Will be called if a subscribe or unsubscribe is issued for the '/text' node.
  /// @param subscribe True if a subscribe was done or false if an unsubscribe was done.
  void on_subscribe_text(bool subscribe)
  {
    if (subscribe) {
      LOG_EFM_INFO(responder_error_code::subscribed_text);
    } else {
      LOG_EFM_INFO(responder_error_code::unsubscribed_text);
    }
  }

private:
  Link& link_;
  Responder& responder_;
  NodePath text_path_{"/text"};

  bool disconnected_{true};
};


int main(int argc, char* argv[])
{
  FileConfigLoader loader;
  LinkOptions options("Simple-Responder-Link", loader);
  if (!options.parse(argc, argv, std::cerr)) {
    return EXIT_FAILURE;
  }

  Link link(std::move(options), LinkType::Responder);
  LOG_EFM_INFO(::responder_error_code::build_with_version, link.get_version_info());

  SimpleResponderLink responder_link(link);

  link.set_on_initialized_handler( std::bind(&SimpleResponderLink::initialize, &responder_link, std::placeholders::_1, std::placeholders::_2 ) );
  link.set_on_connected_handler( std::bind(&SimpleResponderLink::connected, &responder_link, std::placeholders::_1 ) );
  link.set_on_disconnected_handler( std::bind(&SimpleResponderLink::disconnected, &responder_link, std::placeholders::_1 ) );

  link.run();

  return EXIT_SUCCESS;
}

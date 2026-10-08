#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// HTTP Server Configuration and Endpoint Declarations
// ----------------------------------------------------------------------------

// Function prototype declaration for sending standard HTTP responses
void sendResponse(int status, const String& message, const String& contentType = "text/plain");
// Function prototype declaration for setting up and registering all REST API server routes
void setupHttpServer();
// Function prototype declaration for handling root URL HTTP GET requests
void handleRoot();

#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// HTTP Server Configuration and Endpoint Declarations
// ----------------------------------------------------------------------------

// Sends a standard HTTP response with CORS headers and specified content type.
void sendResponse(int status, const String& message, const String& contentType = "text/plain");

// Initializes and registers all HTTP REST API endpoints and web server handlers.
void setupHttpServer();

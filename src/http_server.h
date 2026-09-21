#pragma once
#include "config.h"

// ----------------------------------------------------------------------------
// HTTP Server Configuration and Endpoint Declarations
// ----------------------------------------------------------------------------

// Sends a standard HTTP response with CORS headers and specified content type.
void sendResponse(int status, const String& message, const String& contentType = "text/plain");

// Transmits a direct single-byte Glance command and sends an HTTP success response.
void sendQuickCmd(uint8_t cmdByte, const char* label);

// Initializes and registers all HTTP REST API endpoints and web server handlers.
void setupHttpServer();

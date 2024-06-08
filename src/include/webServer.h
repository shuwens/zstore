// https://github.com/Quado-The-Delivery-Robot/robot-ai/blob/7e126460a4fcf7cff1df91ea9eda64db08b335d7/webServer.h

#pragma once

#ifndef HEADER_CONTROLLERS_SOFTWARE_WEBSERVER
#define HEADER_CONTROLLERS_SOFTWARE_WEBSERVER

#include <CivetServer.h>

struct mg_context* startWebServer();

#endif

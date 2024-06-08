#include <iostream>
#include <string>
#include <CivetServer.h>
// #include <nlohmann/json.hpp>
#include "../include/startRouteRequestHandler.h"

using namespace std;
using json = nlohmann::json;

const char* okMessage = "OK";
const char* notOkMessage = "NOT OK";

int startRouteRequestHandler(struct mg_connection* connection, void* cbdata) {
    const struct mg_request_info* req_info = mg_get_request_info(connection);

    // Check if the request is a POST request.
    if (req_info->request_method && strcmp(req_info->request_method, "POST") == 0) {
        // Read the request body.
        char buffer[1024];
        int content_length = mg_read(connection, buffer, sizeof(buffer));

        if (content_length > 0) {
            buffer[content_length] = '\0';
            std::string requestBody = buffer;

            // Parse to JSON.
            try {
                json requestJson = json::parse(requestBody);

                cout << requestJson << endl;

                // Return that the request was OK.
                unsigned long len = (unsigned long)strlen(okMessage);
                mg_send_http_ok(connection, "application/json", len);
                mg_write(connection, okMessage, len);
                return 200;
            }
            catch (const json::exception& parseError) {
                cout << "Failed to parse JSON from /start route request.\n" << parseError.what() << endl;
            }
        }
    }

    mg_send_http_error(connection, 400, notOkMessage);
    return 400;
}

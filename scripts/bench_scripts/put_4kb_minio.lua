-- Fixed-size 4KB payload
local payload = string.rep("A", 4096 * 1024)

wrk.method = "PUT"
-- wrk.headers["Content-Type"] = "application/octet-stream"
wrk.body = payload

-- Each request gets a unique object name (optional)
local counter = 0
request = function()
   counter = counter + 1
   local path = "/public/obj-" .. counter
   print("Request: " .. path)
   return wrk.format(nil, path)
end

response = function(status, headers, body)
   print("Response: " .. status)
   -- if responses[status] == nil then
   --    responses[status] = 1
   -- else
   --    responses[status] = responses[status] + 1
   -- end
end

-- Script that reads secrets from k/v engine in Vault
-- To indicate the number of secrets you want to read, add "-- <N>" after the URL
-- If you want to print secrets read, add "-- <N> true" after the URL

json = require "json"

local counter = 1
local threads = {}

wrk.headers["Content-Type"] = "application/octet-stream"

function setup(thread)
   thread:set("id", counter)
   table.insert(threads, thread)
   counter = counter + 1
end

function init(args)
   if args[1] == nil then
      num_secrets = 1000
   else
      num_secrets = tonumber(args[1])
   end
   -- print("Number of secrets is: " .. num_secrets)
   if args[2] == nil then
      print_secrets = "false"
   else
      print_secrets = args[2]
   end
   requests  = 0
   reads     = 0
   responses = {}
   method    = "GET"
   body      = ''
   -- give each thread different random seed
   math.randomseed(os.time() + id * 1000)
   -- local msg = "thread %d created with print_secrets set to %s"
   -- print(msg:format(id, print_secrets))
end

request = function()
   reads = reads + 1
   -- randomize path to secret
   path = "/public/obj-" .. math.random(num_secrets)
   requests = requests + 1
   print("Request: " .. path)
   return wrk.format(method, path,
      nil, body)
end

response = function(status, headers, body)
   print("Response: " .. status)
   if responses[status] == nil then
      responses[status] = 1
   else
      responses[status] = responses[status] + 1
   end
end

function done(summary, latency, requests)
   print("\nStatus codes:")
   for code, count in pairs(responses) do
      print(string.format("  %s : %d", code, count))
   end
end

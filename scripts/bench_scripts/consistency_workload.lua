-- Lua script for wrk to perform PUT and GET requests based on a workload file

local json = require "json"

local workload = {}
local threads = {}
local counter = 1

function setup(thread)
   thread:set("id", counter)
   table.insert(threads, thread)
   counter = counter + 1
end

function init(args)
   if not args[1] then
      error("Please provide the workload file: wrk -s wrk_put_get.lua http://host -- workload.txt")
   end

   workload_file = args[1]
   print_bodies = args[2] == "true"

   -- Load the workload from the file
   local file = io.open(workload_file, "r")
   if not file then
      error("Failed to open workload file: " .. workload_file)
   end

   for line in file:lines() do
      local method, full_url, size = line:match("(%S+)%s+(%S+)%s*(%d*)")
      if method and full_url then
         local parsed = {}
         parsed.method = method
         parsed.url = full_url
         parsed.size = tonumber(size) or 0
         table.insert(workload, parsed)
      end
   end
   file:close()

   index     = 1
   requests  = 0
   reads     = 0
   writes    = 0
   responses = 0
end

function request()
   if index > #workload then
      index = 1
   end

   local req = workload[index]
   index = index + 1

   local method = req.method
   local url = req.url
   local body = nil

   if method == "PUT" then
      body = string.rep("x", req.size)
      wrk.headers["Content-Type"] = "application/octet-stream"
      writes = writes + 1
   else
      reads = reads + 1
   end

   requests = requests + 1

   -- wrk.format() accepts full URLs if we override host
   return wrk.format(method, url, nil, body)
end

function response(status, headers, body)
   responses = responses + 1
   if print_bodies and body and #body > 0 then
      print("Response [" .. status .. "]: " .. body)
   end
end

function done(summary, latency, requests)
   for index, thread in ipairs(threads) do
      local id = tonumber(thread:get("id"))
      local msg = "Thread %d made %d requests (%d PUTs, %d GETs) and received %d responses"
      print(msg:format(id, tonumber(requests.total), writes, reads, responses))
   end
end

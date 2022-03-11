local kcp = require "kcp"

local context = kcp.create(1)
print (context)

context:set_output(function (data)
    print ("callback1 ".. data)
end)

context:set_output(function (data)
    print ("callback2 ".. data)
end)
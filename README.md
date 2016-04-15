# lua-libuv

[![Build Status](https://travis-ci.org/zwh8800/lua-libuv.png?branch=master)](https://travis-ci.org/zwh8800/lua-libuv)

使lua支持类似nodejs的异步编程，基于lua5.3.1和libuv

---

## 例子
```lua
local server = uv.createServer()
print(server)
local count = 1
server:listen('0.0.0.0', 8080, 
    function (socket, err) 
        if socket then
            print(count .. ': ')
            count = count + 1
            print(socket)
            print('host socket ip&port: ', socket:getsockname())
            print('remote socket ip&port: ', socket:getpeername())
            
            socket:onData(
                function(data)
                    print('received: ' .. data)
                    socket:write('HTTP/1.1 200 OK\r\n')
                    socket:write('Content-Type: text/plain\r\n')
                    socket:write('\r\n')
                    socket:write('Hello')
                    socket:close()
                    
                end
            )
            socket:onEnd(
                function()
                    print('remote closed')
                end
            )
            socket:onError(
                function(errCode, err)
                    print('error: ' .. errCode .. err)
                end
            )
        else
            print('got nil' .. err)
        end
        
    end
)

uv.loop()
```

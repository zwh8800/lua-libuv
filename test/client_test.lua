function main()
    local socket
    socket = uv.createConnection('127.0.0.1', 8080, 
        function ()
            print('connected')
            print(socket)
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
        end
    )
    print(socket)
    socket:onError(
        function(errCode, err)
            print('error: ' .. '[' .. errCode .. ']' .. err)
        end
    )
    
    uv.loop()
end

main()

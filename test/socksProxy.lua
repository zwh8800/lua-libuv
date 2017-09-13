dofile 'print_r.lua'

SocksVersion = 0x05

Errors = {
    VersionError = 'version error',
    CommandTypeNotSupported = 'command type not supported',
    AddressTypeNotSupported = 'address type not supported',
}

MethodType = {
    NoAuth = 0x00,
    GSSAPI = 0x01,
    UsernamePassword = 0x02,
    IANAssigned = 0x03,
    Private = 0x80,
    NoAcceptable = 0xff,
}

function parseMethodPayload(payload)
    if payload:byte(1) ~= SocksVersion then
        return nil, Errors.VersionError
    end

    local method = {
        version = SocksVersion,
        methods = {},
    }

    local methodCount = payload:byte(2)
    method.methods = {payload:byte(3, 3 + methodCount - 1)}
    return method
end

function selectMethodToString(selectMethodPayload)
    return string.char(selectMethodPayload.version, selectMethodPayload.selectedMethod)
end

function newSelectMethodPayload()
    return {
        version = SocksVersion,
        selectedMethod = MethodType.NoAuth,
        toString = selectMethodToString,
    }
end

CommandType = {
    Connect = 0x01,
    Bind = 0x02,
    Udp = 0x03,
}

AddressType = {
    IPv4 = 0x01,
    DomainName = 0x03,
    IPv6 = 0x04,
}

function parseRequestPayload(payload)
    if payload:byte(1) ~= SocksVersion then
        return nil, Errors.VersionError
    end

    local request = {
        version = SocksVersion,
        command = CommandType.Connect,
        addressType = AddressType.IPv4,
        distAddress = '',
        distPort = 0,
    }

    if payload:byte(2) > CommandType.Udp then
        return nil, Errors.CommandTypeNotSupported
    else
        request.command = payload:byte(2)
    end

    local requestAddressType = payload:byte(4)
    if  requestAddressType ~= AddressType.IPv4 and
        requestAddressType ~= AddressType.DomainName and
        requestAddressType ~= AddressType.IPv6
    then
        return nil, Errors.AddressTypeNotSupported
    else
        request.addressType = requestAddressType
    end

    local portIndex
    if request.addressType == AddressType.IPv4 then
        local ipBytes = {payload:byte(5, 8)}
        request.distAddress = table.concat(ipBytes, '.')
        portIndex = 9
    elseif request.addressType == AddressType.DomainName then
        local len = payload:byte(5)
        request.distAddress = payload:sub(6, 6 + len - 1)
        portIndex = 5 + len + 1
    elseif request.addressType == AddressType.IPv6 then
        return nil, Errors.AddressTypeNotSupported
    end

    local portBytes = {payload:byte(portIndex, portIndex + 1) }
    request.distPort = portBytes[1] * 256 + portBytes[2]

    return request, nil
end

ReplyType = {
    Success = 0x00,
    GeneralSocksServerFailure = 0x01,
    ConnectionNotAllowed = 0x02,
    NetworkUnreachable = 0x03,
    HostUnreachable = 0x04,
    ConnectionRefused = 0x05,
    TTLExpire = 0x06,
    CommandNotSupported = 0x07,
    AddressTypeNotSupported = 0x08,
}

function replyToString(replyPayload)
    return string.char(
        replyPayload.version,
        replyPayload.reply,
        replyPayload.reserved,
        replyPayload.addressType
    ) ..
        replyPayload.bindAddress ..
        string.char(math.floor(replyPayload.bindPort / 256)) ..
        string.char(math.floor(replyPayload.bindPort % 256))
end

function newReplyPayload()
    return {
        version = SocksVersion,
        reply = ReplyType.Success,
        reserved = 0x00,
        addressType = AddressType.DomainName,
        bindAddress = '',
        bindPort = 0,

        toString = replyToString,
    }
end


ClientState = {
    Start = 0,
    MethodSelected = 1,
    RequestHandled = 2,
}

function handleConnection(socket)
    print(socket)
    print('host socket ip&port: ', socket:getsockname())
    print('remote socket ip&port: ', socket:getpeername())

    local state = ClientState.Start
    local distSocket
    socket:onData(
        function(data)
            print('recv: ', data:byte(1, -1))
            if state == ClientState.Start then
                local clientMethods, err = parseMethodPayload(data)
                if err ~= nil then
                    print('error:', err)
                    socket:close()
                    return
                end
                -- print_r(clientMethods)

                local methodOk = false
                for _, v in pairs(clientMethods.methods) do
                    if v == MethodType.NoAuth then
                        methodOk = true
                        break
                    end
                end
                local selectMethod = newSelectMethodPayload()

                if methodOk then
                    -- print_r(selectMethod)
                    print('send: ', selectMethod:toString():byte(1, -1))
                    socket:write(selectMethod:toString())
                    state = ClientState.MethodSelected
                else
                    selectMethod.selectedMethod = MethodType.NoAcceptable
                    -- print_r(selectMethod)
                    print('send: ', selectMethod:toString():byte(1, -1))
                    socket:write(selectMethod:toString())
                    socket:close()
                    return
                end
            elseif state == ClientState.MethodSelected then
                local reply = newReplyPayload()

                local request, err = parseRequestPayload(data)
                if err ~= nil then
                    print('error:', err)
                    if err == Errors.CommandTypeNotSupported then
                        reply.reply = ReplyType.CommandNotSupported

                        print('send: ', reply:toString():byte(1, -1))
                        socket:write(reply:toString())
                    elseif err == Errors.AddressTypeNotSupported then
                        reply.reply = ReplyType.AddressTypeNotSupported

                        print('send: ', reply:toString():byte(1, -1))
                        socket:write(reply:toString())
                    end
                    socket:close()
                    return
                end
                print_r(request)

                if request.command ~= CommandType.Connect then
                    reply.reply = ReplyType.CommandTypeNotSupported

                    print('send: ', reply:toString():byte(1, -1))
                    socket:write(reply:toString())
                    socket:close()
                    return
                end

                if request.addressType == AddressType.IPv4 then

                elseif request.addressType == AddressType.DomainName then
                    uv.lookup(request.distAddress,
                        function (address, err)
                            if address == nil then
                                print('error: ', err)

                                reply.reply = ReplyType.NetworkUnreachable

                                print('send: ', reply:toString():byte(1, -1))
                                socket:write(reply:toString())
                                socket:close()
                                return
                            end

                            distSocket = uv.createConnection(address, request.distPort,
                                function ()
                                    print('connected')
                                    print(distSocket)
                                    local addr, port = distSocket:getsockname()
                                    reply.addressType = AddressType.IPv4

                                    local bindAddressTab = {}
                                    for elem in addr:gmatch("%d+") do
                                        bindAddressTab[#bindAddressTab + 1] = string.char(assert(tonumber(elem)))
                                    end
                                    reply.bindAddress = table.concat(bindAddressTab)

                                    reply.bindPort = port
                                    print_r(reply)
                                    print('send: ', reply:toString():byte(1, -1))
                                    socket:write(reply:toString())

                                    state = ClientState.RequestHandled

                                    distSocket:onData(
                                        function(data)
                                            socket:write(data)
                                        end
                                    )
                                    distSocket:onEnd(
                                        function()
                                            socket:close()
                                        end
                                    )
                                end
                            )

                        end
                    )
                end


            elseif state == ClientState.RequestHandled then
                distSocket:write(data)
            end
        end
    )
    socket:onEnd(
        function()
            print('remote closed')
            if distSocket ~= nil then
                distSocket:close()
            end

            socket:close()
        end
    )
    socket:onError(
        function(errCode, err)
            print('error: ' .. errCode .. err)
        end
    )
end


function main()
    local server = uv.createServer()
    print(server)
    server:listen('0.0.0.0', 1081,
        function (socket, err)
            if socket then
                handleConnection(socket)
            else
                print('got nil' .. err)
            end

        end
    )

    uv.loop()
end

main()

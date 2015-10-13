lua-libuv
=====================

使lua支持类似nodejs的异步编程，基于lua5.3.1和libuv

目前在windows上开发和测试（Visual studio Community 2013），应该很方便能移植到Linux上，下一步会提供Linux上的Makefile

### 例子
	local server = uv.createServer()
	print(server)
	local count = 1
	server:listen('0.0.0.0', 8080, 
		function (socket, err) 
			if socket then
				print(count .. ': ')
				count = count + 1
				print(socket)
				socket:write('hello lua-uv\n')
				
				socket:onData(
					function(socket, nread, data)
						if (nread >= 0) then
							print('received: ' .. data)
							if data == 'exit\n' then
								socket:finish('bye')
							end
						else
							print('error: ' .. socket .. ': ' .. data)
						end
						
					end
				)

			else
				print('got nil' .. err)
			end
			
		end
	)
	
	uv.loop()

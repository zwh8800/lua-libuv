function main()
	math.randomseed(os.time())
	
	for i = 1, 10 do 
		local r = math.random()
		if r % 2 == 0 then
			uv.test(i .. ': ' .. r .. '(even)')
		else
			uv.test(i .. ': ' .. r .. '(odd)')
		end
	end
	
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
				
				-- socket:close()
				-- socket:finish('hello lua-uv\n')
				
			else
				print('got nil' .. err)
			end
			
		end
	)
	
	uv.loop()
	
end

main()

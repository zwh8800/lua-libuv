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
	server:listen('0.0.0.0', 8080, function (client, err) 
			if client then
				uv.test(client)
			else
				uv.test('got nil' .. err)
			end
			
		end
	)
	
	uv.loop()
	
end

main()

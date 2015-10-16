function main()
	uv.resolve4('baidu.com', 
		function (addresses, err)
			if addresses == nil then
				print('error', err)
				return
			end
			print(addresses)
			print('[')
			for i, v in ipairs(addresses) do 
				print(v)
			end
			print(']')
			
		end
	)
	uv.resolve4('www.google.com', 
		function (addresses, err)
			if addresses == nil then
				print('error', err)
				return
			end
			print(addresses)
			print('[')
			for i, v in ipairs(addresses) do 
				print(v)
			end
			print(']')
			
		end
	)
	
	uv.resolve4('baidgdsgsdfgu.com', 
		function (addresses, err)
			if addresses == nil then
				print('error', err)
				return
			end
			print(addresses)
			print('[')
			for i, v in ipairs(addresses) do 
				print(v)
			end
			print(']')
			
		end
	)
	
	uv.lookup('baidu.com', 
		function (address, err)
			if address == nil then
				print('error', err)
				return
			end
			print(address)
			
		end
	)
	
	uv.lookup('baidsafdsafu.com', 
		function (address, err)
			if address == nil then
				print('error', err)
				return
			end
			print(address)
			
		end
	)
	
	uv.lookup('192.168.1.1', 
		function (address, err)
			if address == nil then
				print('error', err)
				return
			end
			print(address)
			
		end
	)
	
	uv.loop()
end

main()

var dns = require('dns');

dns.resolve4('baidu.com', function (err, addresses) {
  if (err) 
      throw err;

  console.log('addresses1: ' + JSON.stringify(addresses));

});

dns.lookup('baidu.com', function onLookup(err, addresses, family) {
      if (err) 
      throw err;
  console.log('addresses2:', addresses);
});

dns.resolve('baidu.com', 'AAAA', function (err, addresses) {
      if (err) 
      throw err;
  console.log('addresses3: ' + JSON.stringify(addresses));
});
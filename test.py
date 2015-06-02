import urllib2

theurl = "https://api.github.com/orgs/ubtue/events"
username = 'ruschein'
password = 'Gelz4+_k)k'

passman = urllib2.HTTPPasswordMgrWithDefaultRealm()
passman.add_password(None, theurl, username, password)

authhandler = urllib2.HTTPBasicAuthHandler(passman)
opener = urllib2.build_opener(authhandler)
urllib2.install_opener(opener)

response = urllib2.urlopen("https://api.github.com/orgs/ubtue/events")

print response.info()

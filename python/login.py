import urllib2
import cookielib
filename='wangxu.baidu.cookie'

'''
cookie = cookielib.MozillaCookieJar(filename)
#cookie = cookielib.CookieJar()
handler=urllib2.HTTPCookieProcessor(cookie)
opener = urllib2.build_opener(handler)
response = opener.open('http://www.baidu.com')
cookie.save(ignore_discard=True, ignore_expires=True)
for item in cookie:
    print 'Name = '+item.name
    print 'Value = '+item.value
'''

print 'hellow world'
cookie = cookielib.MozillaCookieJar()
cookie.load(filename, ignore_discard=True, ignore_expires=True)
req = urllib2.Request("http://www.baidu.com")
opener = urllib2.build_opener(urllib2.HTTPCookieProcessor(cookie))
response = opener.open(req)
print response.read()

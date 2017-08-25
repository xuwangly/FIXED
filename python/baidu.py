#import urllib2
#f = urllib2.urlopen("https://www.baidu.com/")
#buf = f.read()
#print buf
#f.close()

headers = {'Accept':'text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8',  
        'Accept-Encoding':'gzip,deflate,sdch',  
        'Accept-Language':'en,zh-CN;q=0.8,zh;q=0.6',  
        'Cache-Control':'max-age=0',  
        'Connection':'keep-alive',  
        'Content-Type':'application/x-www-form-urlencoded',  
        'User-Agent':'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/34.0.1847.131 Safari/537.36'  
    } #构建请求文件头，这里模仿了Chrome 34.0.1847.131 浏览器  
  
def login(username, password):  
    #登陆界面RUL  
      
    url = 'yourURL'  
      
    #构造登陆post表单  
    postData = 'yourPostData'  
    #记录cookies并下载到本地  
    cj = cookielib.CookieJar()  
    opener = urllib2.build_opener(urllib2.HTTPCookieProcessor(cj))  
    urllib2.install_opener(opener)  
      
    #下载登陆界面cookies  
    testReq = Request(url, headers = headers)  
    urlopen(testReq)  
  
    #提交表单，登陆  
    req = Request(url, postData, headers)  
    res = urlopen(req)  
  
    return opener 

mainPageURL = 'url'  
    req = Request(mainPageURL, headers = headers)  
    res = urlopen(req)  
    print res.read() 

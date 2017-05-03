http://blog.csdn.net/yinrm/article/details/47784269
http://sychen.logdown.com/posts/2014/12/28/delete-project-in-gerrit

#安装ant用来编译buck
sudo apt-get install ant
#克隆buck然后使用ant编译
git clone https://gerrit.googlesource.com/buck
cd buck/
ant
#编译好了buck配制buck的环境变量
ln -s `pwd`/bin/buck ~/bin/
ln -s `pwd`/bin/buckd ~/bin/
#克隆gerrit源码
git clone https://gerrit.googlesource.com/gerrit
cd gerrit/
#编译到自己使用的gerrit版本
git checkout v2.9.4 
#buck build gerrit 编译gerrit.jar
buck build gerrit
cd plugins/
rm -rf download-commands
#克隆plugin的dowmload-commands,并切换到使用的gerrit版本
git clone https://gerrit.googlesource.com/plugins/download-commands
cd download-commands
git checkout v2.9.4
cd ../..
buck build plugins/download-commands:download-commands 
#生成download-commands.jar的路径/gerrit/buck-out/gen/plugins/download-commands/
cd /gerrit/buck-out/gen/plugins/download-commands/
ssh -p 29418 sduadmin@10.4.64.226 gerrit plugin install -n download-commands -<download-commands.jar
ssh -p 29418 sduadmin@10.4.64.226 gerrit plugin ls
